# Unit tests. The replay and snapshot suites live in scripts/ and are
# registered further down, because they drive the real binary.

add_executable(abuse_unit_tests
    unit/main.cpp
    unit/test_keys.cpp
    unit/test_palette.cpp
    unit/test_paths.cpp
    unit/test_config_file.cpp
    unit/test_render_options.cpp
    unit/test_pacer.cpp
    unit/test_actions.cpp
    unit/test_gamepad.cpp
    unit/test_rumble.cpp
    unit/test_aim.cpp
    unit/test_language.cpp
    unit/test_fonts.cpp
    unit/test_hexfont.cpp
    unit/test_overlay.cpp
    unit/test_menu_list.cpp
    unit/test_uitext.cpp
    unit/test_specs.cpp
    unit/test_audio.cpp
    unit/test_lightmap.cpp
    unit/test_shake.cpp
    unit/test_particles.cpp
    unit/test_dynlight.cpp
    unit/test_formats.cpp
    unit/test_hd.cpp
    unit/test_fetch_classic.cpp
    ${abuse_SOURCE_DIR}/src/compat.cpp
    ${abuse_SOURCE_DIR}/src/imlib/hd.cpp
    ${abuse_SOURCE_DIR}/src/data/paths.cpp
    ${abuse_SOURCE_DIR}/src/data/fetch_classic.cpp
    ${abuse_SOURCE_DIR}/src/data/config_file.cpp
    ${abuse_SOURCE_DIR}/src/render/options.cpp
    ${abuse_SOURCE_DIR}/src/render/lightmap.cpp
    ${abuse_SOURCE_DIR}/src/render/shake.cpp
    ${abuse_SOURCE_DIR}/src/render/particles.cpp
    ${abuse_SOURCE_DIR}/src/render/dynlight.cpp
    ${abuse_SOURCE_DIR}/src/audio/formats.cpp
    ${abuse_SOURCE_DIR}/src/audio/buses.cpp
    ${abuse_SOURCE_DIR}/src/audio/voices.cpp
    ${abuse_SOURCE_DIR}/src/audio/limiter.cpp
    ${abuse_SOURCE_DIR}/src/timing/pacer.cpp
    ${abuse_SOURCE_DIR}/src/input/actions.cpp
    ${abuse_SOURCE_DIR}/src/input/gamepad.cpp
    ${abuse_SOURCE_DIR}/src/input/rumble.cpp
    ${abuse_SOURCE_DIR}/src/input/aim.cpp
    ${abuse_SOURCE_DIR}/src/i18n/language.cpp
    ${abuse_SOURCE_DIR}/src/fnt6x13.cpp
    ${abuse_SOURCE_DIR}/src/ui/hexfont.cpp
    ${abuse_SOURCE_DIR}/src/ui/overlay.cpp
    ${abuse_SOURCE_DIR}/src/ui/menu_list.cpp
    ${abuse_SOURCE_DIR}/src/i18n/uitext.cpp
)

target_include_directories(abuse_unit_tests PRIVATE
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
    ${abuse_BINARY_DIR}
)

