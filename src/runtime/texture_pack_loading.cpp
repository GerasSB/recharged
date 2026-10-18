#include "runtime/texture_pack_loading.h"

#include "platform/path.h"
#include "platform/session_log.h"
#include "platform/texture_packs.h"

#include <dolphin/gx.h>
#include <aurora/texture.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

namespace mscharged
{
namespace
{
bool ReadPackFile(void* pack, const char* path, std::vector<std::uint8_t>& out)
{
    return static_cast<const platform::TexturePackFiles*>(pack)->Read(path, out);
}

using KeyFields = std::tuple<std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, bool>;
KeyFields Fields(const aurora::texture::TextureSourceKey& key)
{
    return {key.textureHash, key.tlutHash, key.width, key.height, key.format, key.hasTlut};
}

void LoadPacks(const Settings& settings, const std::filesystem::path& config_path, std::string_view game_id)
{
    if (settings.dump_textures)
        platform::SessionLogLine("Texture dumps: on (the game's textures are saved to data/texture_dumps)");
    if (!settings.texture_packs)
    {
        platform::SessionLogLine("Texture packs: off");
        return;
    }
    const auto folder = ResolveTextureFolder(settings, config_path);
    const auto off = platform::SplitPackNames(settings.texture_packs_off);
    auto packs = platform::OrderTexturePacks(platform::FindTexturePacks(folder),
                                             platform::SplitPackNames(settings.texture_pack_order));
    packs.erase(std::remove_if(packs.begin(), packs.end(),
                               [&](const platform::TexturePack& pack) {
                                   return std::find(off.begin(), off.end(), pack.name) != off.end();
                               }),
                packs.end());
    if (packs.empty())
    {
        platform::SessionLogLine("Texture packs: none in " + PathUtf8(folder));
        return;
    }
    // Aurora's texture workers read the packs until the process ends, so
    // they are never freed.
    static auto* loaded = new std::vector<std::unique_ptr<platform::TexturePackFiles>>;
    std::string summary;
    std::size_t total = 0;
    // The first pack gets the highest priority.
    auto priority = std::int32_t(packs.size());
    for (const auto& pack : packs)
    {
        const auto current = priority--;
        if (!pack.error.empty())
        {
            platform::SessionLogLine("Texture packs: " + pack.name + " skipped: " + pack.error);
            continue;
        }
        std::unique_ptr<platform::TexturePackFiles> files;
        try
        {
            files = std::make_unique<platform::TexturePackFiles>(pack, game_id);
        }
        catch (const std::exception& failure)
        {
            platform::SessionLogLine("Texture packs: " + pack.name + " skipped: " + failure.what());
            continue;
        }
        // Within a pack the first image of a name wins (as in Dolphin).
        std::set<KeyFields> seen;
        std::size_t count = 0;
        for (const auto& image : files->Images())
        {
            const auto slash = image.rfind('/');
            const auto key =
                aurora::texture::parse_replacement_filename(slash == std::string::npos ? image : image.substr(slash + 1));
            if (!key || !seen.insert(Fields(*key)).second) continue;
            const auto registration = aurora::texture::register_virtual_replacement(
                image, {.read = ReadPackFile, .userData = files.get()}, {.priority = current});
            if (registration.id) ++count;
        }
        if (!count) continue;
        total += count;
        summary += (summary.empty() ? "" : ", ") + pack.name + " (" + std::to_string(count) + ")";
        loaded->push_back(std::move(files));
    }
    platform::SessionLogLine(total ? "Texture packs: " + summary : "Texture packs: no usable images in " + PathUtf8(folder));
}
}

void LoadTexturePacks(const Settings& settings, const std::filesystem::path& config_path, std::string_view game_id)
{
    // A pack never stops the game from starting: it plays with its own textures.
    try
    {
        LoadPacks(settings, config_path, game_id);
    }
    catch (const std::exception& failure)
    {
        platform::SessionLogLine(std::string("Texture packs: not loaded: ") + failure.what());
    }
}
}
