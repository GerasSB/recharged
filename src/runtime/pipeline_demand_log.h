#pragma once

#include <aurora/pipeline_cache.h>

#include <string>

namespace mscharged::runtime {
// The session log line for a draw that waited for its pipeline to compile, a
// stutter in play. The line for a configuration new on this PC ends with its
// raw bytes (zlib, base64), so testers' logs complete the bundled catalogue:
//   tools/pipeline_catalog.py --catalog assets/graphics/pipelines.json --import mscharged.log
// Example (config shortened):
//   Graphics: drawing waited 45.2 ms to compile a new pipeline: pipeline=gx17-1f2e3d4c5b6a7980 frame=18234 config=eNrt...
std::string PipelineDemandMissLine(const AuroraPipelineDemandMiss& miss);
} // namespace mscharged::runtime
