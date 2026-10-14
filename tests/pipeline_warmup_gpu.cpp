#include <SDL3/SDL.h>
#include <aurora/aurora.h>
#include <aurora/gfx.hpp>
#include <aurora/pipeline_cache.h>

#include "gfx/clear.hpp"
#include "gfx/frame_packet.hpp"
#include "gfx/pipeline_cache.hpp"
#include "gx/pipeline.hpp"
#include "webgpu/gpu.hpp"

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace {
unsigned checks = 0;
void Check(bool condition, const char* message) {
  ++checks;
  if (!condition) throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {reinterpret_cast<const char*>(text.data()), text.size()};
}
struct Session {
  bool live = false;
  ~Session() { if (live) aurora_shutdown(); }
  void Stop() {
    aurora_shutdown(); live = false;
    const auto stopped = aurora_pipeline_warmup_status();
    Check(!stopped.ready && stopped.pendingPipelines == 0 && stopped.compilingPipelines == 0 &&
          stopped.totalPipelines == 0 && stopped.knownConfigs == 0,
          "Shutdown retained pipeline state");
  }
};
void Start(Session& session, int argc, char** argv, AuroraBackend backend,
           const std::filesystem::path& directory, const std::filesystem::path& resources, unsigned msaa) {
  std::filesystem::create_directories(directory);
  const auto path = Utf8(directory), resourcePath = Utf8(resources);
  AuroraConfig config{};
  config.appName = "Charged pipeline warmup test";
  config.userPath = config.cachePath = path.c_str();
  config.resourcesPath = resourcePath.c_str();
  config.desiredBackend = backend;
  config.windowWidth = 320; config.windowHeight = 240;
  config.windowPosX = config.windowPosY = -1;
  config.msaa = msaa;
  config.vsync = true;
  config.logLevel = LOG_WARNING;
  config.enableBackendValidation = true;
  const auto host = aurora_initialize(argc, argv, &config);
  session.live = true;
  Check(host.window && host.backend == backend, "Requested GPU backend unavailable");
  Check(aurora::webgpu::g_adapterInfo.adapterType != wgpu::AdapterType::CPU,
        "CPU adapter cannot qualify GPU pipeline preparation");
}
AuroraPipelineWarmupStatus WaitReady(std::chrono::seconds timeout = std::chrono::seconds(90)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    const auto status = aurora_pipeline_warmup_status();
    Check(status.failedPipelines == 0, "A cached pipeline failed to compile");
    if (status.ready) {
      Check(status.pendingPipelines == 0 && status.compilingPipelines == 0 &&
            status.totalPipelines == status.readyPipelines,
            "Readiness preceded final pipeline publication/maintenance");
      return status;
    }
    Check(std::chrono::steady_clock::now() < deadline, "Timed out preparing pipelines");
    SDL_PumpEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
std::array<aurora::gfx::clear::PipelineConfig, 8> Configs() {
  std::array<aurora::gfx::clear::PipelineConfig, 8> result{};
  for (unsigned i = 0; i < result.size(); ++i) {
    result[i].clearColor = (i & 1) != 0;
    result[i].clearAlpha = (i & 2) != 0;
    result[i].clearDepth = (i & 4) != 0;
  }
  return result;
}
void RequestKnown(bool bothLayouts) {
  const auto scene = aurora::gfx::scene_render_target_layout();
  auto offscreen = scene;
  offscreen.sampleCount = 1;
  offscreen.colorAttachmentCount = 1;
  aurora::gfx::detail::finalize_render_target_layout(offscreen);
  for (const auto& config : Configs()) {
    const auto ref = aurora::gfx::find_pipeline(config, scene);
    aurora::gfx::CompiledPipeline pipeline;
    Check(aurora::gfx::get_pipeline(ref, pipeline), "Known scene pipeline unavailable");
    if (bothLayouts && offscreen.key != scene.key) {
      const auto offscreenRef = aurora::gfx::find_pipeline(config, offscreen);
      Check(aurora::gfx::get_pipeline(offscreenRef, pipeline), "Known offscreen pipeline unavailable");
    }
  }
}
void VerifyHits(unsigned expectedRequests) {
  const auto before = WaitReady();
  RequestKnown(true);
  const auto after = WaitReady();
  Check(after.demandHits - before.demandHits == expectedRequests,
        "Prepared configurations did not produce demand cache hits");
  Check(after.demandMisses == before.demandMisses &&
        after.demandWaitNanoseconds == before.demandWaitNanoseconds,
        "First post-warmup requests incurred a pipeline miss/wait");
  Check(after.totalPipelines == before.totalPipelines, "Demand rebuilt a prepared pipeline");
  std::printf("Prepared requests: %u hits, 0 misses, 0 ns demand wait; ready=%llu.\n",
              expectedRequests, static_cast<unsigned long long>(after.readyPipelines));
}

using CatalogueConfig = std::variant<aurora::gfx::clear::PipelineConfig, aurora::gx::PipelineConfig>;
struct Catalogue {
  std::vector<CatalogueConfig> configs;
  unsigned clearCount = 0;
  unsigned gxCount = 0;
};
struct Database {
  sqlite3* handle = nullptr;
  sqlite3_stmt* statement = nullptr;
  ~Database() {
    if (statement) sqlite3_finalize(statement);
    if (handle) sqlite3_close(handle);
  }
};

template <typename Config>
Config DecodeConfig(const void* blob, int bytes, uint32_t version, int64_t storedHash,
                    aurora::gfx::ShaderType type) {
  static_assert(std::has_unique_object_representations_v<Config>);
  Check(bytes >= 0 && static_cast<size_t>(bytes) == sizeof(Config) && blob != nullptr, "Catalogue config does not match the target ABI size");
  Config config;
  std::memcpy(&config, blob, sizeof(config));
  Check(config.version == version, "Catalogue blob has an incompatible config version");
  const auto hash = aurora::xxh3_hash(config, static_cast<aurora::HashType>(type));
  Check(static_cast<uint64_t>(storedHash) == static_cast<uint64_t>(hash),
        "Stored catalogue hash differs from the native Aurora XXH3(config, type)");
  return config;
}

Catalogue ReadCatalogue(const std::filesystem::path& path) {
  Database db;
  const auto text = Utf8(path);
  Check(sqlite3_open_v2(text.c_str(), &db.handle, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK,
        "Cannot open catalogue read-only");
  Check(sqlite3_prepare_v2(db.handle, "SELECT value FROM aurora_schema", -1, &db.statement, nullptr) == SQLITE_OK,
        "Catalogue schema table missing");
  Check(sqlite3_step(db.statement) == SQLITE_ROW && sqlite3_column_int(db.statement, 0) == 1,
        "Catalogue SQLite schema version is incompatible");
  Check(sqlite3_step(db.statement) == SQLITE_DONE, "Catalogue has multiple schema versions");
  sqlite3_finalize(db.statement); db.statement = nullptr;
  Check(sqlite3_prepare_v2(db.handle,
        "SELECT type, hash, config_version, config_size, config, first_frame_used "
        "FROM pipeline_cache ORDER BY first_frame_used, type, hash", -1, &db.statement, nullptr) == SQLITE_OK,
        "Cannot read catalogue configurations");
  Catalogue catalogue;
  int result = SQLITE_OK;
  while ((result = sqlite3_step(db.statement)) == SQLITE_ROW) {
    const auto type = sqlite3_column_int(db.statement, 0);
    const auto hash = sqlite3_column_int64(db.statement, 1);
    const auto version = sqlite3_column_int64(db.statement, 2);
    const auto storedSize = sqlite3_column_int64(db.statement, 3);
    const auto* blob = sqlite3_column_blob(db.statement, 4);
    const auto bytes = sqlite3_column_bytes(db.statement, 4);
    const auto firstFrame = sqlite3_column_int64(db.statement, 5);
    Check(storedSize == bytes && firstFrame >= 0 && firstFrame <= UINT32_MAX,
          "Catalogue row metadata is invalid");
    if (type == static_cast<int>(aurora::gfx::ShaderType::Clear)) {
      Check(version == aurora::gfx::clear::ClearPipelineConfigVersion, "Catalogue clear config version is incompatible");
      catalogue.configs.emplace_back(DecodeConfig<aurora::gfx::clear::PipelineConfig>(
          blob, bytes, static_cast<uint32_t>(version), hash, aurora::gfx::ShaderType::Clear));
      ++catalogue.clearCount;
    } else if (type == static_cast<int>(aurora::gfx::ShaderType::GX)) {
      Check(version == aurora::gx::GXPipelineConfigVersion, "Catalogue GX config version is incompatible");
      catalogue.configs.emplace_back(DecodeConfig<aurora::gx::PipelineConfig>(
          blob, bytes, static_cast<uint32_t>(version), hash, aurora::gfx::ShaderType::GX));
      ++catalogue.gxCount;
    } else {
      throw std::runtime_error("Catalogue contains unsupported shader type");
    }
  }
  Check(result == SQLITE_DONE && !catalogue.configs.empty(), "Catalogue read failed or contains no configs");
  std::printf("Native catalogue hashes verified: clear=%u (ABI %zu), GX=%u (ABI %zu).\n",
              catalogue.clearCount, sizeof(aurora::gfx::clear::PipelineConfig),
              catalogue.gxCount, sizeof(aurora::gx::PipelineConfig));
  return catalogue;
}

void VerifyCatalogue(int argc, char** argv, AuroraBackend backend, unsigned msaa,
                     const std::filesystem::path& input, const std::filesystem::path& directory,
                     const std::filesystem::path& resources) {
  // Validate through this target's real C++ layouts and hash implementation,
  // independent of the portable catalogue generator. Input is never writable.
  const auto catalogue = ReadCatalogue(input);
  std::filesystem::copy_file(input, resources / "initial_pipeline_cache.db");
  Session session;
  const auto started = std::chrono::steady_clock::now();
  Start(session, argc, argv, backend, directory / "catalogue-cache", resources, msaa);
  const auto initialized = std::chrono::steady_clock::now();
  aurora_prepare_pipelines();
  const auto prepared = WaitReady(std::chrono::seconds(600));
  const auto readyAt = std::chrono::steady_clock::now();
  const auto scene = aurora::gfx::scene_render_target_layout();
  auto offscreen = scene;
  offscreen.sampleCount = 1;
  offscreen.colorAttachmentCount = 1;
  aurora::gfx::detail::finalize_render_target_layout(offscreen);
  const auto layoutCount = offscreen.key == scene.key ? 1u : 2u;
  const auto requests = catalogue.configs.size() * layoutCount;
  Check(prepared.knownConfigs == catalogue.configs.size() && prepared.readyPipelines == requests,
        "Full catalogue did not prepare every target scene/offscreen runtime key");
  Check(prepared.demandHits == 0 && prepared.demandMisses == 0 && prepared.demandWaitNanoseconds == 0,
        "Catalogue preparation unexpectedly performed demand work");
  for (const auto& entry : catalogue.configs) {
    std::visit([&](const auto& config) {
      const auto ref = aurora::gfx::find_pipeline(config, scene);
      aurora::gfx::CompiledPipeline pipeline;
      Check(aurora::gfx::get_pipeline(ref, pipeline), "Catalogue scene request returned no compiled pipeline");
      if (layoutCount == 2) {
        const auto offscreenRef = aurora::gfx::find_pipeline(config, offscreen);
        Check(aurora::gfx::get_pipeline(offscreenRef, pipeline), "Catalogue offscreen request returned no compiled pipeline");
      }
    }, entry);
  }
  const auto after = WaitReady();
  Check(after.demandHits == requests && after.demandMisses == 0 && after.demandWaitNanoseconds == 0,
        "A first catalogue request caused a compilation miss/wait");
  Check(after.totalPipelines == prepared.totalPipelines && after.readyPipelines == prepared.readyPipelines,
        "A first catalogue request created an additional runtime pipeline");
  const double startupSeconds = std::chrono::duration<double>(readyAt - started).count();
  const double preparationSeconds = std::chrono::duration<double>(readyAt - initialized).count();
  std::printf("Catalogue warmup: configs=%zu runtime_keys=%llu msaa=%u startup_seconds=%.6f "
              "preparation_seconds=%.6f demand_hits=%llu demand_misses=0 demand_wait_ns=0.\n",
              catalogue.configs.size(), static_cast<unsigned long long>(prepared.readyPipelines), msaa,
              startupSeconds, preparationSeconds, static_cast<unsigned long long>(after.demandHits));
  session.Stop();
}
}

int main(int argc, char** argv) {
  try {
#if defined(_WIN32)
    AuroraBackend backend = BACKEND_D3D12;
#elif defined(__APPLE__)
    AuroraBackend backend = BACKEND_METAL;
#else
    AuroraBackend backend = BACKEND_VULKAN;
#endif
    auto directory = std::filesystem::temp_directory_path() /
        ("charged-pipeline-warmup-" + std::to_string(SDL_GetTicksNS()));
    std::filesystem::path catalogue;
    unsigned catalogueMsaa = 1;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--directory" && i + 1 < argc) {
        directory = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[++i]));
      } else if (arg == "--catalog" && i + 1 < argc) {
        catalogue = std::filesystem::path(reinterpret_cast<const char8_t*>(argv[++i]));
      } else if (arg == "--msaa" && i + 1 < argc) {
        const std::string value = argv[++i];
        Check(value == "1" || value == "4", "--msaa must be 1 or 4");
        catalogueMsaa = value == "1" ? 1 : 4;
      } else if (arg == "--backend" && i + 1 < argc) {
        const std::string value = argv[++i];
        if (value == "vulkan") backend = BACKEND_VULKAN;
        else if (value == "dx12") backend = BACKEND_D3D12;
        else if (value == "metal") backend = BACKEND_METAL;
        else throw std::runtime_error("Unknown backend");
      } else throw std::runtime_error("Usage: pipeline_warmup_gpu_tests [--backend vulkan|dx12|metal] [--directory NEW_PATH] [--catalog DB --msaa 1|4]");
    }
    Check(!std::filesystem::exists(directory), "Fixture directory must be new (preserves existing caches)");
    const auto resources = directory / "resources";
    std::filesystem::create_directories(resources);
    if (!catalogue.empty()) {
      VerifyCatalogue(argc, argv, backend, catalogueMsaa, catalogue, directory, resources);
      std::printf("Catalogue GPU passed (%u checks); evidence: %s\n", checks, Utf8(directory).c_str());
      return 0;
    }

    // Establish an actual observed catalogue through normal production requests
    // and the real asynchronous SQLite writer. No saved pipeline is injected.
    {
      Session session;
      Start(session, argc, argv, backend, directory / "observed", resources, 1);
      aurora_prepare_pipelines();
      const auto empty = WaitReady();
      Check(empty.knownConfigs == 0 && empty.totalPipelines == 0, "Missing seed did not start empty");
      RequestKnown(false);
      const auto observed = WaitReady();
      Check(observed.demandMisses == 8 && observed.demandWaitNanoseconds > 0,
            "Cold unknown pipelines did not record demand compilation waits");
      Check(observed.knownConfigs == 8, "Observed configurations were not retained");
      VerifyHits(8);
      session.Stop();
    }
    std::filesystem::copy_file(directory / "observed" / "pipeline_cache.db",
                               resources / "initial_pipeline_cache.db");

    // Fresh local/Dawn caches: consume the bundled seed, then prove that the
    // first actual requests on this device are hits for scene/offscreen layouts.
    for (unsigned msaa : {1u, 4u}) {
      Session session;
      Start(session, argc, argv, backend, directory / ("seeded-" + std::to_string(msaa)), resources, msaa);
      aurora_prepare_pipelines();
      const auto prepared = WaitReady();
      Check(prepared.knownConfigs == 8 && prepared.readyPipelines == (msaa == 1 ? 8u : 16u),
            "Bundled seed did not prepare the selected MSAA/offscreen layouts");
      Check(prepared.demandHits == 0 && prepared.demandMisses == 0,
            "Background preparation was counted as demand work");
      VerifyHits(msaa == 1 ? 8 : 16);
      const auto total = aurora_pipeline_warmup_status().totalPipelines;
      aurora_prepare_pipelines(); aurora_prepare_pipelines();
      Check(WaitReady().totalPipelines == total, "Repeated preparation duplicated runtime pipelines");
      session.Stop();
    }

    // Cancel preparation with the normal shutdown path. An individual driver
    // compile cannot be preempted; queued catalogue work must be discarded.
    {
      Session session;
      Start(session, argc, argv, backend, directory / "cancel", resources, 4);
      aurora_prepare_pipelines();
      session.Stop();
    }
    std::printf("Pipeline warmup GPU passed (%u checks); evidence: %s\n", checks, Utf8(directory).c_str());
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Pipeline warmup GPU failure: %s\n", error.what());
    return 1;
  }
}
