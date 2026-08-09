cmake_minimum_required(VERSION 3.13...3.21)

# Prevent in-source builds that create CMake artifacts in the repo root.
# Packaging environments can explicitly opt in with -DFORCE_IN_SOURCE_BUILDS=ON.
if(NOT DEFINED FORCE_IN_SOURCE_BUILDS)
    set(FORCE_IN_SOURCE_BUILDS OFF CACHE BOOL "Allow in-source CMake builds")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT FORCE_IN_SOURCE_BUILDS)
    message(FATAL_ERROR
        "In-source builds are not supported.\n"
        "Use an out-of-source build directory (for example: var/build/native).\n"
        "Try: just configure\n"
        "If you previously configured in-source, remove CMakeFiles/ and CMakeCache.txt from the project root.")
endif()

option(APPVEYOR_BUILD "Build on appveyor" OFF)
option(CI_BUILD "Set when building in CI. Enables -Werror where possible" OFF)
option(ASAN "Compile with address sanitizers" OFF)
option(QML_DEBUGGING "Enable qml debugging" OFF)
option(MAN "Build man page" OFF)
option(FLATPAK "Set this only if Komai is built as a flatpak" OFF)

# Default to Release builds. Packagers can override with -DCMAKE_BUILD_TYPE=None/Debug/etc.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# CPM-fetched dependencies (litehtml, KDSingleApplication, QtKeychain) are built
# from source and linked into the komai binary. Build them as static libraries so
# the installed binary is self-contained and doesn't require these .so files at
# runtime. Packagers using -DCPM_USE_LOCAL_PACKAGES=ON get system shared libs
# via find_package() for KDSingleApplication and QtKeychain instead, so this
# setting doesn't affect them. litehtml is the exception: it is always built
# from our pinned source regardless of that flag (see the CPMAddPackage call
# below for why).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled dependencies as static libraries")

set(
    CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake"
    CACHE
    FILEPATH "Default toolchain"
)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard to be supported")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "compile as PIC by default")
# std::format with float/double pulls in libc++'s to_chars(long double), which Apple marks __builtin_available(macos 13.3); the prior 10.15 default broke every native macOS build with "'to_chars' is unavailable" inside std::format instantiations.
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "macos deployment target")

include(cmake/CPM.cmake)

project(komai LANGUAGES CXX C)

include(CMakeDependentOption)
set(VOIP_DEFAULT ON)
if(APPLE OR WIN32 OR HAIKU)
    set(VOIP_DEFAULT OFF)
endif()
option(VOIP "Whether to enable voip support. Disable this, if you don't have gstreamer." ${VOIP_DEFAULT})
set(X11_DEFAULT)
if(WIN32 OR APPLE OR HAIKU)
    set(X11_DEFAULT OFF)
endif()
option(X11 "Whether to enable X11 specific features (screenshare, window roles)." ${X11_DEFAULT})
cmake_dependent_option(SCREENSHARE_XDP "Whether to enable screenshare support using xdg-desktop-portal." ON "VOIP" OFF)
# Element Call (MatrixRTC group voice/video). Pulls in QtWebEngine (Chromium),
# which adds ~120-180 MB per artifact and is non-trivial to obtain on some
# platforms (Windows/macOS), so it is gated behind this option. Default ON;
# packagers without QtWebEngine can build with -DELEMENT_CALL=OFF.
option(ELEMENT_CALL "Whether to enable Element Call support. Requires QtWebEngine." ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Include Qt basic functions
include(QtCommon)

include(GNUInstallDirs)
include(CTest)

# Read version from the top-level VERSION.txt file (CalVer: YYYY.MM.DD.BUILD).
# Stored as VERSION.txt rather than VERSION so it cannot be picked up as
# the C++ <version> standard header on case-insensitive filesystems
# (Windows, macOS) when CMAKE_SOURCE_DIR is on the compiler include path.
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION.txt" KOMAI_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${KOMAI_VERSION_STRING}" KOMAI_VERSION_STRING)
string(REPLACE "." ";" KOMAI_VERSION_PARTS "${KOMAI_VERSION_STRING}")
list(GET KOMAI_VERSION_PARTS 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET KOMAI_VERSION_PARTS 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET KOMAI_VERSION_PARTS 2 CPACK_PACKAGE_VERSION_PATCH)
list(GET KOMAI_VERSION_PARTS 3 CPACK_PACKAGE_VERSION_TWEAK)
set(PROJECT_VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH})
set(PROJECT_VERSION_TWEAK ${CPACK_PACKAGE_VERSION_TWEAK})

set(KOMAI_DESKTOP_ID "cc.etke.komai")
set(KOMAI_DESKTOP_ICON_NAME "${KOMAI_DESKTOP_ID}")
set(KOMAI_STARTUP_WM_CLASS "komai")
if(FLATPAK)
    set(KOMAI_EXECUTABLE_NAME "${KOMAI_DESKTOP_ID}")
else()
    set(KOMAI_EXECUTABLE_NAME "komai")
endif()

set(KOMAI_LOGO_SOURCE_SVG "${CMAKE_CURRENT_SOURCE_DIR}/resources/komai.svg")
set(KOMAI_LOGO_ASSET_DIR "${CMAKE_CURRENT_BINARY_DIR}/logo-assets")
set(RESOURCE_FOLDER "${KOMAI_LOGO_ASSET_DIR}")
set(KOMAI_LOGO_GENERATED_META_FILES
    "${KOMAI_LOGO_ASSET_DIR}/komai.ico"
    "${KOMAI_LOGO_ASSET_DIR}/komai.icns")
set_source_files_properties(${KOMAI_LOGO_GENERATED_META_FILES} PROPERTIES GENERATED TRUE)

# Ensure PROJECT_VERSION_PATCH & PROJECT_VERSION_TWEAK default to 0 if somehow unset.
# Needed by add_project_meta (Windows RC, macOS bundle).
fix_project_version()

# Set additional project information
set(COMPANY "Komai")
set(COPYRIGHT "Copyright (c) 2024 Komai Contributors")
set(IDENTIFIER "com.etkecc.komai")

add_project_meta(META_FILES_TO_INCLUDE)

if(NOT MSVC AND NOT APPLE)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

if(BUILD_DOCS)
    find_package(Doxygen)

    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT})

        add_custom_target(docs ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM )
    else(DOXYGEN_FOUND)
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif(DOXYGEN_FOUND)
endif()

#
# Dependencies managed by CPM
#

# PkgConfig is only consumed by the VOIP block (gstreamer
# pkg_check_modules) and the X11 block (xcb pkg_check_modules) further
# below; nothing else in the tree calls into it. Gating the find_package
# on those flags keeps Windows, macOS, and Linux -DVOIP=OFF builds from
# requiring a pkg-config tool they never end up invoking.
if(VOIP OR X11)
    find_package(PkgConfig REQUIRED)
endif()

#
# Discover Qt dependencies.
#

# Suppress the per-Configure CMake warning Qt prints whenever a target
# pulls a Qt6*Private module. The warning's purpose is to remind that
# private-module use ties our binary to the exact Qt build it was
# compiled against; we already accepted that trade-off when we adopted
# matrix-sdk's GuiPrivate / QmlPrivate dependency, so the recurring
# nag is pure CI noise.
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 6.5 COMPONENTS Core Widgets Gui LinguistTools Svg Multimedia Qml QuickControls2 Network REQUIRED)
if (Qt6Qml_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QmlPrivate)
endif()
if(ELEMENT_CALL)
    # WebEngineQuick hosts the Element Call web bundle. The QML import is
    # "QtWebEngine"; the link target is Qt::WebEngineQuick.
    #
    # WebChannelQuick is the transport for the window.postMessage bridge: the
    # injected page script forwards widget->host Widget API messages through a
    # QWebChannel to ElementCallWidgetSession (we do NOT use QWebChannel's RPC
    # model with Element Call itself, only as a dumb JS->C++ pipe). The QML
    # import is "QtWebChannel"; the link target is Qt::WebChannelQuick.
    find_package(Qt6 6.5 COMPONENTS WebEngineQuick WebChannelQuick REQUIRED)
endif()
find_package(Qt6DBus)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(BUILD_TEST_APPLICATION OFF CACHE INTERNAL "")

set(KOMAI_LOGO_GENERATED_FILES
    "${KOMAI_LOGO_ASSET_DIR}/komai-16.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-32.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-44.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-48.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-64.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-128.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-150.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-256.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai-512.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai.png"
    "${KOMAI_LOGO_ASSET_DIR}/komai.svg.sha256")
set(KOMAI_LOGO_GENERATOR_ARGS
    --input "${KOMAI_LOGO_SOURCE_SVG}"
    --output-dir "${KOMAI_LOGO_ASSET_DIR}")
if(WIN32)
    list(APPEND KOMAI_LOGO_GENERATED_FILES "${KOMAI_LOGO_ASSET_DIR}/komai.ico")
    list(APPEND KOMAI_LOGO_GENERATOR_ARGS --with-ico)
endif()
if(APPLE)
    list(APPEND KOMAI_LOGO_GENERATED_FILES
        "${KOMAI_LOGO_ASSET_DIR}/komai.icns"
        "${KOMAI_LOGO_ASSET_DIR}/komai-dmg-background.png"
        "${KOMAI_LOGO_ASSET_DIR}/komai-dmg-background@2x.png")
    list(APPEND KOMAI_LOGO_GENERATOR_ARGS --with-icns --with-dmg-background)
