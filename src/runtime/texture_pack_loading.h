#pragma once

#include "bootstrap/config.h"

#include <filesystem>
#include <string_view>

namespace mscharged
{
// Hands the HD texture packs that are switched on ([mods], see Settings) to
// Aurora, which swaps their images in for the game's textures with the same
// hash: the first pack in the player's order wins. Only the host's graphics
// change; the game's own textures and data stay as on the disc. Logs what it
// loaded. Called once, after Aurora has started.
void LoadTexturePacks(const Settings& settings, const std::filesystem::path& config_path, std::string_view game_id);
}
