#

# SUBDIRS = lisp net imlib sdlport
add_subdirectory(lisp)
add_subdirectory(net)
add_subdirectory(imlib)
add_subdirectory(sdlport)

if (WIN32)
    # FIXME: Where should this logic reside? Whatever, first generate the
    # resource script...
    get_filename_component(abuse_ICON_PATH ${CPACK_WIX_PRODUCT_ICON} ABSOLUTE)
    configure_file(win32/resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.rc ESCAPE_QUOTES NEWLINE_STYLE WIN32)
    # And then include it
    # FIXME: I think this means the build location can't have spaces
    set(abuse_RESOURCE_FILES "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
else()
    set(abuse_RESOURCE_FILES "")
endif()

# The SDL_GPU shaders, built into the binary rather than installed beside
# it. Configure time, because the blobs are committed and only change when
# someone reruns tools/shaders/build-shaders.sh.
include(${abuse_SOURCE_DIR}/cmake/EmbedShaders.cmake)
abuse_embed_shaders("${CMAKE_CURRENT_BINARY_DIR}/gpu_shaders.h"
                    palette.vert palette.frag overlay.frag present.frag
                    bright.frag blur.frag)

add_executable(abuse
    common.h
    compat.cpp compat.h
    data/config_file.cpp data/config_file.h data/paths.cpp data/paths.h
    data/fetch_classic.cpp data/fetch_classic.h
    render/options.cpp render/options.h
    render/lightmap.cpp render/lightmap.h
    render/shake.cpp render/shake.h
    render/particles.cpp render/particles.h
    render/dynlight.cpp render/dynlight.h
    imlib/hd.cpp imlib/hd.h
    sdlport/gpu_present.cpp sdlport/gpu_present.h
    audio/buses.cpp audio/buses.h
    audio/voices.cpp audio/voices.h
    audio/limiter.cpp audio/limiter.h
    audio/formats.cpp audio/formats.h
    timing/pacer.cpp timing/pacer.h
    input/actions.cpp input/actions.h
    input/gamepad.cpp input/gamepad.h
    input/rumble.cpp input/rumble.h
    input/aim.cpp input/aim.h
    i18n/language.cpp i18n/language.h
    i18n/uitext.cpp i18n/uitext.h
    ui/hexfont.cpp ui/hexfont.h
    ui/overlay.cpp ui/overlay.h
    ui/menu_list.cpp ui/menu_list.h
    ui/options_screen.cpp ui/options_screen.h
    ui/classic_data_screen.cpp ui/classic_data_screen.h
    ui/language_screen.cpp ui/language_screen.h
    ui/start_menu.cpp ui/start_menu.h
    ui/hud.cpp ui/hud.h
    lol/matrix.cpp lol/matrix.h
    lol/timer.cpp lol/timer.h
    specache.cpp specache.h
    netcfg.cpp netcfg.h
    innet.cpp
    chat.cpp chat.h
    endgame.cpp
    loadgame.cpp loadgame.h
    profile.cpp profile.h
    cop.cpp cop.h
    statbar.cpp
    compiled.cpp compiled.h
    ant.cpp ant.h
    sensor.cpp
    demo.cpp demo.h
    lcache.cpp lcache.h
    nfclient.cpp nfclient.h
    clisp.cpp clisp.h
    gui.cpp gui.h
    transp.cpp transp.h
    collide.cpp
    property.cpp property.h
    cache.cpp cache.h
    particle.cpp particle.h
    objects.cpp objects.h
    extend.cpp extend.h
    console.cpp console.h
    ability.cpp ability.h
    items.cpp items.h
    dev.cpp dev.h
    chars.cpp chars.h
    level.cpp level.h
    smallfnt.cpp
    automap.cpp automap.h
    help.cpp help.h
    intsect.cpp intsect.h
    loader2.cpp loader2.h
    seq.cpp seq.h
    points.cpp points.h
    fnt6x13.cpp
    morpher.cpp morpher.h
    menu.cpp menu.h
    director.cpp director.h
    view.cpp view.h
    configuration.cpp configuration.h
    game.cpp game.h
    harness.cpp harness.h
    light.cpp light.h
    devsel.cpp devsel.h
    crc.cpp crc.h
    gamma.cpp gamma.h
    id.h netface.h isllist.h sbar.h
    nfserver.h
    ui/volumewindow.cpp ui/volumewindow.h
    sdlport/util.h
    ${abuse_RESOURCE_FILES}
)

set_target_properties(abuse PROPERTIES MACOSX_BUNDLE true)
# Note this doesn't install the icon, that's done later
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_ICON_FILE "abuse")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Abuse")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING "${abuse_VERSION}")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${abuse_VERSION}")

target_link_libraries(abuse PRIVATE lisp)
target_link_libraries(abuse PRIVATE sdlport)
target_link_libraries(abuse PRIVATE imlib)
target_link_libraries(abuse PRIVATE net)
target_link_libraries(abuse PUBLIC SDL3::SDL3)
target_link_libraries(abuse PUBLIC SDL3_mixer::SDL3_mixer)

if(APPLE)
    # Link CoreFoundation
    target_link_libraries(abuse PRIVATE "-framework CoreFoundation")