target_compile_definitions(abuse_unit_tests PRIVATE
    ABUSE_TEST_DATA_DIR="${abuse_SOURCE_DIR}/data"
    ABUSE_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(abuse_unit_tests PRIVATE ${doctest_SOURCE_DIR})

# detect_language() asks SDL for the user's locale, so the pure-logic suite
# still needs the library it links against.
target_link_libraries(abuse_unit_tests PRIVATE imlib SDL3::SDL3)

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    # Same reason as for the game: this one links SDL3 for the locale check.
    add_custom_command(TARGET abuse_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:abuse_unit_tests> $<TARGET_FILE_DIR:abuse_unit_tests>
        COMMAND_EXPAND_LISTS)
endif()

add_test(NAME unit COMMAND abuse_unit_tests)

# The three suites below are shell scripts. CTest runs a command, not a shell,
# so on a machine without one they simply are not registered: better a suite
# that is visibly absent than one that fails for the wrong reason. Windows
# runners do have bash, which is how they run there.
find_program(ABUSE_BASH bash)

if(ABUSE_BASH)

# Replay determinism. Skipped rather than failed when no replay has been
# recorded yet, so a fresh clone does not report a red suite for a missing
# fixture.
add_test(NAME replays
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-replays.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(replays PROPERTIES SKIP_RETURN_CODE 77)

# Writing a savegame, which is the one thing the suites never did until it
# crashed on Windows in front of a person. The path it writes to is chosen by
# the platform, so this only means anything when it runs on the platform.
add_test(NAME save
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-save.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})

# The simulation must not depend on the width of the window: what wakes up
# in a level is the region around the player, not what happens to be on
# screen. Without this, widescreen is a different game.
add_test(NAME aspect
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-aspect.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(aspect PROPERTIES SKIP_RETURN_CODE 77)

# What the packages actually ship. Every other suite runs from the source
# tree, where everything is where the game left it; installing is the step
# they all take and the one nothing looked at, and it broke silently once.
add_test(NAME install_tree
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/check-install.sh
            ${abuse_BINARY_DIR}
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(install_tree PROPERTIES SKIP_RETURN_CODE 77)

# The free pack against what the game asks for. A missing sound is the
# quietest failure there is: the game plays on and one event is silent
# forever. Cheap, and it needs no binary.
add_test(NAME audio_pack
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/check-audio-pack.sh
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})

# The Flatpak manifest against what the build actually fetches. Cheap, and
# it has already been wrong once: pinned to SDL3 3.2.0 while the build used
# 3.4.14, and to a branch, which Flathub refuses. Skips itself without
# PyYAML.
add_test(NAME flatpak
    COMMAND bash ${abuse_SOURCE_DIR}/scripts/check-flatpak-pins.sh
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(flatpak PROPERTIES SKIP_RETURN_CODE 77)

# The GPU presentation path, compared against the classic one rather than
# against the stored frames: a window read back from a real driver does not
# match one read back from SDL's dummy driver, and the references are all
# headless. Skips itself where the path cannot start, which is every CI
# runner. See scripts/test-gpu.sh for what it exists to catch.
add_test(NAME gpu
    COMMAND bash ${abuse_SOURCE_DIR}/scripts/test-gpu.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(gpu PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME snapshots
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-snapshots.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(snapshots PROPERTIES SKIP_RETURN_CODE 77)

# The same frames as presented in the window: scale mode, filter and letterbox.
# The suite above dumps the buffer the game draws into, which is 320x200 no
# matter what any of those are set to, so none of them are visible in it.
add_test(NAME window
    COMMAND ${CMAKE_COMMAND} -E env WINDOW=1
            ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-snapshots.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(window PROPERTIES SKIP_RETURN_CODE 77)

# Leak detection is on, with the roots that are never torn down listed in
# tests/lsan.supp. It used to be off entirely, because the engine abandoned
# ~200 KB at exit and LeakSanitizer turned that into exit code 1, which the
# scripts' set -e read as a failure.
#
# Most of that pile was one defect and is gone; what is left is the window
# manager, which has no shutdown, and ALSA. Suppressing those two and
# turning detection on is worth more than the pile was costing: a leak
# introduced by new code now fails the suite instead of disappearing into
# it.
# The suppression file goes to LSAN_OPTIONS and not to ASAN_OPTIONS: they
# are separate files with separate grammars, and ASan answers a `leak:` line
# with "failed to parse suppressions" and exits.
set_tests_properties(replays save aspect snapshots window gpu PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1;LSAN_OPTIONS=suppressions=${abuse_SOURCE_DIR}/tests/lsan.supp")

else()
    message(STATUS "No bash found: the replay and snapshot suites are not registered")
endif()
