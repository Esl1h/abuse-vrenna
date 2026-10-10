cmake_minimum_required(VERSION 3.14)

project(abuse C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Ours, not upstream's. Xenoveritas was at 0.10.0; this fork numbers itself
# by its own plan, which is v0.9.0-alpha.N while the phases close and 1.0.0
# when they are done. A tag and the number the game prints have to be the
# same string, or the bug report says a version nobody can check out.
set(abuse_VERSION 0.9.0-alpha.3)
set(abuse_VERSION_MAJOR 0)
set(abuse_VERSION_MINOR 9)
set(abuse_VERSION_PATCH 0)
set(abuse_VERSION_TWEAK 0)

include(cmake/CPM.cmake)

# Detect the platform, as there's quite a bit of platform variation

if (WIN32)
    # TODO: Think about Win64
    enable_language(RC)
endif()

# Enabling FluidSynth requires a soundfont be loaded which means it doesn't really work
option(ABUSE_ENABLE_FLUIDSYNTH "Include FluidSynth as a dependency (for music playback)" OFF)
# Using SDL3_native_midi makes this work a lot like it did with SDL2_mixer
option(ABUSE_ENABLE_NATIVE_MIDI "Use SDL3_native_midi to play music" ON)
option(ABUSE_BUILD_TESTS "Build the unit tests and register them with CTest" ON)

# To avoid downloading a package, configure CPM
# 3.4, and not the 3.2 this asked for until a Debian package was built: the
# SDL3_mixer below calls SDL_PutAudioStreamPlanarData and
# SDL_PutAudioStreamDataNoCopy, which arrived in 3.4. With 3.2 accepted, a
# distribution that ships 3.2 satisfied the check and then failed to compile
# the mixer. Where the system SDL3 is older than this, CPM builds the pinned
# one from source and src/CMakeLists.txt installs it beside the game.
CPMAddPackage(
    NAME SDL3
    VERSION 3.4.0
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.14
)
if (ABUSE_ENABLE_FLUIDSYNTH)
    CPMAddPackage(
        NAME FluidSynth
        VERSION 2.6.0
        GITHUB_REPOSITORY FluidSynth/fluidsynth
        EXCLUDE_FROM_ALL ON
        OPTIONS
            # Disable everything SDL3_mixer would
            "enable-alsa OFF"
            "enable-aufile OFF"
            "enable-dbus OFF"
            "enable-ipv6 OFF"
            "enable-jack OFF"
            "enable-ladspa OFF"
            "enable-libinstpatch OFF"
            "enable-libsndfile OFF"
            "enable-midishare OFF"
            "enable-opensles OFF"
            "enable-oboe OFF"
            "enable-network OFF"
            "enable-oss OFF"
            "enable-dsound OFF"
            "enable-wasapi OFF"
            "enable-waveout OFF"
            "enable-winmidi OFF"
            "enable-sdl3 OFF"
            "enable-pulseaudio OFF"
            "enable-pipewire OFF"
            "enable-readline OFF"
            "enable-threads OFF"
            "enable-openmp OFF"
            "enable-unicode OFF"
            "enable-native OFF"
            "enable-signalsmith OFF"
            "enable-framework OFF"
            "enable-systemd OFF"
            "enable-coreaudio OFF"
            "enable-coremidi OFF"
            "enable-dart OFF"
            "enable-kai OFF"
    )
    add_library(FluidSynth::libfluidsynth ALIAS libfluidsynth)
endif()
CPMAddPackage(
    NAME SDL3_mixer
    VERSION 3.0.0
    GITHUB_REPOSITORY libsdl-org/SDL_mixer
    GIT_TAG release-3.2.4
    OPTIONS
        # Don't use the vendored libraries (they all add support for stuff we don't need anyway)
        "SDLMIXER_VENDORED OFF"
        "SDLMIXER_MIDI_FLUIDSYNTH ${ABUSE_ENABLE_FLUIDSYNTH}"
        "SDLMIXER_MIDI_TIMIDITY OFF"
)
if (ABUSE_ENABLE_NATIVE_MIDI)
    find_package(Git REQUIRED)
    # This is really a fancy way to make this an optional, external dependency.
    # It's never used directly - instead, the source files are added to the sdlport target.
    # PATCHES: get_app_name() adds one to what SDL_strrchr returned before
    # testing it, so a command line with no '/' in it reads address one and
    # the process dies in the MIDI probe. A Flatpak launches the game as
    # "abuse-vrenna", with no path, which is how this was found.
    CPMAddPackage(
        NAME SDL3_native_midi
        # Version number is made up
        VERSION 1.0
        GITHUB_REPOSITORY libsdl-org/SDL_native_midi
        GIT_TAG 64236646bc50ba7a196c2cc05dad86f0bde3f55f
        EXCLUDE_FROM_ALL ON
        # Not CPM's PATCHES keyword: that one needs the `patch` program,
        # which the reference host does not have. See apply.cmake.
        PATCH_COMMAND ${CMAKE_COMMAND}
            -DGIT=${GIT_EXECUTABLE}
            -DPATCH=${CMAKE_CURRENT_SOURCE_DIR}/third_party/patches/native-midi-app-name.patch
            -P ${CMAKE_CURRENT_SOURCE_DIR}/third_party/patches/apply.cmake
    )
endif()
if (ABUSE_BUILD_TESTS)
    # Header only, and its own CMakeLists declares cmake_minimum_required(3.0),
    # which CMake 4 rejects. Take the headers and skip its build system.
    CPMAddPackage(
        NAME doctest
        VERSION 2.4.11
        GITHUB_REPOSITORY doctest/doctest
        GIT_TAG v2.4.11
        DOWNLOAD_ONLY YES
    )
endif()

include(CheckIncludeFiles)

check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files("sys/time.h" HAVE_SYS_TIME_H)
check_include_files("sys/ioctl.h" HAVE_SYS_IOCTL_H)
check_include_files("netinet/in.h" HAVE_NETINET_IN_H)
check_include_files(bstring.h HAVE_BSTRING_H)

set(HAVE_NETWORK TRUE CACHE BOOL "Enable networking support")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if(WIN32)
    # For Windows, just use whatever CMake wants
    set(ASSETDIR ".")
elseif(APPLE)
    # Under macOS, I actually want to redirect this to an app bundle,
    # which makes things weird.
    set(ASSETDIR "abuse.app/Contents/Resources/data")
else()
    # Under Linux, match the original autotools prefix
    set(ASSETDIR "share/games/abuse")
    # Only define this for ports that actually use it
    add_definitions(-DASSETDIR="${CMAKE_INSTALL_PREFIX}/${ASSETDIR}")
endif()

add_definitions(-DPACKAGE_NAME="abuse")
add_definitions(-DPACKAGE_VERSION="${abuse_VERSION}")
add_definitions(-DHAVE_CONFIG_H)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Packaging

# For CMAKE_INSTALL_FULL_LIBDIR below. src/CMakeLists.txt includes this too,
# for the install rules; including it twice costs nothing.
include(GNUInstallDirs)

set(CPACK_PACKAGE_NAME "abuse-vrenna")
set(CPACK_PACKAGE_VENDOR "Abuse: Vrenna")
set(CPACK_PACKAGE_VERSION ${abuse_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${abuse_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${abuse_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${abuse_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern port of Abuse (1995)")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Esl1h/abuse-vrenna")
set(CPACK_PACKAGE_CONTACT "Esli Silva <eslih@pm.me>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_EXECUTABLES "abuse-vrenna;Abuse: Vrenna")
set(CPACK_STRIP_FILES ON)
if (WIN32)
    set(CPACK_GENERATOR "WIX;ZIP")
    # This doesn't make sense for a Windows archive
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY off)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")

    # The native packages. Which of the three generators is asked for is the
    # caller's business, hence scripts/package-linux.sh; the settings below
    # only say what each one should contain when it is.
    #
    # The dependency lists are deliberately short. SDL3 is the only library
    # this asks a distribution for: SDL3_mixer is built here and installed
    # beside the game, because no distribution packages it yet, and the
    # binary finds it through a RUNPATH of $ORIGIN/../${CMAKE_INSTALL_LIBDIR}.
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    # Worked out from what the binary actually links against, which differs
    # by distribution: where SDL3 comes from the system it is a dependency,
    # and where it was built here it is inside the package instead.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    # A game provides nothing to anything else. Without this, a bundled
    # libSDL3_mixer.so.0 would be advertised as this package providing the
    # library, which is not something another package should ever satisfy
    # from here.
    set(CPACK_RPM_PACKAGE_AUTOPROV OFF)

    # And what is bundled must not be required either. rpm reads the
    # binary's NEEDED list and asks for every name in it; with the providing
    # side turned off above, a bundled libSDL3_mixer.so.0 became a
    # dependency nothing on the system could satisfy and dnf refused to
    # install the package. Only the libraries this build actually brought
    # with it are filtered, so a system SDL3 stays a real dependency.
    set(_bundled "")
    if(SDL3_ADDED)
        list(APPEND _bundled "libSDL3\\.so")
    endif()
    if(SDL3_mixer_ADDED)
        list(APPEND _bundled "libSDL3_mixer\\.so")
    endif()
    if(_bundled)
        string(JOIN "|" _bundled_re ${_bundled})
        set(CPACK_RPM_SPEC_MORE_DEFINE
            "%define __requires_exclude ^(${_bundled_re}).*$")
    endif()

    # dpkg-shlibdeps reports a private directory as a library with no
    # package behind it, which is an error unless it is told where to look.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
        "${CMAKE_INSTALL_FULL_LIBDIR}/abuse-vrenna")
    # The game data is not a candidate for byte compiling or stripping, and
    # /usr/share/games is owned by the filesystem package.
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        "/usr/share/games" "/usr/share/icons/hicolor"
        "/usr/share/icons/hicolor/16x16" "/usr/share/icons/hicolor/16x16/apps"
        "/usr/share/icons/hicolor/32x32" "/usr/share/icons/hicolor/32x32/apps"
        "/usr/share/icons/hicolor/48x48" "/usr/share/icons/hicolor/48x48/apps"
        "/usr/share/icons/hicolor/64x64" "/usr/share/icons/hicolor/64x64/apps"
        "/usr/share/icons/hicolor/128x128" "/usr/share/icons/hicolor/128x128/apps"
        "/usr/share/icons/hicolor/256x256" "/usr/share/icons/hicolor/256x256/apps"
        "/usr/share/icons/hicolor/512x512" "/usr/share/icons/hicolor/512x512/apps"
        "/usr/share/icons/hicolor/scalable" "/usr/share/icons/hicolor/scalable/apps")
endif()
# WIX-specific
set(CPACK_WIX_UPGRADE_GUID "3A89DEB1-B520-4DF5-B44D-96A0EBB27DC0")
set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.rtf")
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/doc/icon.ico")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Abuse")

include(CPack)

add_subdirectory(src)
add_subdirectory(data)

if (ABUSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (ABUSE_ENABLE_FLUIDSYNTH)
    # Add Fluidsynth to the build
    target_link_libraries(abuse PRIVATE libfluidsynth)
    install(FILES $<TARGET_FILE:libfluidsynth> DESTINATION ".")
    install(FILES "${FluidSynth_SOURCE_DIR}/sf2/VintageDreamsWaves-v2.sf2" DESTINATION "${ASSETDIR}")
endif()