endif()

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    # Windows looks for a DLL beside the executable, and CPM leaves SDL3.dll
    # and SDL3_mixer.dll in their own build directories. The install rules
    # below already collect them; this is about running from the build tree,
    # which is what the test suites and anyone hacking on the game do.
    #
    # Without it the binary starts and dies with "SDL3.dll: cannot open
    # shared object file", which is how the replay suite first failed on the
    # Windows runner.
    add_custom_command(TARGET abuse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:abuse> $<TARGET_FILE_DIR:abuse>
        COMMAND_EXPAND_LISTS)
endif()

add_executable(abuse-tool tool/abuse-tool.cpp crc.cpp crc.h)

target_link_libraries(abuse-tool imlib)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/lisp
    ${abuse_SOURCE_DIR}/src/imlib
    ${abuse_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_BINARY_DIR}   # gpu_shaders.h, written at configure time
)
if(WIN32)
    # Under Windows, throw them into the root
    install(TARGETS abuse abuse-tool
        RUNTIME DESTINATION "."
        LIBRARY DESTINATION ".")
    install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION ".")
    install(FILES $<TARGET_FILE:SDL3_mixer::SDL3_mixer> DESTINATION ".")
    # The Original mode needs data this project cannot ship, so the fetcher
    # travels with the game: a player who unpacked a zip has everything they
    # need to get the sound without a clone. The checksum list it reads is
    # installed with the data, by data/CMakeLists.txt, which on Windows is
    # this same directory.
    install(FILES
        ${abuse_SOURCE_DIR}/scripts/fetch-classic-data.ps1
        DESTINATION ".")
elseif(APPLE)
    install(TARGETS abuse abuse-tool RUNTIME DESTINATION bin
    BUNDLE DESTINATION "${CMAKE_INSTALL_PREFIX}")
    # Fix up the bundle (basically, install SDL with it)
    install(CODE "
        include(BundleUtilities)
        set(BU_CHMOD_BUNDLE_ITEMS TRUE)
        fixup_bundle(
            \"${CMAKE_INSTALL_PREFIX}/abuse.app\"
            \"\"
            \"${SDL3_BINARY_DIR};${SDL3_mixer_BINARY_DIR}\"
        )
    ")
    # Add icon
    install(FILES "../macOS/abuse.icns" DESTINATION "${CMAKE_INSTALL_PREFIX}/abuse.app/Contents/Resources")
else()
    # lib or lib64, and bin, spelled the way this distribution spells them.
    include(GNUInstallDirs)

    # The installed name is abuse-vrenna, and the built one stays abuse.
    #
    # The two cannot be the same. `abuse` is taken on Flathub and in the AUR
    # by two other ports of this game, which is the whole reason for the
    # project's name; and every test script, the CI workflow and the
    # packaging notes point at build/<preset>/src/abuse, so renaming the
    # target instead of the installed copy would rename the thing hundreds
    # of lines already refer to.
    #
    # install(TARGETS) and a rename afterwards, rather than the shorter
    # install(PROGRAMS ... RENAME): only install(TARGETS) rewrites the
    # RPATH. Copied as a plain file, the binary keeps the one the linker
    # gave it, which points into this build tree, and the package then
    # works on the machine that built it and nowhere else. Measured, not
    # assumed: the first attempt shipped a RUNPATH of
    # /home/.../build/dev/_deps/sdl3_mixer-build.
    install(TARGETS abuse abuse-tool RUNTIME DESTINATION bin)
    install(CODE "
        foreach(pair \"abuse;abuse-vrenna\" \"abuse-tool;abuse-vrenna-tool\")
            list(GET pair 0 built)
            list(GET pair 1 shipped)
            # The prefix is read at install time, not baked in here: a
            # --prefix on the install command overrides the configured one,
            # and CMAKE_INSTALL_FULL_BINDIR would still name the old path.
            set(dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
            if(EXISTS \"\${dir}/\${built}\")
                file(RENAME \"\${dir}/\${built}\" \"\${dir}/\${shipped}\")
            endif()
        endforeach()
    ")

    # A library CPM built is a library only this build tree has.
    #
    # No distribution packages SDL3_mixer yet, so CPM builds it here, and
    # until now nothing installed it: the installed binary died on
    # "libSDL3_mixer.so.0: cannot open shared object file". Where a
    # distribution does package it, CPM finds it instead and *_ADDED is
    # false, so this installs nothing and the system copy is used.
    #
    # Both names go in, because what the loader asks for is the SONAME and
    # what the target points at is the real file; they are two entries and
    # not a symlink because install(FILES) writes files.
    #
    # Into a directory of our own, and not into the system one: a .deb that
    # writes /usr/lib/x86_64-linux-gnu/libSDL3.so.0 refuses to install,
    # because that path belongs to Debian's libsdl3-0. Measured by building
    # the package and installing it in a clean container.
    foreach(lib SDL3 SDL3_mixer)
        if(${lib}_ADDED)
            install(FILES
                $<TARGET_FILE:${lib}::${lib}>
                $<TARGET_SONAME_FILE:${lib}::${lib}>
                DESTINATION ${CMAKE_INSTALL_LIBDIR}/abuse-vrenna)
        endif()
    endforeach()

    # So the binary finds them next door instead of only in a system path.
    # Where a distribution packages the library, nothing was installed above
    # and this directory does not exist, so the system copy is what loads.
    set_target_properties(abuse PROPERTIES
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/abuse-vrenna")
endif()