endif()
# Windows builds keep qtkeychain's legacy bare-key Credential Manager naming.
# The service-qualified default ("<key>@<service>") cannot read entries stored
# by existing installs, which would sign every Windows user out and strand
# their encrypted local store. Komai keys are fully namespaced on their own
# ("komai.<tag>.<profile>..."), so the legacy scheme loses nothing. Only flip
# this together with a migration for already-stored credentials.
set(USE_COMPAT_NAMING_SCHEME ON CACHE BOOL INTERNAL)
CPMAddPackage(
    NAME qt6keychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG 0.17.0
    # The system QtKeychain config file is installed as Qt6KeychainConfig.cmake,
    # so the lowercase package name we use here doesn't match find_package's
    # default lookup. NAMES forces find_package into Config mode and points at
    # the correct file, letting distro packagers reuse the system package via
    # -DCPM_USE_LOCAL_PACKAGES=ON. EXCLUDE_FROM_ALL keeps the bundled fallback
    # (when CPM does fetch + build) out of `cmake --install`.
    FIND_PACKAGE_ARGUMENTS "NAMES Qt6Keychain"
    EXCLUDE_FROM_ALL YES
)

set(KDSingleApplication_QT6 ON CACHE BOOL INTERNAL)
set(KDSingleApplication_STATIC ON CACHE BOOL INTERNAL)
set(KDSingleApplication_EXAMPLES OFF CACHE BOOL INTERNAL)
CPMAddPackage(
    NAME kdsingleapplication
    GIT_REPOSITORY https://github.com/KDAB/KDSingleApplication.git
    GIT_TAG v1.2.1
    # System config file is KDSingleApplication-qt6Config.cmake; see qt6keychain
    # above for the rationale.
    FIND_PACKAGE_ARGUMENTS "NAMES KDSingleApplication-qt6"
    EXCLUDE_FROM_ALL YES
)

if(Qt6Widgets_FOUND)
    if(Qt6Widgets_VERSION VERSION_LESS 6.5.0)
        message(STATUS "Qt version ${Qt6Widgets_VERSION}")
        message(WARNING "Minimum supported Qt6 version is 6.5!")
    endif()
endif(Qt6Widgets_FOUND)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
if(NOT MSVC)
    set(
        CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} \
        -Wall \
        -Wextra \
        -pedantic \
        -fsized-deallocation \
        -fdiagnostics-color=always \
        -Wunreachable-code \
        -Wno-attributes \
        -Wno-error=unused-parameter"
    )
    if(NOT CMAKE_COMPILER_IS_GNUCXX)
        # -Wshadow is buggy and broken in GCC, so do not enable it.
        # see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=79328
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
    endif()
endif()

if(MSVC)
    set(
        CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} /bigobj"
    )
endif()

if(NOT (CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES))
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
    message("Setting build type to '${CMAKE_BUILD_TYPE}'")
else(NOT (CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES))
    message("Build type set to '${CMAKE_BUILD_TYPE}'")
endif(NOT (CMAKE_BUILD_TYPE OR CMAKE_CONFIGURATION_TYPES))

set(KOMAI_ENABLE_DEBUG_LOG false)

# Windows doesn't handle CMAKE_BUILD_TYPE.
if(NOT WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(KOMAI_ENABLE_DEBUG_LOG true)
endif()

# [komai] Git hash suffix removed — version is read from the top-level VERSION.txt file.

set(CPACK_PACKAGE_VERSION "${KOMAI_VERSION_STRING}")
set(PROJECT_VERSION "${KOMAI_VERSION_STRING}")

message(STATUS "Version: ${PROJECT_VERSION}")

# Get git commit hash for version info. Packaging recipes (flatpak,
# appimage, snap) build in environments where `git rev-parse` can't
# reach the repo — sources are copied in without `.git`, or with
# ownership mismatches that trip git's safe.directory check. Those
# recipes write `.git-commit-hash` in the source root on the host
# before launching the sandboxed build; CMake reads it as a fallback.
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH AND EXISTS "${CMAKE_SOURCE_DIR}/.git-commit-hash")
    file(READ "${CMAKE_SOURCE_DIR}/.git-commit-hash" GIT_COMMIT_HASH)
    string(STRIP "${GIT_COMMIT_HASH}" GIT_COMMIT_HASH)
endif()
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")

# Several values displayed by Komai (app version, About-tab acknowledgements)
# are parsed from these files at configure time and baked into config/komai.h.
# CMake won't re-run configure on its own when an arbitrary file changes, so
# register them as configure dependencies — otherwise a `just build` after a
# version/pin bump silently keeps the old values until something else
# triggers reconfigure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/VERSION.txt"
    "${CMAKE_SOURCE_DIR}/rust-toolchain.toml"
    "${CMAKE_SOURCE_DIR}/bin/icons/fluent/VERSION_REF"
    "${CMAKE_SOURCE_DIR}/bin/icons/fontawesome/VERSION_REF"
    "${CMAKE_SOURCE_DIR}/bin/element-call/sources.lock.yml"
    "${CMAKE_SOURCE_DIR}/src/rust/Cargo.toml"
)

# Read pinned Fluent icon source ref (used by About acknowledgements)
set(FLUENT_ICONS_VERSION "unknown")
set(FLUENT_VERSION_REF_FILE "${CMAKE_SOURCE_DIR}/bin/icons/fluent/VERSION_REF")
if(EXISTS "${FLUENT_VERSION_REF_FILE}")
    file(STRINGS "${FLUENT_VERSION_REF_FILE}" FLUENT_VERSION_REF_LINES)
    foreach(_line IN LISTS FLUENT_VERSION_REF_LINES)
        string(STRIP "${_line}" _line_stripped)
        if(_line_stripped STREQUAL "" OR _line_stripped MATCHES "^#")
            continue()
        endif()
        set(FLUENT_ICONS_VERSION "${_line_stripped}")
        break()
    endforeach()
endif()
message(STATUS "Fluent icons ref: ${FLUENT_ICONS_VERSION}")

# Read pinned Font Awesome icon source ref (used by About acknowledgements)
set(FONTAWESOME_ICONS_VERSION "unknown")
set(FONTAWESOME_VERSION_REF_FILE "${CMAKE_SOURCE_DIR}/bin/icons/fontawesome/VERSION_REF")
if(EXISTS "${FONTAWESOME_VERSION_REF_FILE}")
    file(STRINGS "${FONTAWESOME_VERSION_REF_FILE}" FONTAWESOME_VERSION_REF_LINES)
    foreach(_line IN LISTS FONTAWESOME_VERSION_REF_LINES)
        string(STRIP "${_line}" _line_stripped)
        if(_line_stripped STREQUAL "" OR _line_stripped MATCHES "^#")
            continue()
        endif()
        set(FONTAWESOME_ICONS_VERSION "${_line_stripped}")
        break()
    endforeach()
endif()
message(STATUS "Font Awesome icons ref: ${FONTAWESOME_ICONS_VERSION}")

