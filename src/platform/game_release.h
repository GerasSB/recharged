#pragma once

#include "platform/disc.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mscharged
{
// The game code that runs a release. Both European revisions run the code of
// revision 2; a revision 1 disc also turns on the tweak series' adapters for
// its older data (audio bank and script numbering, one pool size).
enum class GameCode : std::uint8_t
{
    Usa,
    Europe,
};

// A game text language: the [game] language value, its launcher label and the
// Wii system language that makes original main pick it for this release.
struct GameLanguage
{
    const char* value;
    const char* label;
    std::uint8_t sc_language;
};

struct GameRelease
{
    const char* game_id;           // disc ID, e.g. "R4QE01"
    std::uint8_t revision;         // revision byte of the disc header
    const char* dol_sha1;          // SHA-1 of sys/main.dol (lower-case hex)
    const char* region;            // "USA", "Europe"
    const char* title;             // title as printed on the box
    std::uint64_t title_id;        // Wii title: saves and the NAND home
    GameCode code;                 // game module that runs it
    bool rev1_data;                // EU revision 1 data on the revision 2 code
    bool playable;                 // this build has the game module for it
    std::uint32_t disc_message_table; // sDiscMessageImages in main.dol
    const GameLanguage* languages; // game text languages, without Automatic
    std::size_t language_count;
};

// Every release this port knows, playable or not.
std::size_t ReleaseCount();
const GameRelease& ReleaseAt(std::size_t index);

// The release with this disc ID and header revision, or nullptr.
const GameRelease* FindRelease(std::string_view game_id, std::uint8_t revision);
inline const GameRelease* FindRelease(const DiscInfo& info) { return FindRelease(info.game_id, info.revision); }

// "USA" or "Europe, revision 2" (the revision only where a region has several).
std::string ReleaseLabel(const GameRelease& release);

// The language entry for a [game] language value, or nullptr.
const GameLanguage* FindLanguage(const GameRelease& release, std::string_view value);
}
