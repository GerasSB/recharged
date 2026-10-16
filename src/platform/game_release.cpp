#include "platform/game_release.h"

#include <iterator>

namespace mscharged
{
namespace
{
// Wii system languages (SC_LANG_*) as original main maps them: the USA build
// shows English, NA French or NA Spanish; the European build UK English,
// German, French, Spanish or Italian.
constexpr GameLanguage kUsaLanguages[] = {
    {"english", "English", 1},
    {"french", "French", 3},
    {"spanish", "Spanish", 4},
};
constexpr GameLanguage kEuropeLanguages[] = {
    {"english", "English", 1},
    {"german", "German", 2},
    {"french", "French", 3},
    {"spanish", "Spanish", 4},
    {"italian", "Italian", 5},
};

// main.dol hashes and disc-message tables follow the decomp's
// config/<version>/build.sha1 and symbols.txt.
constexpr GameRelease kReleases[] = {
    {"R4QE01", 1, "e96d2298067b70652752145b4e63644e6e1b1560", "USA", "Mario Strikers Charged",
     0x0001000052345145ull, GameCode::Usa, false, true, 0x804E70E0u,
     kUsaLanguages, std::size(kUsaLanguages)},
    {"R4QP01", 2, "c39e1eda1942aa2fc96a6a71bfef051698e23516", "Europe", "Mario Strikers Charged Football",
     0x0001000052345150ull, GameCode::Europe, false, false, 0x804E6A08u,
     kEuropeLanguages, std::size(kEuropeLanguages)},
    {"R4QP01", 1, "0a066b755b469648a371d1cfc2d1bcad315b2b4a", "Europe", "Mario Strikers Charged Football",
     0x0001000052345150ull, GameCode::Europe, true, false, 0x804E6488u,
     kEuropeLanguages, std::size(kEuropeLanguages)},
};
}

std::size_t ReleaseCount() { return std::size(kReleases); }

const GameRelease& ReleaseAt(std::size_t index) { return kReleases[index]; }

const GameRelease* FindRelease(std::string_view game_id, std::uint8_t revision)
{
    for (const auto& release : kReleases)
        if (game_id == release.game_id && revision == release.revision) return &release;
    return nullptr;
}

std::string ReleaseLabel(const GameRelease& release)
{
    std::string label = release.region;
    int variants = 0;
    for (const auto& other : kReleases)
        if (std::string_view(other.game_id) == release.game_id) ++variants;
    if (variants > 1) label += ", revision " + std::to_string(release.revision);
    return label;
}

const GameLanguage* FindLanguage(const GameRelease& release, std::string_view value)
{
    for (std::size_t i = 0; i < release.language_count; ++i)
        if (value == release.languages[i].value) return &release.languages[i];
    return nullptr;
}

bool SupportedRelease(const DiscInfo& info)
{
    const auto* release = FindRelease(info);
    return release && release->playable;
}
}
