include_guard(GLOBAL)

# Nintendo Switch, for now: the build makes the two game images
# (cmake/StaticGameImages.cmake) and stops short of the program, whose host is
# not ported yet (Aurora with graphics through Dawn and NVK, SDL, and a disc
# reader in place of nod). Until it is, the host libraries' names stand only
# for the SDK headers the game compiles with, so the shared build files
# configure; building anything that links them fails. The switch build preset
# builds the images alone.
if(NOT MSCHARGED_BUILD_ORIGINAL_FRONTEND_DIAGNOSTIC)
    message(FATAL_ERROR "The Switch build makes the game images: use the switch preset")
endif()
if(MSCHARGED_BUILD_LAUNCHER OR MSCHARGED_BUILD_AURORA_HOST OR MSCHARGED_BUILD_GX_CHECK
        OR MSCHARGED_BUILD_GAME_STARTUP OR MSCHARGED_BUILD_SCENE_PREVIEW OR BUILD_TESTING)
    message(FATAL_ERROR "The Switch build has no host yet: only the game images build (switch preset)")
endif()
# Horizon has no dynamic loading: the game modules are always static images.
set(MSCHARGED_STATIC_GAME ON CACHE BOOL
    "Link the game modules into the program instead of loading them (no dynamic loading)" FORCE)
# devkitPro's platform makes every module an archive, as it has no shared
# libraries; the game modules must stay linked targets to become images.
set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

mscharged_prepare_dependency(aurora MSCHARGED_AURORA_PREPARED)
# What the game takes from Aurora's OS library (cmake/aurora_os.cmake there).
add_library(aurora::os INTERFACE IMPORTED)
set_target_properties(aurora::os PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${MSCHARGED_AURORA_PREPARED}/include"
    INTERFACE_COMPILE_DEFINITIONS AURORA_WII_CLOCK=1
    INTERFACE_LINK_LIBRARIES switch-host-pending-aurora-os)
foreach(name IN ITEMS aurora::core aurora::dvd aurora::gx aurora::mtx aurora::vi
        SDL3::SDL3 ZLIB::ZLIB absl::flat_hash_map nod::nod)
    string(REPLACE "::" "-" library "${name}")
    add_library(${name} INTERFACE IMPORTED)
    set_target_properties(${name} PROPERTIES INTERFACE_LINK_LIBRARIES "switch-host-pending-${library}")
endforeach()