# Read pinned matrix-rust-sdk ref from src/rust/Cargo.toml (used by About
# acknowledgements). Supports both git-pinning (`rev = "..."`) and crates.io
# versioning (`version = "X.Y.Z"` or bare `"X.Y.Z"`). Exactly one of
# MATRIX_SDK_REV / MATRIX_SDK_VERSION is non-empty in practice; the C++ side
# picks the link target accordingly.
set(MATRIX_SDK_REV "")
set(MATRIX_SDK_VERSION "")
set(MATRIX_SDK_CARGO_TOML "${CMAKE_SOURCE_DIR}/src/rust/Cargo.toml")
if(EXISTS "${MATRIX_SDK_CARGO_TOML}")
    file(STRINGS "${MATRIX_SDK_CARGO_TOML}" _matrix_sdk_lines REGEX "^matrix-sdk[ \t]*=")
    foreach(_line IN LISTS _matrix_sdk_lines)
        if(_line MATCHES "rev[ \t]*=[ \t]*\"([0-9a-fA-F]+)\"")
            set(MATRIX_SDK_REV "${CMAKE_MATCH_1}")
            break()
        elseif(_line MATCHES "version[ \t]*=[ \t]*\"([^\"]+)\"")
            set(MATRIX_SDK_VERSION "${CMAKE_MATCH_1}")
            break()
        elseif(_line MATCHES "^matrix-sdk[ \t]*=[ \t]*\"([^\"]+)\"")
            set(MATRIX_SDK_VERSION "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
endif()
if(MATRIX_SDK_REV)
    message(STATUS "matrix-sdk pinned to git rev: ${MATRIX_SDK_REV}")
elseif(MATRIX_SDK_VERSION)
    message(STATUS "matrix-sdk pinned to version: ${MATRIX_SDK_VERSION}")
else()
    message(WARNING "Could not parse matrix-sdk pin from ${MATRIX_SDK_CARGO_TOML}")
endif()

# Read pinned Element Call version from bin/element-call/sources.lock.yml (used
# by About acknowledgements). The pin is a quoted YAML value
# (`version: "X.Y.Z"`); the unquoted `version: 1` schema line does not match.
set(ELEMENT_CALL_VERSION "unknown")
set(ELEMENT_CALL_LOCK_FILE "${CMAKE_SOURCE_DIR}/bin/element-call/sources.lock.yml")
if(EXISTS "${ELEMENT_CALL_LOCK_FILE}")
    file(STRINGS "${ELEMENT_CALL_LOCK_FILE}" _element_call_lines REGEX "version[ \t]*:[ \t]*\"[^\"]+\"")
    foreach(_line IN LISTS _element_call_lines)
        if(_line MATCHES "version[ \t]*:[ \t]*\"([^\"]+)\"")
            set(ELEMENT_CALL_VERSION "${CMAKE_MATCH_1}")
            break()
        endif()
    endforeach()
endif()
message(STATUS "Element Call version: ${ELEMENT_CALL_VERSION}")

cmake_host_system_information(RESULT BUILD_HOST QUERY HOSTNAME)

include(CheckSymbolExists)
check_symbol_exists(backtrace_symbols_fd "execinfo.h" HAVE_BACKTRACE_SYMBOLS_FD)

configure_file(cmake/komai.h config/komai.h)

#
# Declare source and header files.
#
set(SRC_FILES
    # Emoji
    src/emoji/Provider.cpp
    src/emoji/Provider.h
    src/emoji/Emoji.cpp
    src/emoji/Emoji.h
    src/emoji/EmoticonReplace.cpp
    src/emoji/EmoticonReplace.h
    src/emoji/EmojiNormalize.cpp
    src/emoji/EmojiNormalize.h

    # Timeline
    src/timeline/CommunitiesModel.cpp
    src/timeline/CommunitiesModel.h
    src/timeline/communities/CommunitiesModelData.cpp
    src/timeline/DelegateChooser.cpp
    src/timeline/DelegateChooser.h
    src/timeline/EventDataSource.cpp
    src/timeline/EventDataSource.h
    src/timeline/EventDelegateChooser.cpp
    src/timeline/EventDelegateChooser.h
    src/timeline/SlashCommands.cpp
    src/timeline/SlashCommands.h
    src/timeline/RoomRoles.h
    src/timeline/formattedcode/RawJsonFormatter.cpp
    src/timeline/formattedcode/RawJsonFormatter.h
    src/timeline/litehtml/LitehtmlContainer.cpp
    src/timeline/litehtml/LitehtmlContainer.h
    src/timeline/litehtml/LitehtmlItem.cpp
    src/timeline/litehtml/LitehtmlItem.h
    src/timeline/litehtml/LitehtmlStylesheet.cpp
    src/timeline/litehtml/LitehtmlStylesheet.h
    src/timeline/data/RoomlistModelData.cpp
    src/timeline/roomlist/FilteredRoomlistModel.cpp
    src/timeline/roomlist/RoomlistModelPreviews.cpp
    src/timeline/roomlist/RoomlistPreviewSelection.cpp
    src/timeline/roomlist/RoomlistModelSelection.cpp
    src/timeline/rust/MatrixTimelineModel.cpp
    src/timeline/rust/MatrixTimelineModel.h
    src/timeline/rust/TimelineFilter.cpp
    src/timeline/rust/TimelineFilter.h
    src/timeline/DirectChatResolver.cpp
    src/timeline/DirectChatResolver.h
    src/timeline/Permissions.cpp
    src/timeline/Permissions.h
    src/timeline/PresenceEmitter.cpp
    src/timeline/PresenceEmitter.h
    src/timeline/Reaction.cpp
    src/timeline/Reaction.h
    src/timeline/RoomlistModel.cpp
    src/timeline/RoomlistModel.h
    src/timeline/StateEventText.cpp
    src/timeline/StateEventText.h
    src/timeline/TimelineEventTypes.cpp
    src/timeline/TimelineEventTypes.h
    src/timeline/NavigationHistory.cpp
    src/timeline/NavigationHistory.h
    src/timeline/TimelineViewManager.cpp
    src/timeline/TimelineViewManager.h
    src/timeline/view/TimelineViewManagerColors.cpp
    src/timeline/view/TimelineViewManagerCompletions.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineAttachments.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineBackend.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineCompose.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineEvents.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineForward.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineInternal.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineInternal.h
    src/timeline/view/TimelineViewManagerMatrixTimelineLifecycle.cpp
    src/timeline/view/TimelineViewManagerMatrixTimelineThreads.cpp
    src/timeline/view/TimelineViewManagerMedia.cpp
    src/timeline/view/TimelineViewManagerRoomActions.cpp
    src/timeline/view/TimelineViewManagerSlashCommands.cpp

    # UI components
    src/ui/HiddenEvents.cpp
    src/ui/HiddenEvents.h
    src/ui/MediaDownloadProgressWatcher.cpp
    src/ui/MediaDownloadProgressWatcher.h
    src/ui/MxcAnimatedImage.cpp
    src/ui/MxcAnimatedImage.h
    src/ui/MxcMediaProxy.cpp
    src/ui/MxcMediaProxy.h
    src/ui/KomaiCursorShape.cpp
    src/ui/KomaiCursorShape.h
    src/ui/KomaiDropArea.cpp
    src/ui/KomaiDropArea.h
    src/ui/KomaiGlobalObject.cpp
    src/ui/KomaiGlobalObject.h
    src/ui/LayoutAgnosticKeys.cpp
    src/ui/LayoutAgnosticKeys.h
    src/ui/KomaiMenuVisibilityFilter.h
    src/ui/KomaiMenuVisibilityFilter.cpp
    src/ui/RoomSettings.cpp
    src/ui/roomsettings/RoomSettingsAllowedRoomsModel.cpp
    src/ui/roomsettings/RoomSettingsAvatar.cpp
    src/ui/roomsettings/RoomSettingsAccess.cpp
    src/ui/RoomSettings.h
    src/ui/RoomSummary.cpp
    src/ui/RoomSummary.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/ThemeDef.h
    src/ui/ThemeRegistry.cpp
    src/ui/ThemeRegistry.h
    src/ui/UserProfile.cpp
    src/ui/UserProfileActions.cpp
    src/ui/UserProfileModels.cpp
    src/ui/UserProfile.h

    src/voip/CallDevices.cpp
    src/voip/CallDevices.h
    src/voip/CallManager.cpp
    src/voip/callmanager/CallManagerScreenShare.cpp
    src/voip/CallManager.h
    # Always-compiled app-global coordinator for the Element Call surface. The
    # QtWebEngine-backed panel/session it hands off to are ELEMENT_CALL-gated
    # (KOMAI_QML_MODULE_SOURCES below), but this thin QML singleton is always
    # present so the composer button can reference it on every build.
    src/voip/ElementCallController.cpp
    src/voip/ElementCallController.h
    src/voip/ScreenCastPortal.cpp
    src/voip/ScreenCastPortal.h
    src/voip/WebRTCSession.h
    src/voip/Transcription.cpp
    src/voip/Transcription.h
    src/voip/TranscriptionAudioCapture.cpp
    src/voip/TranscriptionAudioCapture.h
    src/voip/VoiceRecorder.cpp
    src/voip/VoiceRecorder.h
    src/voip/WebRTCSession.cpp
    src/voip/WebRTCSession.h

    # Spell checking
    src/spellcheck/SpellCheckEngine.cpp
    src/spellcheck/SpellCheckEngine.h
    src/spellcheck/SpellChecker.cpp
    src/spellcheck/SpellChecker.h

    src/encryption/DeviceVerificationFlow.cpp
    src/encryption/DeviceVerificationFlowHandlers.cpp
    src/encryption/DeviceVerificationFlowProtocol.cpp
    src/encryption/CryptoTrust.h
    src/encryption/DeviceVerificationFlow.h
    src/encryption/EncryptionKeyExport.cpp
    src/encryption/EncryptionKeyExport.h
    src/encryption/SelfVerificationStatus.cpp
    src/encryption/SelfVerificationStatus.h
    src/encryption/VerificationManager.cpp
    src/encryption/VerificationManager.h

    # Generic notification stuff
    src/notifications/Manager.cpp
    src/notifications/Manager.h

    src/dock/Dock.cpp
    src/dock/Dock.h

    src/models/AliasEditModel.cpp
    src/models/AliasEditModel.h
    src/providers/AvatarProvider.cpp
    src/providers/AvatarProvider.h
    src/providers/BlurhashProvider.cpp
    src/providers/BlurhashProvider.h
    src/chat/ChatPageRoomActions.cpp
    src/chat/ChatPageSessionLifecycle.cpp
    src/chat/ChatPageCore.cpp
    src/chat/ChatPageBootstrap.cpp
    src/chat/ChatPage.h
    src/ui/Clipboard.cpp
    src/ui/Clipboard.h
    src/providers/ColorImageProvider.cpp
    src/providers/ColorImageProvider.h
    src/imagepacks/CombinedImagePackModel.cpp
    src/imagepacks/CombinedImagePackModel.h
    src/imagepacks/GridImagePackModel.cpp
    src/imagepacks/GridImagePackModel.h
    src/models/CommandCompleter.cpp
    src/models/CommandCompleter.h
    src/models/CompletionModelRoles.h
    src/models/CompletionProxyModel.cpp
    src/models/CompletionProxyModel.h
    src/matrix/MatrixStateTypes.h
    src/imagepacks/ImagePackListModel.cpp
    src/imagepacks/ImagePackListModel.h
    src/models/InviteesModel.cpp
    src/models/InviteesModel.h
    src/avatars/default/BoringAvatars.h
    src/avatars/default/BoringAvatarsBeam.cpp
    src/avatars/default/BoringAvatarsMarble.cpp
    src/avatars/default/BoringAvatarsBauhaus.cpp
    src/avatars/default/LetterInitialGenerator.h
    src/avatars/default/LetterInitialGenerator.cpp
    src/avatars/default/UserIconGenerator.h
    src/avatars/default/UserIconGenerator.cpp
    src/avatars/default/DefaultAvatarProvider.h
    src/avatars/default/DefaultAvatarProvider.cpp
    src/logging/Logging.cpp
    src/logging/Logging.h
    src/auth/LoginPage.cpp
    src/auth/LoginPageDiscovery.cpp
    src/auth/LoginPage.h
    src/auth/RegisterPage.cpp
    src/auth/RegisterPage.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/NotificationAction.h
    src/matrix/backend/MatrixAuthService.cpp
    src/matrix/backend/MatrixAuthService.h
    src/matrix/backend/MatrixRegistrationService.cpp
    src/matrix/backend/MatrixRegistrationService.h
    src/matrix/backend/MatrixBackendBridge.cpp
    src/matrix/backend/MatrixBackendBridge.h
    src/matrix/backend/MatrixBackendRuntimeService.h
    src/matrix/backend/MatrixBackendRuntimeServiceCore.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceEncryption.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceInternal.h
    src/matrix/backend/MatrixBackendRuntimeServiceMedia.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceMessaging.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceProfile.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceRooms.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceTimeline.cpp
    src/matrix/backend/MatrixBackendRuntimeServiceTypes.h
    src/matrix/backend/MatrixSessionSecrets.cpp
    src/matrix/backend/MatrixSessionSecrets.h
    src/matrix/backend/MatrixSdkPaths.cpp
    src/matrix/backend/MatrixSdkPaths.h
    src/matrix/MatrixServerResolver.cpp
    src/matrix/MatrixServerResolver.h
    src/models/MemberList.cpp
    src/models/MemberList.h
    src/providers/MxcImageProvider.cpp
    src/providers/MxcImageProvider.h
    src/profile/KeyringEnvironment.cpp
    src/profile/KeyringEnvironment.h
    src/profile/Paths.cpp
    src/profile/Paths.h
    src/profile/ProfileManager.cpp
    src/profile/ProfileManager.h
    src/settings/SettingKeys.h
    src/settings/ui/LanguagePresentation.h
    src/settings/ui/LanguagePresentation.cpp
    src/settings/ui/SettingDescriptor.h
    src/settings/ui/SettingDescriptor.cpp
    src/settings/ui/SettingDescriptorTable.cpp
    src/settings/ui/SettingRoleData.h
    src/settings/ui/SettingRoleData.cpp
    src/settings/ui/SettingInputValidation.h
    src/settings/ui/SettingInputValidation.cpp
    src/settings/StartupSettings.h
    src/settings/StartupSettings.cpp
    src/settings/core/SettingDefinition.h
    src/settings/core/StartupConfig.h
    src/settings/core/StartupConfig.cpp
    src/settings/core/SettingsStore.h
    src/settings/core/SettingsStore.cpp
    src/settings/SettingsStorage.cpp
    src/settings/SettingsStoragePathApi.cpp
    src/settings/SettingsStorageReaderWriterFilesystem.cpp
    src/settings/SettingsStorageReaderWriterInMemory.cpp
    src/settings/SettingsStorageInternal.h
    src/settings/SettingsStorageSecureBackend.cpp
    src/settings/SettingsStorage.h
    src/settings/SettingsSerializerConfigConverters.cpp
    src/settings/SettingsSerializerConfigEnumTokenAdapters.cpp
    src/settings/SettingsSerializerConfigSchema.cpp
    src/settings/SettingsSerializerConfigSchemaValidation.cpp
    src/settings/SettingsSerializerConfig.cpp
    src/settings/SettingsSerializerConfigLoad.cpp
    src/settings/SettingsSerializerConfigSave.cpp
    src/settings/SettingsSerializerSession.cpp
    src/settings/SettingsSerializerState.cpp
    src/settings/SettingsSerializer.h
    src/settings/SettingsController.h
    src/settings/SettingsController.cpp
    src/settings/SettingsControllerLoad.cpp
    src/settings/SettingsControllerSave.cpp
    src/settings/SettingsControllerInternal.h
    src/powerlevels/PowerlevelEditingModels.cpp
    src/powerlevels/PowerlevelsEditModels.h
    src/powerlevels/PowerlevelsSpacesListModel.cpp
    src/powerlevels/PowerlevelsTypeListModel.cpp
    src/powerlevels/PowerlevelsUserListModel.cpp
    src/profile/ProfileSecrets.cpp
    src/profile/ProfileSecrets.h
    src/models/ReadReceiptsModel.cpp
    src/models/ReadReceiptsModel.h
    src/models/RoomDirectoryModel.cpp
    src/models/RoomDirectoryModel.h
    src/models/RoomsModel.cpp
    src/models/RoomsModel.h
    src/auth/SSOHandler.cpp
    src/auth/SSOHandler.h
    src/imagepacks/SingleImagePackModel.cpp
    src/imagepacks/SingleImagePackModel.h
    src/ui/TrayIcon.cpp
    src/ui/TrayIcon.h
    src/settings/ui/UserSettingsModel.h
    src/settings/ui/UserSettingsModel.cpp
    src/settings/ui/UserSettingsModelData.cpp
    src/settings/ui/UserSettingsModelConnections.cpp
    src/settings/ui/facade/UserSettingsPage.cpp
    src/settings/ui/facade/UserSettingsGettersUi.cpp
    src/settings/ui/facade/UserSettingsGettersTimelineComposer.cpp
    src/settings/ui/facade/UserSettingsGettersLayout.cpp
    src/settings/ui/facade/UserSettingsGettersCalls.cpp
    src/settings/ui/facade/UserSettingsGettersEncryption.cpp
    src/settings/ui/facade/UserSettingsGettersMisc.cpp
    src/settings/ui/facade/UserSettingsPage.h
    src/settings/ui/facade/UserSettingsSettersCore.cpp
    src/settings/ui/facade/UserSettingsSettersLayout.cpp
    src/settings/ui/facade/UserSettingsSettersUi.cpp
    src/settings/ui/facade/UserSettingsSettersMisc.cpp
    src/settings/ui/facade/UserSettingsCoreStoreBridge.cpp
    src/settings/ui/facade/UserSettingsLanguage.cpp
    src/settings/ui/facade/UserSettingsSessionSettings.cpp
    src/settings/ui/facade/UserSettingsTheme.cpp
    src/models/UsersModel.cpp
    src/models/UsersModel.h
    src/utils/BotDetection.cpp
    src/utils/UtilsCore.cpp
    src/utils/Utils.h
    src/utils/UtilsColor.cpp
    src/utils/UtilsMessageDescription.cpp
    src/utils/UtilsReveal.cpp
    src/utils/UtilsRoomMaintenance.cpp
    src/utils/UtilsTextAndUri.cpp


    # IPC (shared logic + local socket server)
    src/ipc/SharedLogic.h
    src/ipc/SharedLogic.cpp
    src/ipc/IpcServer.h
    src/ipc/IpcServer.cpp

    # CLI subcommands
    src/cli/IpcClient.h
    src/cli/CliDispatch.cpp
    src/cli/CliDispatch.h
    src/cli/schema/Dispatcher.cpp
    src/cli/schema/Dispatcher.h
    src/cli/schema/HelpRenderer.cpp
    src/cli/schema/HelpRenderer.h
    src/cli/schema/SchemaTypes.h
    src/cli/AppCommands.cpp
    src/cli/AppCommands.h
    src/cli/CompletionsCommands.cpp
    src/cli/CompletionsCommands.h
    src/cli/MediaCommands.cpp
    src/cli/MediaCommands.h
    src/cli/McpCommands.cpp
    src/cli/McpCommands.h
    src/cli/ProfileCommands.cpp
    src/cli/ProfileCommands.h
    src/cli/RoomCommands.cpp
    src/cli/RoomCommands.h
    src/cli/SettingsCommands.cpp
    src/cli/SettingsCommands.h
    src/cli/UserCommands.cpp
    src/cli/UserCommands.h
    src/cli/ThemeColorUtils.cpp
    src/cli/ThemeColorUtils.h
    src/cli/ThemeCommands.cpp
    src/cli/ThemeCommands.h
    src/cli/theme/ThemeCommandUtils.cpp
    src/cli/theme/ThemeCommandUtils.h
    src/cli/theme/ThemeCommandHandlers.h
    src/cli/theme/ThemeCommandsTinted.cpp
    src/cli/theme/ThemeCommandsGeneral.cpp

    src/app/MainApplication.cpp
    src/app/MainApplication.h
    src/app/MainApplicationSupport.cpp
    src/app/MainApplicationSupport.h
    src/main.cpp
)

include(FeatureSummary)

set(BUILD_LIB_EXAMPLES OFF CACHE INTERNAL "")
set(BUILD_LIB_TESTS OFF CACHE INTERNAL "")

# litehtml is always built from our pinned source, never the system package —
# even under -DCPM_USE_LOCAL_PACKAGES=ON (which still applies to every other
# dependency). litehtml's document_container is a pure-virtual interface that
# LitehtmlContainer subclasses, and it changes shape between releases (v0.10
# reworked create_font/draw_background/get_client_rect, added on_mouse_event,
# and dropped document::content_width). Our C++ can only match one version at a
# time, so letting a rolling distro substitute its own litehtml would silently
# break the build the moment its package version diverged from ours. Pinning
# the GIT_TAG here keeps the API we compile against deterministic everywhere
# (native, CI, Flatpak, AppImage, Snap, and the Arch package).
CPMAddPackage(
    NAME litehtml
    GIT_REPOSITORY https://github.com/litehtml/litehtml.git
    GIT_TAG v0.10
    OPTIONS "LITEHTML_BUILD_TESTING OFF"
    # EXCLUDE_FROM_ALL keeps the bundled litehtml + gumbo headers, CMake
    # configs, and static archives out of `cmake --install` so distro
    # packagers don't ship them — only the static code linked into komai.
    EXCLUDE_FROM_ALL YES
)
if(litehtml_ADDED)
    # Suppress compiler warnings from litehtml's own sources and headers
    target_compile_options(litehtml PRIVATE -w)
    target_compile_options(gumbo PRIVATE -w)
    get_target_property(_litehtml_inc litehtml INTERFACE_INCLUDE_DIRECTORIES)
    if(_litehtml_inc)
        set_target_properties(litehtml PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_litehtml_inc}")
    endif()
endif()

# Rust integration via Corrosion (matrix-sdk backend, resolvematrix, and supporting crates)
#
# Corrosion's FindRust does not read rust-toolchain.toml — it picks the default
# rustup toolchain. We parse the channel ourselves so the version is defined in
# one place (rust-toolchain.toml) and Corrosion uses the same one.
#
# Distro packagers (e.g. the AUR PKGBUILD) can opt out of the pin by removing
# rust-toolchain.toml in their packaging step. When the file is absent we skip
# this block and Corrosion falls back to whichever rustc/cargo is on PATH. The
# removal is also what prevents rustup's shim from silently auto-installing
# the pinned channel during cargo invocations later in the build.
if(NOT DEFINED Rust_TOOLCHAIN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/rust-toolchain.toml" _rust_toolchain_toml)
    string(REGEX MATCH "channel *= *\"([^\"]+)\"" _rust_channel_match "${_rust_toolchain_toml}")
    if(NOT CMAKE_MATCH_1)
        message(FATAL_ERROR "Could not parse channel from rust-toolchain.toml")
    endif()
    set(_rust_channel "${CMAKE_MATCH_1}")

    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(_rust_host_arch "aarch64")
    elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(_rust_host_arch "x86_64")
    else()
        set(_rust_host_arch "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
    if(CMAKE_HOST_WIN32)
        # MSVC ABI; MinGW would be -pc-windows-gnu, which we don't support.
        set(_rust_host_os "pc-windows-msvc")
    elseif(CMAKE_HOST_APPLE)
        set(_rust_host_os "apple-darwin")
    else()
        set(_rust_host_os "unknown-linux-gnu")
    endif()
    set(_rust_host_triple "${_rust_host_arch}-${_rust_host_os}")
    set(Rust_TOOLCHAIN "${_rust_channel}-${_rust_host_triple}" CACHE STRING
        "Rust toolchain used by Corrosion"
    )
endif()
CPMAddPackage(
    NAME Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.6.1
    # No system Corrosion package on Arch (or most distros), so this always
    # falls through to fetch + build. EXCLUDE_FROM_ALL keeps Corrosion's own
    # install rules out of `cmake --install` so distro packagers don't ship
    # them under /usr alongside komai.
    EXCLUDE_FROM_ALL YES
)
corrosion_import_crate(MANIFEST_PATH src/rust/Cargo.toml)
corrosion_add_cxxbridge(komai-rust-cxxbridge
    CRATE komai_rust
    FILES ffi.rs
)
target_include_directories(komai-rust-cxxbridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# GCC's -Wmaybe-uninitialized is a known false-positive against the
# Vec<T>::Vec() constructor pattern that cxxbridge emits in the
# generated ffi.cpp (clang and MSVC don't trip). Each `#[cxx::bridge]`
# entry compiles as its own warning instance, so we get ~30 hits in
# every CI Build step — pure noise drowning out warnings we'd actually
# want to see. Scope-limit the suppression to this generated target so
# our own C++ keeps -Wmaybe-uninitialized fully active.
target_compile_options(komai-rust-cxxbridge PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>)

if(VOIP)
    include(FindPkgConfig)
    # gstreamer-allocators-1.0 is listed explicitly because gstreamer-video
    # declares it as Requires.private, so pkg-config omits it from link flags.
    # Without it the linker cannot resolve gst_udmabuf_allocator_get.
    pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-allocators-1.0 gstreamer-rtp-1.0>=1.18 gstreamer-sdp-1.0>=1.18 gstreamer-webrtc-1.0>=1.18 gstreamer-gl-1.0)
    if (WIN32)
        pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-d3d11-1.0)
    endif()
endif()

if(X11 AND NOT WIN32 AND NOT APPLE AND NOT HAIKU)
    pkg_check_modules(XCB REQUIRED IMPORTED_TARGET xcb xcb-ewmh)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

# this must be defined here to make the moc work properly
if(NOT APPLE AND NOT WIN32)
    add_compile_definitions(KOMAI_DBUS_SYS)
endif()

#
# Bundle resources
#
qt_add_resources(QRC resources/res.qrc)

if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework Foundation -framework Cocoa -framework UserNotifications")
    set(SRC_FILES ${SRC_FILES} src/notifications/NotificationManagerProxy.h src/notifications/MacNotificationDelegate.h src/notifications/MacNotificationDelegate.mm src/notifications/ManagerMac.mm src/notifications/ManagerMac.cpp src/notifications/MacReopenHandler.h src/notifications/MacReopenHandler.mm)
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
        set_source_files_properties( src/notifications/NotificationManagerProxy.h src/notifications/MacNotificationDelegate.h src/notifications/MacNotificationDelegate.mm src/notifications/ManagerMac.mm src/notifications/MacReopenHandler.h src/notifications/MacReopenHandler.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
elseif(WIN32)
    CPMAddPackage(
        NAME WinToast
        GIT_REPOSITORY https://github.com/mohabouje/WinToast.git
        GIT_TAG v1.3.2
        DOWNLOAD_ONLY YES
    )

    set(SRC_FILES ${SRC_FILES} src/notifications/ManagerWin.cpp ${WinToast_SOURCE_DIR}/src/wintoastlib.cpp)
else()
    set(SRC_FILES ${SRC_FILES}
        src/dbus/Api.h
        src/dbus/Backend.h
        src/dbus/Api.cpp
        src/dbus/Backend.cpp
        src/notifications/ManagerLinux.cpp
    )
endif()

# Generate derived logo assets with a tiny Qt-based helper instead of external
# tools like ImageMagick/rsvg-convert. This keeps the normal build,
# distro packaging, and CI environments free of extra raster-tool deps.
add_executable(komai_logo_asset_generator etc/tools/logo-assets/LogoAssetGenerator.cpp)
target_link_libraries(komai_logo_asset_generator PRIVATE Qt::Core Qt::Gui Qt::Svg)

# Windows: stage Qt DLLs next to the helper so it can launch during the build.
# Linux/macOS resolve Qt via RPATH/ld.so, so the block is a no-op there.
if(WIN32)
    add_custom_command(TARGET komai_logo_asset_generator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:komai_logo_asset_generator>
            $<TARGET_FILE_DIR:komai_logo_asset_generator>
        COMMAND_EXPAND_LISTS
    )
    # Qt's plugin lookup defaults to <exe-dir>/plugins, which is empty in the
    # build dir; point it at Qt's real plugin directory so qico (needed for
    # the .ico output) loads. Derived from Qt6Core.dll's location to avoid
    # hardcoding any specific Qt install layout.
    set(_komai_logo_helper_env
        ${CMAKE_COMMAND} -E env
            "QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
    )
else()
    set(_komai_logo_helper_env "")
endif()

add_custom_command(
    OUTPUT ${KOMAI_LOGO_GENERATED_FILES}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KOMAI_LOGO_ASSET_DIR}"
    COMMAND ${_komai_logo_helper_env} $<TARGET_FILE:komai_logo_asset_generator> ${KOMAI_LOGO_GENERATOR_ARGS}
    DEPENDS
        komai_logo_asset_generator
        "${KOMAI_LOGO_SOURCE_SVG}"
    COMMENT "Generating logo assets"
    VERBATIM
)
add_custom_target(logo_assets ALL DEPENDS ${KOMAI_LOGO_GENERATED_FILES})



# Generate runtime emoji data JSON files from pinned upstream cache/sources.
file(GLOB EMOJI_LANG_DIRS
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/resources/langs"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/langs/*"
)
set(EMOJI_LOCALES "")
foreach(_emoji_lang IN LISTS EMOJI_LANG_DIRS)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/langs/${_emoji_lang}")
        list(APPEND EMOJI_LOCALES "${_emoji_lang}")
    endif()
endforeach()
list(SORT EMOJI_LOCALES)

set(EMOJI_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/emoji-data")
set(EMOJI_CORE_JSON "${EMOJI_DATA_DIR}/core.json")
set(EMOJI_MANIFEST_JSON "${EMOJI_DATA_DIR}/manifest.json")
set(EMOJI_LOCALE_JSONS "")
foreach(_emoji_locale IN LISTS EMOJI_LOCALES)
    list(APPEND EMOJI_LOCALE_JSONS "${EMOJI_DATA_DIR}/locale/${_emoji_locale}.json")
endforeach()

file(GLOB_RECURSE EMOJI_OVERRIDE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/emoji/overrides/*.yml"
)

add_custom_command(
    OUTPUT
        "${EMOJI_CORE_JSON}"
        "${EMOJI_MANIFEST_JSON}"
        ${EMOJI_LOCALE_JSONS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EMOJI_DATA_DIR}/locale"
    COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/emoji/pipeline.py"
        build
        --repo-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --out-dir "${EMOJI_DATA_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/emoji/pipeline.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/emoji/sources.lock.yml"
        ${EMOJI_OVERRIDE_FILES}
    COMMENT "Generating runtime emoji data"
    VERBATIM
)
add_custom_target(emoji_runtime_data
    DEPENDS
        "${EMOJI_CORE_JSON}"
        "${EMOJI_MANIFEST_JSON}"
        ${EMOJI_LOCALE_JSONS}
)
set_source_files_properties(
    "${EMOJI_CORE_JSON}"
    "${EMOJI_MANIFEST_JSON}"
    ${EMOJI_LOCALE_JSONS}
    PROPERTIES GENERATED TRUE
)

set(KOMAI_DEPS
    ${SRC_FILES}
    ${QRC}
    ${META_FILES_TO_INCLUDE})

if(ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
endif()

if(WIN32)
    qt_add_executable(komai WIN32 ${OS_BUNDLE} ${KOMAI_DEPS})
    target_compile_definitions(komai PRIVATE _WIN32_WINNT=0x0601 NOMINMAX WIN32_LEAN_AND_MEAN STRICT)
else()
    qt_add_executable(komai ${OS_BUNDLE} ${KOMAI_DEPS})

    if(APPLE)
        set_target_properties(komai PROPERTIES MACOSX_BUNDLE_INFO_PLIST
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in")
    endif()

    # Enable -rdynamic only for explicit Debug builds.  -rdynamic exports every
    # static-archive symbol (Qt's QmlCacheGen output, the matrix-sdk Rust crate,
    # AWS-LC, etc.) into .dynsym/.dynstr so backtrace_symbols_fd can name them
    # at runtime — useful for developer debugging, but ~26 MiB of dead weight
    # in optimized builds.  Distro packagers (Arch, Debian, Fedora) configure
    # with CMAKE_BUILD_TYPE=None so the distro's own CFLAGS apply unmodified;
    # treating "None" as "not Release" would saddle every distro binary with
    # the rdynamic bloat.  Same shape for RelWithDebInfo/MinSizeRel, which
    # are still optimized release variants.  So gate strictly on Debug.
    if(HAVE_BACKTRACE_SYMBOLS_FD AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(komai PROPERTIES ENABLE_EXPORTS ON)
    endif()
endif()

set_target_properties(komai
    PROPERTIES
        CMAKE_SKIP_INSTALL_RPATH TRUE
        AUTOMOC ON)
target_compile_definitions(komai PRIVATE KOMAI_WITH_QML=1)
add_dependencies(komai emoji_runtime_data)
add_dependencies(komai logo_assets)
qt_add_resources(komai emoji_runtime_resources
    PREFIX "/emoji"
    BASE "${EMOJI_DATA_DIR}"
    FILES
        "${EMOJI_CORE_JSON}"
        ${EMOJI_LOCALE_JSONS}
)

if(ELEMENT_CALL)
    # Fetch + embed the pinned @element-hq/element-call-embedded bundle. Its file
    # set is dynamic (hashed asset names), so fetch at configure time and glob the
    # extracted web root rather than listing files. fetch.py is idempotent and
    # offline-friendly (offline builds pre-populate var/element-call/<version>/).
    set(ELEMENT_CALL_LOCK "${CMAKE_CURRENT_SOURCE_DIR}/bin/element-call/sources.lock.yml")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ELEMENT_CALL_LOCK}")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/element-call/fetch.py"
            --lock "${ELEMENT_CALL_LOCK}"
            --out-dir "${CMAKE_CURRENT_SOURCE_DIR}/var/element-call"
        OUTPUT_VARIABLE ELEMENT_CALL_DIST_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE ELEMENT_CALL_FETCH_RC
    )
    if(NOT ELEMENT_CALL_FETCH_RC EQUAL 0)
        message(FATAL_ERROR
            "Failed to fetch the Element Call bundle (exit ${ELEMENT_CALL_FETCH_RC}). "
            "Build with -DELEMENT_CALL=OFF to skip Element Call.")
    endif()
    # fetch.py prints a native path; on Windows that has backslashes, which CMake
    # bakes verbatim into the CONFIGURE_DEPENDS glob string in VerifyGlobs.cmake
    # and then rejects at build time ("Invalid character escape '\a'"). Normalize
    # to CMake-style forward slashes so the glob string stays valid everywhere.
    file(TO_CMAKE_PATH "${ELEMENT_CALL_DIST_DIR}" ELEMENT_CALL_DIST_DIR)
    file(GLOB_RECURSE ELEMENT_CALL_BUNDLE_FILES CONFIGURE_DEPENDS "${ELEMENT_CALL_DIST_DIR}/*")
    # Served to the webview as komai-ec://app/<path> (see ElementCallWebProfile).
    qt_add_resources(komai element_call_resources
        PREFIX "/element-call"
        BASE "${ELEMENT_CALL_DIST_DIR}"
        FILES ${ELEMENT_CALL_BUNDLE_FILES}
    )
endif()

#
# Add qml files
#
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()
set(QML_SOURCES_EXCLUDE
)
if(NOT ELEMENT_CALL)
    # This QML file "import QtWebEngine", which qmlcachegen cannot resolve
    # without the WebEngineQuick module, so keep it out of -DELEMENT_CALL=OFF builds.
    list(APPEND QML_SOURCES_EXCLUDE
        resources/qml/voip/ElementCallPanel.qml
    )
endif()
file(GLOB_RECURSE QML_SOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/qml/*.qml"
)
list(SORT QML_SOURCES)
foreach(qml_source_exclude IN LISTS QML_SOURCES_EXCLUDE)
    list(REMOVE_ITEM QML_SOURCES "${qml_source_exclude}")
endforeach()
set_source_files_properties(
    resources/qml/ui/media/InlineAudioPlaybackRegistry.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(
    resources/qml/components/MenuSizing.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set_source_files_properties(
    resources/qml/components/MutedText.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)
set(KOMAI_QML_MODULE_DEPENDENCIES QtQuick QtQml.Models)
set(KOMAI_QML_MODULE_SOURCES
    src/models/UserDirectoryModel.cpp
    src/models/UserDirectoryModel.h
)
if(ELEMENT_CALL)
    list(APPEND KOMAI_QML_MODULE_DEPENDENCIES QtWebEngine QtWebChannel)
    # ElementCallWebProfile is a QML singleton (QML_ELEMENT/QML_SINGLETON) and
    # ElementCallWidgetSession is a QML-instantiable type (QML_ELEMENT), so both
    # must be module SOURCES for qmltyperegistrar to pick them up. Gated behind
    # ELEMENT_CALL because the spike QML that uses them imports QtWebEngine.
    list(APPEND KOMAI_QML_MODULE_SOURCES
        src/voip/ElementCallWebProfile.cpp
        src/voip/ElementCallWebProfile.h
        src/voip/ElementCallWidgetSession.cpp
        src/voip/ElementCallWidgetSession.h
    )
endif()
qt_add_qml_module(komai
    URI cc.etke.komai
    NO_RESOURCE_TARGET_PATH
    RESOURCE_PREFIX "/"
    VERSION 1.1
    DEPENDENCIES ${KOMAI_QML_MODULE_DEPENDENCIES}
    QML_FILES
    ${QML_SOURCES}
    SOURCES
    ${KOMAI_QML_MODULE_SOURCES}
)
    #qt_target_qml_sources(komai
    #   #PREFIX "/"
    #)

#set_target_properties(komai PROPERTIES
#    QT_QMLCACHEGEN_ARGUMENTS "--verbose"
#)

#
# Bundle translations
#
file(GLOB LANG_TS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/resources/langs/*/komai_*.ts")
qt_add_translations(komai RESOURCE_PREFIX "/translations" TS_FILES
    ${LANG_TS_SRC} SOURCES ${QML_SOURCES} ${SRC_FILES})

if(WIN32)
    target_compile_definitions(komai PRIVATE WIN32_LEAN_AND_MEAN)
    target_link_libraries(komai PRIVATE Qt6::DBus)
    target_include_directories(komai PRIVATE ${WinToast_SOURCE_DIR}/include)
    if(MSVC)
        target_compile_options(komai PUBLIC "/Zc:__cplusplus")
    endif()
else()
    target_link_libraries(komai PRIVATE Qt6::DBus)
endif()

target_include_directories(komai PRIVATE src src/auth/ src/providers/ src/avatars/default/ src/imagepacks/ src/models/ src/powerlevels/ src/cli/ src/timeline/ src/timeline/litehtml/ src/timeline/rust/ src/ui/ src/encryption/ src/voip/ src/spellcheck/)

# Fixup bundled kdsingleapplication include dirs
target_include_directories(komai PRIVATE ${kdsingleapplication_SOURCE_DIR} ${kdsingleapplication_BINARY_DIR})

if(BUILD_TESTING)
    # Intentionally point HOME/XDG_* to non-writable /proc paths for all tests.
    # This makes accidental writes to user/home-derived locations fail fast.
    # Tests that need filesystem access must explicitly use ScopedTestHome.
    set(komai_test_env
        "HOME=/proc/komai-test-home"
        "XDG_CONFIG_HOME=/proc/komai-test-config"
        "XDG_DATA_HOME=/proc/komai-test-data"
        "XDG_CACHE_HOME=/proc/komai-test-cache"
        "XDG_STATE_HOME=/proc/komai-test-state"
        # Force headless Qt platform to avoid display-server dependency in hooks/CI.
        "QT_QPA_PLATFORM=offscreen"
    )

    function(komai_configure_test test_name labels)
        set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")
        set_property(TEST ${test_name} PROPERTY ENVIRONMENT "${komai_test_env}")
    endfunction()

    add_executable(komai_settings_integration_test
        tests/SettingsIntegrationTest.cpp
        tests/MatrixBackendBridgeStartupStub.cpp
        tests/ProfileSecretsStartupStub.cpp
        tests/UserSettingsModelStartupStub.cpp
        src/settings/ui/UserSettingsModel.h
        src/profile/KeyringEnvironment.cpp
        tests/LoggingStub.cpp
        src/profile/Paths.cpp
        src/settings/ui/facade/UserSettingsPage.cpp
        src/settings/ui/facade/UserSettingsGettersUi.cpp
        src/settings/ui/facade/UserSettingsGettersTimelineComposer.cpp
        src/settings/ui/facade/UserSettingsGettersLayout.cpp
        src/settings/ui/facade/UserSettingsGettersCalls.cpp
        src/settings/ui/facade/UserSettingsGettersEncryption.cpp
        src/settings/ui/facade/UserSettingsGettersMisc.cpp
        src/settings/ui/facade/UserSettingsSessionSettings.cpp
        src/settings/ui/facade/UserSettingsSettersCore.cpp
        src/settings/ui/facade/UserSettingsSettersLayout.cpp
        src/settings/ui/facade/UserSettingsSettersUi.cpp
        src/settings/ui/facade/UserSettingsSettersMisc.cpp
        src/settings/ui/facade/UserSettingsCoreStoreBridge.cpp
        src/settings/ui/facade/UserSettingsLanguage.cpp
        src/settings/ui/facade/UserSettingsTheme.cpp
        src/settings/ui/LanguagePresentation.cpp
        src/ui/Theme.cpp
        src/ui/ThemeRegistry.cpp
        src/settings/SettingsController.cpp
        src/settings/SettingsControllerLoad.cpp
        src/settings/SettingsControllerSave.cpp
        src/settings/SettingsSerializerConfig.cpp
        src/settings/SettingsSerializerConfigLoad.cpp
        src/settings/SettingsSerializerConfigSave.cpp
        src/settings/SettingsSerializerConfigConverters.cpp
        src/settings/SettingsSerializerConfigEnumTokenAdapters.cpp
        src/settings/SettingsSerializerConfigSchema.cpp
        src/settings/SettingsSerializerConfigSchemaValidation.cpp
        src/settings/SettingsSerializerSession.cpp
        src/settings/SettingsSerializerState.cpp
        src/settings/SettingsStorage.cpp
        src/settings/SettingsStoragePathApi.cpp
        src/settings/SettingsStorageReaderWriterFilesystem.cpp
        src/settings/SettingsStorageReaderWriterInMemory.cpp
        src/settings/SettingsStorageSecureBackend.cpp
        tests/support/settings/SettingsStorageSecretsCodec.cpp
        src/settings/core/SettingsStore.cpp
        src/timeline/TimelineEventTypes.cpp
        src/emoji/Provider.cpp
    )
    target_include_directories(komai_settings_integration_test PRIVATE
        src
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(komai_settings_integration_test PRIVATE
        Qt::Core
        Qt::Qml
        Qt::Gui
        Qt::Widgets
        Qt::Quick
        komai_rust
        komai-rust-cxxbridge
    )
    target_link_libraries(komai_settings_integration_test PRIVATE
        qt6keychain
    )
    set_target_properties(komai_settings_integration_test PROPERTIES AUTOMOC ON)
    add_test(NAME komai_settings_integration_test COMMAND komai_settings_integration_test)
    komai_configure_test(komai_settings_integration_test "integration;settings")

    add_executable(komai_settings_storage_test
        tests/SettingsStorageTest.cpp
        tests/LoggingStub.cpp
        tests/MatrixBackendBridgeStartupStub.cpp
        tests/ProfileSecretsStub.cpp
        src/matrix/backend/MatrixSessionSecrets.cpp
        src/profile/KeyringEnvironment.cpp
        src/profile/Paths.cpp
        src/settings/SettingsStorage.cpp
        src/settings/SettingsStoragePathApi.cpp
        src/settings/SettingsStorageReaderWriterFilesystem.cpp
        src/settings/SettingsStorageReaderWriterInMemory.cpp
        src/settings/SettingsStorageSecureBackend.cpp
        tests/support/settings/SettingsStorageSecretsCodec.cpp
    )
    target_include_directories(komai_settings_storage_test PRIVATE src)
    target_link_libraries(komai_settings_storage_test PRIVATE
        Qt::Core
        Qt::Qml
        komai_rust
        komai-rust-cxxbridge
    )
    target_link_libraries(komai_settings_storage_test PRIVATE
        qt6keychain
    )
    add_test(NAME komai_settings_storage_test COMMAND komai_settings_storage_test)
    komai_configure_test(komai_settings_storage_test "unit;settings")

    add_executable(komai_settings_store_test
        tests/SettingsStoreTest.cpp
        src/settings/core/SettingsStore.cpp
    )
    target_include_directories(komai_settings_store_test PRIVATE src)
    target_link_libraries(komai_settings_store_test PRIVATE Qt::Core)
    add_test(NAME komai_settings_store_test COMMAND komai_settings_store_test)
    komai_configure_test(komai_settings_store_test "unit;settings")

    add_executable(komai_setting_input_validation_test
        tests/SettingInputValidationTest.cpp
        src/settings/ui/SettingInputValidation.cpp
    )
    target_include_directories(komai_setting_input_validation_test PRIVATE src)
    target_link_libraries(komai_setting_input_validation_test PRIVATE Qt::Core Qt::Qml)
    add_test(NAME komai_setting_input_validation_test COMMAND komai_setting_input_validation_test)
    komai_configure_test(komai_setting_input_validation_test "unit;settings")


    add_executable(komai_notification_payload_test
        tests/NotificationPayloadTest.cpp
    )
    target_include_directories(komai_notification_payload_test PRIVATE src)
    target_link_libraries(komai_notification_payload_test PRIVATE
        Qt::Core
    )
    add_test(NAME komai_notification_payload_test COMMAND komai_notification_payload_test)
    komai_configure_test(komai_notification_payload_test "unit;timeline")

    add_executable(komai_roomlist_preview_selection_test
        tests/RoomlistPreviewSelectionTest.cpp
        src/timeline/roomlist/RoomlistPreviewSelection.cpp
    )
    target_include_directories(komai_roomlist_preview_selection_test PRIVATE src)
    target_link_libraries(komai_roomlist_preview_selection_test PRIVATE
        Qt::Core
    )
    add_test(NAME komai_roomlist_preview_selection_test COMMAND komai_roomlist_preview_selection_test)
    komai_configure_test(komai_roomlist_preview_selection_test "unit;timeline")

    add_executable(komai_theme_color_utils_test
        tests/ThemeColorUtilsTest.cpp
        src/cli/ThemeColorUtils.cpp
    )
    target_include_directories(komai_theme_color_utils_test PRIVATE src src/cli/)
    target_link_libraries(komai_theme_color_utils_test PRIVATE Qt::Core)
    add_test(NAME komai_theme_color_utils_test COMMAND komai_theme_color_utils_test)
    komai_configure_test(komai_theme_color_utils_test "unit;theme")

    add_executable(komai_mcp_commands_test
        tests/McpCommandsTest.cpp
        src/cli/McpCommands.cpp
        src/cli/schema/Dispatcher.cpp
        src/cli/schema/HelpRenderer.cpp
        src/app/MainApplicationSupport.cpp
    )
    target_include_directories(komai_mcp_commands_test PRIVATE src src/cli/)
    target_link_libraries(komai_mcp_commands_test PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
    )
    add_test(NAME komai_mcp_commands_test COMMAND komai_mcp_commands_test)
    komai_configure_test(komai_mcp_commands_test "unit;cli")

    add_executable(komai_dispatcher_test
        tests/DispatcherTest.cpp
        src/cli/schema/Dispatcher.cpp
        src/cli/schema/HelpRenderer.cpp
        src/app/MainApplicationSupport.cpp
    )
    target_include_directories(komai_dispatcher_test PRIVATE src src/cli/)
    target_link_libraries(komai_dispatcher_test PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
    )
    add_test(NAME komai_dispatcher_test COMMAND komai_dispatcher_test)
    komai_configure_test(komai_dispatcher_test "unit;cli")

    add_executable(komai_profile_commands_test
        tests/ProfileCommandsTest.cpp
        src/cli/ProfileCommands.cpp
        src/cli/schema/Dispatcher.cpp
        src/cli/schema/HelpRenderer.cpp
        src/app/MainApplicationSupport.cpp
        src/profile/Paths.cpp
        src/profile/KeyringEnvironment.cpp
    )
    target_include_directories(komai_profile_commands_test PRIVATE src src/cli/ ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(komai_profile_commands_test PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
    )
    add_test(NAME komai_profile_commands_test COMMAND komai_profile_commands_test)
    komai_configure_test(komai_profile_commands_test "unit;cli")

    add_executable(komai_bot_detection_test
        tests/BotDetectionTest.cpp
        src/utils/BotDetection.cpp
    )
    target_include_directories(komai_bot_detection_test PRIVATE src)
    add_test(NAME komai_bot_detection_test COMMAND komai_bot_detection_test)
    komai_configure_test(komai_bot_detection_test "unit;utils")

    add_executable(komai_emoji_completion_search_test
        tests/EmojiCompletionSearchTest.cpp
        tests/LoggingStub.cpp
        src/models/CompletionProxyModel.cpp
        src/emoji/Provider.cpp
        src/emoji/Emoji.cpp
    )
    target_include_directories(komai_emoji_completion_search_test PRIVATE src)
    target_link_libraries(komai_emoji_completion_search_test PRIVATE
        Qt::Core
    )
    set_target_properties(komai_emoji_completion_search_test PROPERTIES AUTOMOC ON)
    add_dependencies(komai_emoji_completion_search_test emoji_runtime_data)
    qt_add_resources(komai_emoji_completion_search_test emoji_runtime_resources_test
        PREFIX "/emoji"
        BASE "${EMOJI_DATA_DIR}"
        FILES
            "${EMOJI_CORE_JSON}"
            ${EMOJI_LOCALE_JSONS}
    )
    add_test(NAME komai_emoji_completion_search_test_en
        COMMAND komai_emoji_completion_search_test --locale en)
    komai_configure_test(komai_emoji_completion_search_test_en "unit;emoji")
    add_test(NAME komai_emoji_completion_search_test_ja
        COMMAND komai_emoji_completion_search_test --locale ja)
    komai_configure_test(komai_emoji_completion_search_test_ja "unit;emoji")

    add_executable(komai_emoji_normalization_test
        tests/EmojiNormalizationTest.cpp
        src/emoji/EmojiNormalize.cpp
    )
    target_include_directories(komai_emoji_normalization_test PRIVATE src)
    target_link_libraries(komai_emoji_normalization_test PRIVATE Qt::Core)
    add_test(NAME komai_emoji_normalization_test COMMAND komai_emoji_normalization_test)
    komai_configure_test(komai_emoji_normalization_test "unit;emoji")

    add_executable(komai_emoticon_replace_test
        tests/EmoticonReplaceTest.cpp
        src/emoji/EmoticonReplace.cpp
    )
    target_include_directories(komai_emoticon_replace_test PRIVATE src)
    # EmoticonReplace.h pulls in UserSettingsPage.h (for the AutoReplaceEmoji
    # enum used by the existing replaceEmoticons()), which needs the
    # generated cxxbridge headers to resolve at compile time. Nothing in
    # this test calls into Rust FFI, so only the include path is borrowed
    # from the cxxbridge target -- it is not linked.
    target_include_directories(komai_emoticon_replace_test PRIVATE
        $<TARGET_PROPERTY:komai-rust-cxxbridge,INTERFACE_INCLUDE_DIRECTORIES>
    )
    target_link_libraries(komai_emoticon_replace_test PRIVATE
        Qt::Core
        Qt::Qml
    )
    # Not linked (see above), but the generated headers must still exist
    # before this target compiles.
    add_dependencies(komai_emoticon_replace_test komai-rust-cxxbridge)
    add_test(NAME komai_emoticon_replace_test COMMAND komai_emoticon_replace_test)
    komai_configure_test(komai_emoticon_replace_test "unit;emoji")

    add_executable(komai_navigation_history_test
        tests/NavigationHistoryTest.cpp
        tests/LoggingStub.cpp
        src/timeline/NavigationHistory.cpp
    )
    target_include_directories(komai_navigation_history_test PRIVATE src)
    target_link_libraries(komai_navigation_history_test PRIVATE
        Qt::Core
    )
    add_test(NAME komai_navigation_history_test COMMAND komai_navigation_history_test)
    komai_configure_test(komai_navigation_history_test "unit;timeline")

    add_executable(komai_slash_commands_test
        tests/SlashCommandsTest.cpp
        tests/LoggingStub.cpp
        src/models/CommandCompleter.cpp
        src/timeline/SlashCommands.cpp
        src/utils/UtilsTextAndUri.cpp
    )
    target_include_directories(komai_slash_commands_test PRIVATE src)
    target_link_libraries(komai_slash_commands_test PRIVATE
        Qt::Core
        Qt::Gui
    )
    add_test(NAME komai_slash_commands_test COMMAND komai_slash_commands_test)
    komai_configure_test(komai_slash_commands_test "unit;timeline")

    add_executable(komai_layout_agnostic_keys_test
        tests/LayoutAgnosticKeysTest.cpp
        tests/LoggingStub.cpp
        src/ui/LayoutAgnosticKeys.cpp
    )
    target_include_directories(komai_layout_agnostic_keys_test PRIVATE src)
    target_link_libraries(komai_layout_agnostic_keys_test PRIVATE
        Qt::Core
        Qt::Qml
    )
    set_target_properties(komai_layout_agnostic_keys_test PROPERTIES AUTOMOC ON)
    add_test(NAME komai_layout_agnostic_keys_test COMMAND komai_layout_agnostic_keys_test)
    komai_configure_test(komai_layout_agnostic_keys_test "unit;ui")

    add_custom_target(komai_tests DEPENDS
        komai_settings_integration_test
        komai_settings_storage_test
        komai_settings_store_test
        komai_setting_input_validation_test
        komai_notification_payload_test
        komai_roomlist_preview_selection_test
        komai_theme_color_utils_test
        komai_mcp_commands_test
        komai_dispatcher_test
        komai_profile_commands_test
        komai_bot_detection_test
        komai_emoji_completion_search_test
        komai_emoji_normalization_test
        komai_emoticon_replace_test
        komai_navigation_history_test
        komai_slash_commands_test
        komai_layout_agnostic_keys_test
    )
endif()

target_link_libraries(komai PRIVATE
    Qt::Widgets
    Qt::Svg
    Qt::Gui
    Qt::Multimedia
    Qt::Network
    Qt::Qml
    Qt::QmlPrivate
    Qt::QuickControls2
    qt6keychain
    KDAB::kdsingleapplication
    litehtml
    komai_rust
    komai-rust-cxxbridge)

if(UNIX)
    # for wayland activation tokens
    target_link_libraries(komai PRIVATE Qt::GuiPrivate)
endif()

if(ELEMENT_CALL)
    target_link_libraries(komai PRIVATE Qt::WebEngineQuick Qt::WebChannelQuick)
    target_compile_definitions(komai PRIVATE ELEMENT_CALL_AVAILABLE)
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
    target_precompile_headers(komai
        PRIVATE
        <string>
        <algorithm>
    )
endif()

if(TARGET PkgConfig::GSTREAMER)
    target_link_libraries(komai PRIVATE PkgConfig::GSTREAMER)
    target_compile_definitions(komai PRIVATE GSTREAMER_AVAILABLE)
endif()
if(TARGET PkgConfig::XCB)
    target_link_libraries(komai PRIVATE PkgConfig::XCB)
    target_compile_definitions(komai PRIVATE XCB_AVAILABLE)
endif()

if(APPLE)
    # CoreServices for Launch Services API (external media player lookup by MIME type)
    target_link_libraries(komai PRIVATE "-framework CoreServices")
    # SystemConfiguration is needed by the `system_configuration` Rust crate (transitive via rustls-platform-verifier); its `cargo:rustc-link-lib=framework=` directive doesn't reach the C++ link line through Corrosion, mirroring the Windows `iphlpapi` workaround below.
    target_link_libraries(komai PRIVATE "-framework SystemConfiguration")
endif()

if(WIN32)
    # shlwapi for AssocQueryString (external media player lookup by file extension)
    # iphlpapi for GetAdaptersAddresses, called by the Rust `ipconfig` crate
    # (transitive dep of matrix-sdk for network adapter enumeration). The crate's
    # link declaration doesn't propagate through Corrosion to the C++ linker.
    # bcrypt for BCryptGenRandom, called by the Rust `getrandom` 0.2 crate; its
    # `#[link(name = "bcrypt")]` directive likewise doesn't reach the C++ linker.
    target_link_libraries(komai PRIVATE shlwapi iphlpapi bcrypt)
endif()

if(MSVC)
    target_link_libraries(komai PRIVATE ntdll)
endif()

if(HAIKU)
    target_link_libraries(komai PRIVATE network)
endif()

if(QML_DEBUGGING)
    target_compile_definitions(komai PRIVATE QML_DEBUGGING)
endif()

if(NOT MSVC AND NOT HAIKU)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR CI_BUILD)
        target_compile_options(komai PRIVATE "-Werror")
    endif()
endif()
#if(NOT MSVC)
#   target_link_options(komai PRIVATE "LINKER:,--gc-sections")
#endif()

if(MAN)
    add_subdirectory(etc/man)
endif()

# potential workaround for macdeployqt issues
if(APPLE)
    install(TARGETS komai
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    if(TARGET komai-mcp)
        install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/komai-mcp${CMAKE_EXECUTABLE_SUFFIX}"
            DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    # Default empty so a normal `just build` on a developer Mac gets stripped + ad-hoc-signed frameworks from macdeployqt.
    # Packagers can override (e.g. `-no-strip;-no-codesign`) when the host toolchain's strip or codesign isn't safe to invoke during install; Nix sandboxes need this for LC_DYLD_CHAINED_FIXUPS support and a spawn-codesign env quirk.
    set(KOMAI_DEPLOY_TOOL_OPTIONS "" CACHE STRING "Extra options forwarded to macdeployqt via DEPLOY_TOOL_OPTIONS")
    qt_generate_deploy_qml_app_script(
        TARGET komai
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS ${KOMAI_DEPLOY_TOOL_OPTIONS}
    )
    install(SCRIPT ${deploy_script})
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(komai PROPERTIES OUTPUT_NAME "${KOMAI_EXECUTABLE_NAME}")

    install(TARGETS komai RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(TARGET komai-mcp)
        install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/komai-mcp${CMAKE_EXECUTABLE_SUFFIX}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-16.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-32.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-48.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-64.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-256.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_ASSET_DIR}/komai-512.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.png")
    install(FILES "${KOMAI_LOGO_SOURCE_SVG}" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps" RENAME "${KOMAI_DESKTOP_ICON_NAME}.svg")
    install(FILES "resources/completions/bash/komai" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/bash-completion/completions")
    install(FILES "resources/completions/zsh/_komai" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/zsh/site-functions")
    install(FILES "resources/completions/fish/komai.fish" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/fish/vendor_completions.d")

    configure_file("resources/komai.desktop.in" "resources/komai.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/komai.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications" RENAME "${KOMAI_DESKTOP_ID}.desktop")
    configure_file("resources/komai.appdata.xml.in" "resources/komai.appdata.xml" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/komai.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo" RENAME "${KOMAI_DESKTOP_ID}.appdata.xml")

    if(NOT TARGET uninstall)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)
        add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
    endif()
endif()

if(WIN32)
    set_target_properties(komai PROPERTIES OUTPUT_NAME "${KOMAI_EXECUTABLE_NAME}")

    install(TARGETS komai RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(TARGET komai-mcp)
        install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/komai-mcp${CMAKE_EXECUTABLE_SUFFIX}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
endif()
# vim: tabstop=4 shiftwidth=4 expandtab
