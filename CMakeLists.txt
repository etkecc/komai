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
if(UNIX AND NOT APPLE)
    option(MAN "Build man page" ON)
else()
    option(MAN "Build man page" OFF)
endif()
option(FLATPAK "Set this only if Komai is built as a flatpak" OFF)
option(JSON_ImplicitConversions "Disable implicit conversions in nlohmann/json" ON)

set(
    CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain.cmake"
    CACHE
    FILEPATH "Default toolchain"
)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard to be supported")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "compile as PIC by default")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macos deployment target")

option(HUNTER_ENABLED "Enable Hunter package manager" OFF)
include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.26.6.tar.gz"
    SHA1 "e70c29f878f5d5f5cdf1b9ccd628fb872e8624a8"
    LOCAL
)

macro(hunter_add_package_safe)
    set(pkg_temp_backup_libdir "$ENV{PKG_CONFIG_LIBDIR}")
    set(pkg_temp_backup_path "$ENV{PKG_CONFIG_PATH}")
    hunter_add_package(${ARGV})
    if("${pkg_temp_backup_path}" STREQUAL "")
        unset(ENV{PKG_CONFIG_PATH})
    else()
        set(ENV{PKG_CONFIG_PATH} "${pkg_temp_backup_path}")
    endif()
    if("${pkg_temp_backup_libdir}" STREQUAL "")
        unset(ENV{PKG_CONFIG_LIBDIR})
    else()
        set(ENV{PKG_CONFIG_LIBDIR} "${pkg_temp_backup_libdir}")
    endif()
    message("pkg_conf_path: '$ENV{PKG_CONFIG_PATH}', pkg_conf_libdir: '$ENV{PKG_CONFIG_LIBDIR}'")
endmacro()

project(komai LANGUAGES CXX C)

option(USE_BUNDLED_SPDLOG "Use the bundled version of spdlog." ${HUNTER_ENABLED})
option(USE_BUNDLED_FMT "Use the bundled version of {fmt}." ${HUNTER_ENABLED})
option(USE_BUNDLED_OLM "Use the bundled version of libolm." ${HUNTER_ENABLED})
option(USE_BUNDLED_GTEST "Use the bundled version of Google Test." ${HUNTER_ENABLED})
option(USE_BUNDLED_CMARK "Use the bundled version of cmark." ${HUNTER_ENABLED})
option(USE_BUNDLED_JSON "Use the bundled version of nlohmann json." ${HUNTER_ENABLED})
option(USE_BUNDLED_OPENSSL "Use the bundled version of OpenSSL." OFF)
# Default to bundled mtxclient because v0.10.1 has a key-backup public_key
# double-encoding issue that we patch locally in third_party/mtxclient-patches.
# Upstream PR: https://github.com/Nheko-Reborn/mtxclient/pull/113
option(USE_BUNDLED_MTXCLIENT "Use the bundled version of the Matrix Client library." ON)
option(USE_BUNDLED_LMDB "Use the bundled version of lmdb." ${HUNTER_ENABLED})
option(USE_BUNDLED_LMDBXX "Use the bundled version of lmdb++." ${HUNTER_ENABLED})
option(USE_BUNDLED_QTKEYCHAIN "Use the bundled version of Qt6Keychain." ${HUNTER_ENABLED})
option(USE_BUNDLED_COEURL "Use a bundled version of the Curl wrapper"
    ${HUNTER_ENABLED})
option(USE_BUNDLED_LIBEVENT "Use the bundled version of libevent." ${HUNTER_ENABLED})
option(USE_BUNDLED_LIBCURL "Use the bundled version of libcurl." ${HUNTER_ENABLED})
option(USE_BUNDLED_RE2 "Use the bundled version of re2." ${HUNTER_ENABLED})
option(USE_BUNDLED_KDSINGLEAPPLICATION "Use the bundled version of KDSingleApplication." ${HUNTER_ENABLED})
option(USE_BUNDLED_CPPHTTPLIB "Use the bundled version of cpp-httplib." ON)
option(USE_BUNDLED_BLURHASH "Use the bundled version of blurhash." ON)
option(USE_BUNDLED_YAMLCPP "Use the bundled version of yaml-cpp." ${HUNTER_ENABLED})
option(KOMAI_DB_ENABLE_LMDB_BACKEND "Build LMDB database backend support." ON)

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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(${CMAKE_VERSION} VERSION_LESS "3.14.0")
    message("Adding FetchContent_MakeAvailable")
    # from cmakes sources
    macro(FetchContent_MakeAvailable)
        foreach(contentName IN ITEMS ${ARGV})
            string(TOLOWER ${contentName} contentNameLower)
            FetchContent_GetProperties(${contentName})
            if(NOT ${contentNameLower}_POPULATED)
                FetchContent_Populate(${contentName})

                # Only try to call add_subdirectory() if the populated content
                # can be treated that way. Protecting the call with the check
                # allows this function to be used for projects that just want
                # to ensure the content exists, such as to provide content at
                # a known location.
                if(EXISTS ${${contentNameLower}_SOURCE_DIR}/CMakeLists.txt)
                    add_subdirectory(${${contentNameLower}_SOURCE_DIR}
                        ${${contentNameLower}_BINARY_DIR})
                endif()
            endif()
        endforeach()
    endmacro()
endif()

# Include Qt basic functions
include(QtCommon)

include(GNUInstallDirs)
include(CTest)

set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(PROJECT_VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR})
set(PROJECT_VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR})
set(PROJECT_VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH})

# Set PROJECT_VERSION_PATCH & PROJECT_VERSION_TWEAK to 0 if not present
# Needed by add_project_meta.
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
## coeurl
#
## Need to repeat all libevent deps?!?
if(USE_BUNDLED_LIBEVENT)
    hunter_add_package_safe(Libevent)
    find_package(Libevent CONFIG REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libevent_core REQUIRED IMPORTED_TARGET libevent_core)
    if(WIN32)
        pkg_check_modules(libevent_windows REQUIRED IMPORTED_TARGET libevent_windows)
    else()
        pkg_check_modules(libevent_pthreads REQUIRED IMPORTED_TARGET libevent_pthreads)
    endif()
endif()

# curl
if(USE_BUNDLED_LIBCURL)
    hunter_add_package_safe(CURL)
    find_package(CURL CONFIG REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)
endif()

# fmt
if(USE_BUNDLED_FMT)
    hunter_add_package_safe(fmt)
endif()
find_package(fmt 8.0.0 CONFIG REQUIRED)

# spdlog
if(USE_BUNDLED_SPDLOG)
    hunter_add_package_safe(spdlog)
endif()
find_package(spdlog 1.0.0 CONFIG REQUIRED)

if(USE_BUNDLED_COEURL)
    include(FetchContent)
    FetchContent_Declare(
        coeurl
        GIT_REPOSITORY https://nheko.im/Nheko-Reborn/coeurl.git
        GIT_TAG        831e2ee8e9cf08ea1ee9736cde8370f9d0312abc
    )
    FetchContent_MakeAvailable(coeurl)
    set(COEURL_TARGET_NAME coeurl::coeurl)
else()
    find_package(PkgConfig)
    pkg_check_modules(coeurl IMPORTED_TARGET coeurl>=0.3.0)
    if(TARGET PkgConfig::coeurl)
        set(COEURL_TARGET_NAME PkgConfig::coeurl)
    endif()
endif()

if(NOT TARGET PkgConfig::coeurl)
    find_package(coeurl 0.2.1 CONFIG)
    if(TARGET coeurl::coeurl)
        set(COEURL_TARGET_NAME coeurl::coeurl)
    endif()
endif()

if(NOT COEURL_TARGET_NAME)
    message(ERROR "Couldn't find coeurl")
endif()

if(USE_BUNDLED_RE2)
    hunter_add_package(re2)
    find_package(re2 CONFIG REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(re2 REQUIRED IMPORTED_TARGET re2)
endif()

#
# LMDB
#
#include(LMDB)
if(KOMAI_DB_ENABLE_LMDB_BACKEND)
    if(USE_BUNDLED_LMDB)
        hunter_add_package_safe(lmdb)
        find_package(liblmdb CONFIG REQUIRED)

        target_include_directories(liblmdb::lmdb INTERFACE
            "${HUNTER_INSTALL_PREFIX}/include/lmdb")
    else()
        find_package(LMDB REQUIRED)
    endif()
endif()

#
# Discover Qt dependencies.
#

find_package(Qt6 6.5 COMPONENTS Core Widgets Gui LinguistTools Svg Multimedia Qml QuickControls2 Network REQUIRED)
if (Qt6Qml_VERSION VERSION_GREATER_EQUAL "6.10.0")
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate QmlPrivate)
endif()
find_package(KF6SyntaxHighlighting REQUIRED)
find_package(Qt6DBus)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(USE_BUNDLED_QTKEYCHAIN)
    include(FetchContent)
    set(BUILD_WITH_QT6 ON)
    FetchContent_Declare(
        qt6keychain
        GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
        GIT_TAG        v0.14.0
    )
    set(BUILD_TEST_APPLICATION OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(qt6keychain)
else()
    find_package(Qt6Keychain REQUIRED)
endif()

if(USE_BUNDLED_KDSINGLEAPPLICATION)
    include(FetchContent)
    set(KDSingleApplication_QT6 ON CACHE BOOL INTERNAL)
    set(KDSingleApplication_EXAMPLES OFF CACHE BOOL INTERNAL)
    FetchContent_Declare(
        kdsingleapplication
        GIT_REPOSITORY https://github.com/KDAB/KDSingleApplication.git
        GIT_TAG        v1.0.0
    )
    FetchContent_MakeAvailable(kdsingleapplication)
else()
    find_package(KDSingleApplication-qt6 REQUIRED)
endif()

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

set(SPDLOG_DEBUG_ON false)

# Windows doesn't handle CMAKE_BUILD_TYPE.
if(NOT WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(SPDLOG_DEBUG_ON true)
    else()
        set(SPDLOG_DEBUG_ON false)
    endif()
endif()

# [komai] Git hash suffix removed — version is set statically above.

set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})
set(PROJECT_VERSION ${CPACK_PACKAGE_VERSION})

message(STATUS "Version: ${PROJECT_VERSION}")

# Get git commit hash for version info
execute_process(
    COMMAND git rev-parse --short=8 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")

# Read pinned Fluent icon source ref (used by About acknowledgements)
set(FLUENT_ICONS_VERSION "unknown")
set(FLUENT_VERSION_REF_FILE "${CMAKE_SOURCE_DIR}/bin/icons/FLUENT_VERSION_REF")
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

    # Timeline
    src/timeline/CommunitiesModel.cpp
    src/timeline/CommunitiesModel.h
    src/timeline/communities/CommunitiesModelData.cpp
    src/timeline/communities/CommunitiesModelSpaces.cpp
    src/timeline/communities/CommunitiesModelSync.cpp
    src/timeline/DelegateChooser.cpp
    src/timeline/DelegateChooser.h
    src/timeline/EventStore.cpp
    src/timeline/EventStore.h
    src/timeline/eventstore/EventStoreRelations.cpp
    src/timeline/eventstore/EventStoreAccess.cpp
    src/timeline/eventstore/EventStoreWindowing.cpp
    src/timeline/eventstore/EventStorePending.cpp
    src/timeline/eventstore/EventStoreKeyRequests.cpp
    src/timeline/EventDelegateChooser.cpp
    src/timeline/EventDelegateChooser.h
    src/timeline/InputBar.cpp
    src/timeline/InputBar.h
    src/timeline/inputbar/InputBarCommands.cpp
    src/timeline/inputbar/InputBarMessaging.cpp
    src/timeline/inputbar/InputBarMediaMessages.cpp
    src/timeline/inputbar/InputBarUploads.cpp
    src/timeline/FormattedCodeBlockHighlighter.cpp
    src/timeline/FormattedCodeBlockHighlighter.h
    src/timeline/formattedcode/HtmlRenderer.cpp
    src/timeline/formattedcode/HtmlRenderer.h
    src/timeline/formattedcode/LanguageResolver.cpp
    src/timeline/formattedcode/LanguageResolver.h
    src/timeline/formattedcode/RawJsonFormatter.cpp
    src/timeline/formattedcode/RawJsonFormatter.h
    src/timeline/formattedmessage/HtmlProcessor.cpp
    src/timeline/formattedmessage/HtmlProcessor.h
    src/timeline/litehtml/LitehtmlContainer.cpp
    src/timeline/litehtml/LitehtmlContainer.h
    src/timeline/litehtml/LitehtmlItem.cpp
    src/timeline/litehtml/LitehtmlItem.h
    src/timeline/litehtml/LitehtmlStylesheet.cpp
    src/timeline/litehtml/LitehtmlStylesheet.h
    src/timeline/composer/TimelineModelComposer.cpp
    src/timeline/actions/TimelineModelActions.cpp
    src/timeline/data/RoomlistModelData.cpp
    src/timeline/data/TimelineModelDataFormatting.cpp
    src/timeline/data/TimelineModelDataRoleHelpers.cpp
    src/timeline/data/TimelineModelDataRoles.cpp
    src/timeline/format/TimelineImagePackFormatter.cpp
    src/timeline/format/TimelineImagePackFormatter.h
    src/timeline/format/TimelineMemberEventFormatter.cpp
    src/timeline/format/TimelineMemberEventFormatter.h
    src/timeline/format/TimelinePolicyRuleFormatter.cpp
    src/timeline/format/TimelinePolicyRuleFormatter.h
    src/timeline/format/TimelinePowerLevelFormatter.cpp
    src/timeline/format/TimelinePowerLevelFormatter.h
    src/timeline/format/TimelineRedactedEventFormatter.cpp
    src/timeline/format/TimelineRedactedEventFormatter.h
    src/timeline/media/TimelineMediaController.cpp
    src/timeline/media/TimelineMediaController.h
    src/timeline/navigation/TimelineModelNavigation.cpp
    src/timeline/presentation/TimelineModelPresentation.cpp
    src/timeline/sync/TimelineModelSync.cpp
    src/timeline/rawmessage/RawMessageDialogPayload.cpp
    src/timeline/rawmessage/RawMessageDialogPayload.h
    src/timeline/roomlist/FilteredRoomlistModel.cpp
    src/timeline/roomlist/RoomlistModelCachedMessages.cpp
    src/timeline/roomlist/RoomlistModelMaterialization.cpp
    src/timeline/roomlist/RoomlistModelPreviews.cpp
    src/timeline/roomlist/RoomlistPreviewSelection.cpp
    src/timeline/roomlist/RoomlistModelPrewarm.cpp
    src/timeline/roomlist/RoomlistModelSelection.cpp
    src/timeline/roomlist/RoomlistModelSync.cpp
    src/timeline/DirectChatResolver.cpp
    src/timeline/DirectChatResolver.h
    src/timeline/state/TimelineModelRoomState.cpp
    src/timeline/state/TimelineModelStateEventActions.cpp
    src/timeline/send/TimelineMessageSendPipeline.cpp
    src/timeline/send/TimelineMessageSendPipeline.h
    src/timeline/Permissions.cpp
    src/timeline/Permissions.h
    src/timeline/PresenceEmitter.cpp
    src/timeline/PresenceEmitter.h
    src/timeline/Reaction.cpp
    src/timeline/Reaction.h
    src/timeline/RoomlistModel.cpp
    src/timeline/RoomlistModel.h
    src/timeline/TimelineFilter.cpp
    src/timeline/TimelineFilter.h
    src/timeline/TimelineEventTypes.cpp
    src/timeline/TimelineEventTypes.h
    src/timeline/TimelineModel.cpp
    src/timeline/TimelineModel.h
    src/timeline/NavigationHistory.cpp
    src/timeline/NavigationHistory.h
    src/timeline/TimelineViewManager.cpp
    src/timeline/TimelineViewManager.h
    src/timeline/view/TimelineViewManagerMedia.cpp
    src/timeline/view/TimelineViewManagerForwarding.cpp
    src/timeline/view/TimelineViewManagerRoomActions.cpp
    src/timeline/view/TimelineViewManagerColors.cpp
    src/timeline/view/TimelineViewManagerCompletions.cpp

    # UI components
    src/ui/HiddenEvents.cpp
    src/ui/HiddenEvents.h
    src/ui/EventExpiry.cpp
    src/ui/EventExpiry.h
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
    src/ui/ThemeRegistry.cpp
    src/ui/ThemeRegistry.h
    src/ui/UIA.cpp
    src/ui/UIA.h
    src/ui/UserProfile.cpp
    src/ui/UserProfileActions.cpp
    src/ui/UserProfileModels.cpp
    src/ui/UserProfile.h

    src/voip/CallDevices.cpp
    src/voip/CallDevices.h
    src/voip/CallManager.cpp
    src/voip/callmanager/CallManagerScreenShare.cpp
    src/voip/CallManager.h
    src/voip/ScreenCastPortal.cpp
    src/voip/ScreenCastPortal.h
    src/voip/WebRTCSession.h
    src/voip/WebRTCSession.cpp
    src/voip/WebRTCSession.h

    src/encryption/DeviceVerificationFlow.cpp
    src/encryption/DeviceVerificationFlowHandlers.cpp
    src/encryption/DeviceVerificationFlowProtocol.cpp
    src/encryption/DeviceVerificationFlow.h
    src/encryption/Olm.cpp
    src/encryption/OlmSupport.cpp
    src/encryption/OlmToDeviceSend.cpp
    src/encryption/OlmMegolmBackup.cpp
    src/encryption/OlmKeyBackupSync.cpp
    src/encryption/OlmCrossSigningDownload.cpp
    src/encryption/OlmSessionDecrypt.cpp
    src/encryption/Olm.h
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
    src/cache/core/Cache.cpp
    src/cache/core/CacheCoreStorage.cpp
    src/cache/lifecycle/CacheLifecycleRooms.cpp
    src/cache/lifecycle/CacheLifecycleState.cpp
    src/cache/lifecycle/CacheLifecycleMigrations.cpp
    src/cache/lifecycle/CacheLifecycleStateWrite.cpp
    src/cache/lifecycle/CacheLifecycleStateRead.cpp
    src/cache/lifecycle/CacheLifecycleSyncSaveState.cpp
    src/cache/lifecycle/CacheLifecycleSyncPersist.cpp
    src/cache/lifecycle/CacheLifecycleSyncAux.cpp
    src/cache/lifecycle/CacheLifecycle.cpp
    src/cache/setup/CacheSetup.cpp
    src/cache/membership/CacheMembership.cpp
    src/cache/membership/CacheMembershipInvites.cpp
    src/cache/rooms/CacheRoomInfo.cpp
    src/cache/rooms/CacheRoomInfoState.cpp
    src/cache/rooms/CacheRoomInfoInvites.cpp
    src/cache/spaces/CacheSpaces.cpp
    src/cache/spaces/CacheSpacesImagePacks.cpp
    src/cache/timeline/CacheTimeline.cpp
    src/cache/timeline/CacheTimelineWrite.cpp
    src/cache/timeline/CacheTimelinePending.cpp
    src/cache/timeline/CacheTimelineRead.cpp
    src/cache/api/CacheApiStateTemplates.cpp
    src/cache/crypto/CacheCryptoMegolm.cpp
    src/cache/crypto/CacheCryptoMegolmMetadata.cpp
    src/cache/crypto/CacheCryptoOlm.cpp
    src/cache/crypto/CacheCryptoSecrets.cpp
    src/cache/crypto/CacheCryptoSecretsStore.cpp
    src/cache/core/CacheCoreRuntime.cpp
    src/cache/users/CacheUserProfile.cpp
    src/cache/users/CacheUserKeys.cpp
    src/cache/users/CacheUserKeysNetwork.cpp
    src/cache/users/CacheUserKeysVerification.cpp
    src/cache/users/CacheUserTrust.cpp
    src/cache/api/CacheApiLoggers.cpp
    src/cache/api/CacheApiInstance.cpp
    src/cache/api/CacheApiLifecycleOps.cpp
    src/cache/api/CacheApiUsers.cpp
    src/cache/api/CacheApiRoomsInfo.cpp
    src/cache/api/CacheApiSignals.cpp
    src/cache/api/CacheApiTimeline.cpp
    src/cache/api/CacheApiRooms.cpp
    src/cache/api/CacheApiCrypto.cpp
    src/cache/Cache.h
    src/cache/api/CacheApi.h
    src/cache/api/CacheApiTypes.h
    src/cache/api/CacheApiLifecycle.h
    src/cache/api/CacheApiUsers.h
    src/cache/api/CacheApiSignals.h
    src/cache/api/CacheApiRooms.h
    src/cache/api/CacheApiTimeline.h
    src/cache/api/CacheApiCrypto.h
    src/cache/crypto/CacheCryptoStructs.cpp
    src/cache/crypto/CacheCryptoStructsQml.cpp
    src/cache/crypto/CacheCryptoStructs.h
    src/cache/core/CacheVersion.h
    src/cache/core/CacheStorageContext.h
    src/cache/core/CacheTxn.h
    src/cache/core/Cache_p.h
    src/db/InMemoryBackend.h
    src/db/LmdbBackend.h
    src/db/Backend.h
    src/db/Catalog.h
    src/db/DbTypes.h
    src/db/DupIndex.h
    src/db/Internal.h
    src/db/LmdbHeaders.h
    src/db/NamePolicy.h
    src/db/OrderEntry.h
    src/db/Scan.h
    src/db/StateIndex.h
    src/chat/ChatPageRoomActions.cpp
    src/chat/ChatPageSessionLifecycle.cpp
    src/chat/ChatPageSync.cpp
    src/chat/ChatPageNotifications.cpp
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
    src/timeline/formattedmessage/LinkPatterns.h
    src/matrix/MatrixStateTypes.h
    src/events/EventAccessors.cpp
    src/events/EventAccessorsMedia.cpp
    src/events/EventAccessors.h
    src/auth/FallbackAuth.cpp
    src/auth/FallbackAuth.h
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
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/matrix/MatrixClient.cpp
    src/matrix/MatrixClient.h
    src/models/MemberList.cpp
    src/models/MemberList.h
    src/providers/MxcImageProvider.cpp
    src/providers/MxcImageProvider.h
    src/profile/Paths.cpp
    src/profile/Paths.h
    src/profile/ProfileManager.cpp
    src/profile/ProfileManager.h
    src/settings/SettingKeys.h
    src/settings/ui/SettingDescriptor.h
    src/settings/ui/SettingDescriptor.cpp
    src/settings/ui/SettingDescriptorTable.cpp
    src/settings/ui/SettingRoleData.h
    src/settings/ui/SettingRoleData.cpp
    src/settings/ui/SettingInputValidation.h
    src/settings/ui/SettingInputValidation.cpp
    src/settings/ui/SessionKeyActions.h
    src/settings/ui/SessionKeyActions.cpp
    src/settings/StartupSettings.h
    src/settings/StartupSettings.cpp
    src/settings/core/SettingDefinition.h
    src/settings/core/StartupConfig.h
    src/settings/core/StartupConfig.cpp
    src/settings/core/SettingsStore.h
    src/settings/core/SettingsStore.cpp
    src/settings/core/SettingsSerializer.h
    src/settings/core/SettingsSerializer.cpp
    src/settings/SettingsPersistence.cpp
    src/settings/SettingsPersistenceLoad.cpp
    src/settings/SettingsPersistenceSave.cpp
    src/settings/SettingsPersistenceInternal.h
    src/settings/SettingsPersistence.h
    src/settings/SettingsMigrations.cpp
    src/settings/SettingsMigrations.h
    src/settings/SettingsStorage.cpp
    src/settings/SettingsStoragePathApi.cpp
    src/settings/SettingsStorageReaderWriterFilesystem.cpp
    src/settings/SettingsStorageReaderWriterInMemory.cpp
    src/settings/SettingsStorageInternal.h
    src/settings/SettingsStorageSecureBackend.cpp
    src/settings/SettingsStorageSecretsCodec.cpp
    src/settings/SettingsStorage.h
    src/settings/SettingsSerializerConfigConverters.cpp
    src/settings/SettingsSerializerConfigEnumTokenAdapters.cpp
    src/settings/SettingsSerializerConfigSchema.cpp
    src/settings/SettingsSerializerConfigSchemaValidation.cpp
    src/settings/SettingsSerializerConfig.cpp
    src/settings/SettingsSerializerConfigLoad.cpp
    src/settings/SettingsSerializerConfigSave.cpp
    src/settings/SettingsSerializerConfigInternal.h
    src/settings/SettingsSerializerSession.cpp
    src/settings/SettingsSerializerState.cpp
    src/settings/SettingsSerializer.h
    src/settings/SettingsController.h
    src/settings/SettingsController.cpp
    src/settings/SettingsControllerLoad.cpp
    src/settings/SettingsControllerSave.cpp
    src/settings/SettingsControllerInternal.h
    src/settings/YamlSettings.h
    src/powerlevels/PowerlevelEditingModels.cpp
    src/powerlevels/PowerlevelsEditModels.h
    src/powerlevels/PowerlevelsSpacesListModel.cpp
    src/powerlevels/PowerlevelsTypeListModel.cpp
    src/powerlevels/PowerlevelsUserListModel.cpp
    src/profile/ProfileSecrets.cpp
    src/profile/ProfileSecrets.h
    src/models/ReadReceiptsModel.cpp
    src/models/ReadReceiptsModel.h
    src/auth/ReCaptcha.cpp
    src/auth/ReCaptcha.h
    src/auth/RegisterPage.cpp
    src/auth/RegisterPage.h
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
    src/settings/ui/facade/UserSettingsSessionSettings.cpp
    src/settings/ui/facade/UserSettingsTheme.cpp
    src/models/UsersModel.cpp
    src/models/UsersModel.h
    src/utils/BotDetection.cpp
    src/utils/UtilsCore.cpp
    src/utils/Utils.h
    src/utils/UtilsColor.cpp
    src/utils/UtilsMessageDescription.cpp
    src/utils/UtilsRoomMaintenance.cpp
    src/utils/UtilsSpaceVias.cpp
    src/utils/UtilsEventExpiry.cpp
    src/utils/UtilsMarkdown.cpp
    src/utils/UtilsTextAndUri.cpp


    # CLI subcommands
    src/cli/CliDispatch.cpp
    src/cli/CliDispatch.h
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

if(USE_BUNDLED_OPENSSL)
    hunter_add_package_safe(OpenSSL)
endif()
find_package(OpenSSL 1.1.0 REQUIRED)
if(USE_BUNDLED_OLM)
    include(FetchContent)
    FetchContent_Declare(
        Olm
        GIT_REPOSITORY https://gitlab.matrix.org/matrix-org/olm.git
        GIT_TAG        3.2.16
        PATCH_COMMAND  git apply --whitespace=nowarn
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/olm-patches/0001-fix-list-const-ptr.patch
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/olm-patches/0002-fix-cmake-cmp0148.patch
        UPDATE_DISCONNECTED 1
    )
    set(OLM_TESTS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(Olm)
else()
    find_package(Olm 3.2.7 REQUIRED)
    set_package_properties(Olm PROPERTIES
        DESCRIPTION "An implementation of the Double Ratchet cryptographic ratchet"
        URL "https://git.matrix.org/git/olm/about/"
        TYPE REQUIRED
    )
endif()
if(USE_BUNDLED_SPDLOG)
    hunter_add_package_safe(spdlog)
endif()
find_package(spdlog 1.0.0 CONFIG REQUIRED)

if(USE_BUNDLED_CMARK)
    include(FetchContent)
    FetchContent_Declare(
        cmark
        GIT_REPOSITORY https://github.com/commonmark/cmark.git
        GIT_TAG        0.31.1
        CMAKE_ARGS     "BUILD_TESTING=OFF"
    )
    FetchContent_MakeAvailable(cmark)
    if (NOT TARGET cmark::cmark)
        add_library(cmark::cmark ALIAS cmark)
    endif()
else()
    find_package(cmark REQUIRED 0.29.0)
endif()

if(USE_BUNDLED_JSON)
    hunter_add_package_safe(nlohmann_json)
endif()
find_package(nlohmann_json 3.2.0)
set_package_properties(nlohmann_json PROPERTIES
    DESCRIPTION "JSON for Modern C++, a C++11 header-only JSON class"
    URL "https://nlohmann.github.io/json/"
    TYPE REQUIRED
)

# yaml-cpp for YAML config files
if(USE_BUNDLED_YAMLCPP)
    include(FetchContent)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG        0.8.0
    )
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(yaml-cpp)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(yaml-cpp REQUIRED IMPORTED_TARGET yaml-cpp>=0.6.0)
endif()

if(KOMAI_DB_ENABLE_LMDB_BACKEND)
    if(USE_BUNDLED_LMDBXX)
        include(FetchContent)
        FetchContent_Declare(
            lmdbxx
            URL "https://raw.githubusercontent.com/hoytech/lmdbxx/1.0.0/lmdb++.h"
            DOWNLOAD_NO_EXTRACT TRUE
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
        )
        FetchContent_Populate(lmdbxx)
        add_library(lmdbxx INTERFACE)
        target_include_directories(lmdbxx INTERFACE ${lmdbxx_SOURCE_DIR})
        add_library(lmdbxx::lmdbxx ALIAS lmdbxx)
    else()
        if(NOT LMDBXX_INCLUDE_DIR)
            find_path(LMDBXX_INCLUDE_DIR
                NAMES lmdb++.h
                PATHS /usr/include
                /usr/local/include
                $ENV{LIB_DIR}/include
                $ENV{LIB_DIR}/include/lmdbxx)
        endif()
        add_library(lmdbxx INTERFACE)
        target_include_directories(lmdbxx INTERFACE ${LMDBXX_INCLUDE_DIR})
        add_library(lmdbxx::lmdbxx ALIAS lmdbxx)
    endif()
endif()

if(USE_BUNDLED_MTXCLIENT)
    include(FetchContent)
    set(MTXCLIENT_PATCH_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mtxclient-patches/0001-fix-online-backup-public-key-double-encoding.patch)
    FetchContent_Declare(
        MatrixClient
            GIT_REPOSITORY https://github.com/Nheko-Reborn/mtxclient.git
            GIT_TAG        v0.10.1
    )
    set(BUILD_LIB_EXAMPLES OFF CACHE INTERNAL "")
    set(BUILD_LIB_TESTS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(MatrixClient)

    # Apply the local compatibility patch for bundled MatrixClient sources.
    # Upstream PR: https://github.com/Nheko-Reborn/mtxclient/pull/113
    # Drop this local patch once a released mtxclient tag includes that fix.
    # This handles both normal FetchContent checkouts and external source dirs
    # (for example via FETCHCONTENT_SOURCE_DIR_MATRIXCLIENT in Flatpak builds).
    execute_process(
        COMMAND git apply --whitespace=nowarn --check ${MTXCLIENT_PATCH_FILE}
        WORKING_DIRECTORY ${matrixclient_SOURCE_DIR}
        RESULT_VARIABLE MTXCLIENT_PATCH_CHECK_RESULT
        OUTPUT_QUIET
        ERROR_QUIET)
    if(MTXCLIENT_PATCH_CHECK_RESULT EQUAL 0)
        execute_process(
            COMMAND git apply --whitespace=nowarn ${MTXCLIENT_PATCH_FILE}
            WORKING_DIRECTORY ${matrixclient_SOURCE_DIR}
            RESULT_VARIABLE MTXCLIENT_PATCH_APPLY_RESULT)
        if(NOT MTXCLIENT_PATCH_APPLY_RESULT EQUAL 0)
            message(FATAL_ERROR "Failed to apply MatrixClient compatibility patch.")
        endif()
    else()
        execute_process(
            COMMAND git apply -R --check ${MTXCLIENT_PATCH_FILE}
            WORKING_DIRECTORY ${matrixclient_SOURCE_DIR}
            RESULT_VARIABLE MTXCLIENT_PATCH_REVERSE_CHECK_RESULT
            OUTPUT_QUIET
            ERROR_QUIET)
        if(NOT MTXCLIENT_PATCH_REVERSE_CHECK_RESULT EQUAL 0)
            message(FATAL_ERROR "MatrixClient sources have unexpected state; local compatibility patch cannot be applied.")
        endif()
    endif()
else()
    find_package(MatrixClient 0.10.0 REQUIRED)
endif()

option(USE_BUNDLED_LITEHTML "Use the bundled version of litehtml." ON)

if(USE_BUNDLED_LITEHTML)
    include(FetchContent)
    FetchContent_Declare(
        litehtml
        GIT_REPOSITORY https://github.com/litehtml/litehtml.git
        GIT_TAG        v0.9
    )
    set(LITEHTML_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(litehtml)

    # Suppress compiler warnings from litehtml's own sources and headers
    target_compile_options(litehtml PRIVATE -w)
    target_compile_options(gumbo PRIVATE -w)
    get_target_property(_litehtml_inc litehtml INTERFACE_INCLUDE_DIRECTORIES)
    if(_litehtml_inc)
        set_target_properties(litehtml PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_litehtml_inc}")
    endif()
else()
    find_package(litehtml REQUIRED)
endif()

if(VOIP)
    include(FindPkgConfig)
    pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.18 gstreamer-webrtc-1.0>=1.18 gstreamer-gl-1.0)
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
    set(SRC_FILES ${SRC_FILES} src/notifications/NotificationManagerProxy.h src/notifications/MacNotificationDelegate.h src/notifications/MacNotificationDelegate.mm src/notifications/ManagerMac.mm src/notifications/ManagerMac.cpp)
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
        set_source_files_properties( src/notifications/NotificationManagerProxy.h src/notifications/MacNotificationDelegate.h src/notifications/MacNotificationDelegate.mm src/notifications/ManagerMac.mm PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    endif()
elseif(WIN32)
    file(DOWNLOAD
        "https://raw.githubusercontent.com/mohabouje/WinToast/v1.3.0/src/wintoastlib.cpp"
        ${PROJECT_SOURCE_DIR}/src/wintoastlib.cpp
        EXPECTED_HASH SHA256=1ddf8ee2604e1757d383a2f60d4d597c4f5d9496369e6a22586cedf4af77ca8f)

    file(DOWNLOAD
        "https://raw.githubusercontent.com/mohabouje/WinToast/v1.3.0/include/wintoastlib.h"
        ${PROJECT_SOURCE_DIR}/src/wintoastlib.h
        EXPECTED_HASH SHA256=08cb30d28db9c851d3d4259ae6108ce7dafa966957df6735a06cd55e0716fded)

    set(SRC_FILES ${SRC_FILES} src/notifications/ManagerWin.cpp src/wintoastlib.cpp src/wintoastlib.h)
else()
    set(SRC_FILES ${SRC_FILES}
        src/dbus/Api.h
        src/dbus/Backend.h
        src/dbus/Api.cpp
        src/dbus/Backend.cpp
        src/notifications/ManagerLinux.cpp
    )
endif()

# Generate ThemeDefinitions.h from theme YAML files.
# Re-runs automatically when any theme YAML changes.
file(GLOB THEME_YAML_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes/*.yml")
set(THEME_DEFINITIONS_H "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/ThemeDefinitions.h")
add_custom_command(
    OUTPUT "${THEME_DEFINITIONS_H}"
    COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/theme/generate.py"
        "${THEME_DEFINITIONS_H}"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/themes"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/theme/generate.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/theme/colors.py"
        ${THEME_YAML_FILES}
    COMMENT "Generating ThemeDefinitions.h from theme YAML files"
    VERBATIM
)

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
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/extra_emoji.txt"
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
    ${META_FILES_TO_INCLUDE}
    ${THEME_DEFINITIONS_H})

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

    if(HAVE_BACKTRACE_SYMBOLS_FD AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        set_target_properties(komai PROPERTIES ENABLE_EXPORTS ON)
    endif()
endif()

set_target_properties(komai
    PROPERTIES
        CMAKE_SKIP_INSTALL_RPATH TRUE
        AUTOMOC ON)
target_compile_definitions(komai PRIVATE KOMAI_WITH_QML=1)
add_dependencies(komai emoji_runtime_data)
qt_add_resources(komai emoji_runtime_resources
    PREFIX "/emoji"
    BASE "${EMOJI_DATA_DIR}"
    FILES
        "${EMOJI_CORE_JSON}"
        ${EMOJI_LOCALE_JSONS}
)

#
# Add qml files
#
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()
set(QML_SOURCES_EXCLUDE
)
file(GLOB_RECURSE QML_SOURCES
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "resources/qml/*.qml"
)
list(SORT QML_SOURCES)
foreach(qml_source_exclude IN LISTS QML_SOURCES_EXCLUDE)
    list(REMOVE_ITEM QML_SOURCES "${qml_source_exclude}")
endforeach()
qt_add_qml_module(komai
    URI cc.etke.komai
    NO_RESOURCE_TARGET_PATH
    RESOURCE_PREFIX "/"
    VERSION 1.1
    DEPENDENCIES QtQuick QtQml.Models
    QML_FILES
    ${QML_SOURCES}
    SOURCES
    src/models/UserDirectoryModel.cpp
    src/models/UserDirectoryModel.h
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
    if(MSVC)
        target_compile_options(komai PUBLIC "/Zc:__cplusplus")
    endif()
else()
    target_link_libraries(komai PRIVATE Qt6::DBus)
    if(FLATPAK)
        target_compile_definitions(komai PRIVATE KOMAI_FLATPAK)
    endif()
endif()

target_include_directories(komai PRIVATE src src/auth/ src/providers/ src/avatars/default/ src/imagepacks/ src/models/ src/powerlevels/ src/cli/ src/timeline/ src/timeline/litehtml/ src/ui/ src/encryption/ src/voip/)

if(USE_BUNDLED_CPPHTTPLIB)
    target_include_directories(komai PRIVATE third_party/cpp-httplib-0.5.12)
    target_sources(komai PRIVATE third_party/cpp-httplib-0.5.12/httplib.h)
else()
    find_package(httplib REQUIRED)
    target_link_libraries(komai PRIVATE httplib::httplib)
endif()

if(USE_BUNDLED_BLURHASH)
    target_include_directories(komai PRIVATE third_party/blurhash)
    set(BLURHASH_SRC_FILES
        third_party/blurhash/blurhash.cpp
        third_party/blurhash/blurhash.hpp
    )
    target_sources(komai PRIVATE ${BLURHASH_SRC_FILES})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(blurhash REQUIRED IMPORTED_TARGET blurhash)
    target_link_libraries(komai PRIVATE PkgConfig::blurhash)
endif()

# Fixup bundled keychain include dirs
if(USE_BUNDLED_QTKEYCHAIN)
    target_include_directories(komai PRIVATE ${qt6keychain_SOURCE_DIR} ${qt6keychain_BINARY_DIR})
endif()
if(USE_BUNDLED_KDSINGLEAPPLICATION)
    target_include_directories(komai PRIVATE ${kdsingleapplication_SOURCE_DIR} ${kdsingleapplication_BINARY_DIR})
endif()

if(NOT JSON_ImplicitConversions)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "JSON_USE_IMPLICIT_CONVERSIONS=\$<BOOL:OFF>;JSON_DIAGNOSTICS=\$<BOOL:OFF>"
    )
    target_compile_definitions(komai PUBLIC JSON_USE_IMPLICIT_CONVERSIONS=0)
endif()

if(USE_BUNDLED_YAMLCPP)
    set(YAMLCPP_TARGET_NAME yaml-cpp::yaml-cpp)
else()
    set(YAMLCPP_TARGET_NAME PkgConfig::yaml-cpp)
endif()

add_library(komai_db STATIC
    src/db/Catalog.cpp
    src/db/DbTypes.cpp
    src/db/DupIndex.cpp
    src/db/Factory.cpp
    src/db/InMemoryBackend.cpp
    src/db/inmemory/InMemoryBackendInternal.h
    src/db/inmemory/InMemoryCursor.cpp
    src/db/inmemory/InMemoryTxn.cpp
    src/db/MegolmIndex.cpp
    src/db/MemberInfo.cpp
    src/db/NamePolicy.cpp
    src/db/OlmSessionIndex.cpp
    src/db/OrderEntry.cpp
    src/db/ReadReceiptIndex.cpp
    src/db/RoomInfo.cpp
    src/db/Scan.cpp
    src/db/Maintenance.cpp
    src/db/StateIndex.cpp
    src/db/SyncState.cpp
    src/db/TimelineIndex.cpp
    src/db/timelineindex/TimelineIndexQueries.cpp
    src/db/timelineindex/TimelineIndexCleanup.cpp
)
target_include_directories(komai_db PUBLIC src)
target_link_libraries(komai_db PUBLIC
    Qt::Core
    nlohmann_json::nlohmann_json
    MatrixClient::MatrixClient
)
if(KOMAI_DB_ENABLE_LMDB_BACKEND)
    target_sources(komai_db PRIVATE src/db/LmdbBackend.cpp)
    target_link_libraries(komai_db PUBLIC
        lmdbxx::lmdbxx
        liblmdb::lmdb
    )
    target_compile_definitions(komai_db PRIVATE KOMAI_DB_WITH_LMDB=1)
else()
    target_compile_definitions(komai_db PRIVATE KOMAI_DB_WITH_LMDB=0)
endif()

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

    add_executable(komai_yaml_settings_test tests/YamlSettingsTest.cpp)
    target_include_directories(komai_yaml_settings_test PRIVATE src)
    target_link_libraries(komai_yaml_settings_test PRIVATE Qt::Core ${YAMLCPP_TARGET_NAME})
    add_test(NAME komai_yaml_settings_test COMMAND komai_yaml_settings_test)
    komai_configure_test(komai_yaml_settings_test "unit;settings")

    add_executable(komai_staged_load_plan_test tests/StagedLoadPlanTest.cpp)
    target_include_directories(komai_staged_load_plan_test PRIVATE src)
    target_link_libraries(komai_staged_load_plan_test PRIVATE Qt::Core ${YAMLCPP_TARGET_NAME})
    add_test(NAME komai_staged_load_plan_test COMMAND komai_staged_load_plan_test)
    komai_configure_test(komai_staged_load_plan_test "unit;settings")

    add_executable(komai_startup_settings_test
        tests/StartupSettingsTest.cpp
        tests/ProfileSecretsStartupStub.cpp
        tests/UserSettingsModelStartupStub.cpp
        src/settings/ui/UserSettingsModel.h
        tests/OlmStartupStub.cpp
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
        src/settings/ui/facade/UserSettingsTheme.cpp
        src/ui/Theme.cpp
        src/ui/ThemeRegistry.cpp
        src/settings/StartupSettings.cpp
        src/settings/SettingsController.cpp
        src/settings/SettingsControllerLoad.cpp
        src/settings/SettingsControllerSave.cpp
        src/settings/SettingsMigrations.cpp
        src/settings/SettingsPersistence.cpp
        src/settings/SettingsPersistenceLoad.cpp
        src/settings/SettingsPersistenceSave.cpp
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
        src/settings/SettingsStorageSecretsCodec.cpp
        src/settings/core/StartupConfig.cpp
        src/settings/core/SettingsSerializer.cpp
        src/settings/core/SettingsStore.cpp
        src/emoji/Provider.cpp
        src/matrix/MatrixClient.cpp
        ${THEME_DEFINITIONS_H}
    )
    target_include_directories(komai_startup_settings_test PRIVATE
        src
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(komai_startup_settings_test PRIVATE
        Qt::Core
        Qt::Qml
        Qt::Gui
        Qt::Widgets
        Qt::Quick
        ${YAMLCPP_TARGET_NAME}
    )
    target_link_libraries(komai_startup_settings_test PRIVATE
        MatrixClient::MatrixClient
        qt6keychain
        spdlog::spdlog
        fmt::fmt
    )
    set_target_properties(komai_startup_settings_test PROPERTIES AUTOMOC ON)
    add_test(NAME komai_startup_settings_test COMMAND komai_startup_settings_test)
    komai_configure_test(komai_startup_settings_test "integration;settings")

    add_executable(komai_settings_storage_test
        tests/SettingsStorageTest.cpp
        tests/LoggingStub.cpp
        tests/ProfileSecretsStub.cpp
        src/profile/Paths.cpp
        src/cache/api/CacheApiLoggers.cpp
        src/settings/SettingsPersistence.cpp
        src/settings/SettingsPersistenceLoad.cpp
        src/settings/SettingsPersistenceSave.cpp
        src/settings/SettingsStorage.cpp
        src/settings/SettingsStoragePathApi.cpp
        src/settings/SettingsStorageReaderWriterFilesystem.cpp
        src/settings/SettingsStorageReaderWriterInMemory.cpp
        src/settings/SettingsStorageSecureBackend.cpp
        src/settings/SettingsStorageSecretsCodec.cpp
    )
    target_include_directories(komai_settings_storage_test PRIVATE src)
    target_link_libraries(komai_settings_storage_test PRIVATE
        Qt::Core
        Qt::Qml
        ${YAMLCPP_TARGET_NAME}
    )
    target_link_libraries(komai_settings_storage_test PRIVATE
        qt6keychain
        spdlog::spdlog
        fmt::fmt
        MatrixClient::MatrixClient
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

    add_executable(komai_settings_serializer_core_test
        tests/SettingsSerializerCoreTest.cpp
        src/settings/core/SettingsSerializer.cpp
        src/settings/core/SettingsStore.cpp
    )
    target_include_directories(komai_settings_serializer_core_test PRIVATE src)
    target_link_libraries(komai_settings_serializer_core_test PRIVATE ${YAMLCPP_TARGET_NAME})
    add_test(NAME komai_settings_serializer_core_test COMMAND komai_settings_serializer_core_test)
    komai_configure_test(komai_settings_serializer_core_test "unit;settings")

    add_executable(komai_db_backend_test
        tests/DbBackendTest.cpp
        src/cache/crypto/CacheCryptoStructs.cpp
    )
    target_include_directories(komai_db_backend_test PRIVATE src)
    target_link_libraries(komai_db_backend_test PRIVATE
        komai_db
        MatrixClient::MatrixClient
    )
    add_test(NAME komai_db_backend_test COMMAND komai_db_backend_test)
    komai_configure_test(komai_db_backend_test "integration;db")

    add_executable(komai_matrix_store_test
        tests/MatrixStoreIntegrationTest.cpp
        tests/MatrixStoreHarnessStubs.cpp
        tests/OlmCacheStub.cpp
        tests/ProfileSecretsCacheStub.cpp
        tests/UtilsLocalUserStub.cpp
        tests/UserSettingsModelStartupStub.cpp
        src/settings/ui/UserSettingsModel.h
        tests/LoggingStub.cpp
        src/profile/Paths.cpp
        src/events/EventAccessors.cpp
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
        src/settings/ui/facade/UserSettingsTheme.cpp
        src/ui/Theme.cpp
        src/ui/ThemeRegistry.cpp
        src/settings/StartupSettings.cpp
        src/settings/SettingsController.cpp
        src/settings/SettingsControllerLoad.cpp
        src/settings/SettingsControllerSave.cpp
        src/settings/SettingsMigrations.cpp
        src/settings/SettingsPersistence.cpp
        src/settings/SettingsPersistenceLoad.cpp
        src/settings/SettingsPersistenceSave.cpp
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
        src/settings/SettingsStorageSecretsCodec.cpp
        src/settings/core/StartupConfig.cpp
        src/settings/core/SettingsSerializer.cpp
        src/settings/core/SettingsStore.cpp
        src/emoji/Provider.cpp
        src/matrix/MatrixClient.cpp
        src/cache/core/Cache.cpp
        src/cache/core/CacheCoreStorage.cpp
        src/cache/core/CacheCoreRuntime.cpp
        src/cache/setup/CacheSetup.cpp
        src/cache/lifecycle/CacheLifecycleRooms.cpp
        src/cache/lifecycle/CacheLifecycleMigrations.cpp
        src/cache/timeline/CacheTimelineRead.cpp
        src/cache/crypto/CacheCryptoSecretsStore.cpp
        src/cache/api/CacheApiLoggers.cpp
        ${THEME_DEFINITIONS_H}
    )
    target_include_directories(komai_matrix_store_test PRIVATE
        src
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_link_libraries(komai_matrix_store_test PRIVATE
        komai_db
        Qt::Core
        Qt::Qml
        Qt::Gui
        Qt::Widgets
        Qt::Quick
        ${YAMLCPP_TARGET_NAME}
    )
    target_link_libraries(komai_matrix_store_test PRIVATE
        MatrixClient::MatrixClient
        qt6keychain
        spdlog::spdlog
        fmt::fmt
    )
    set_target_properties(komai_matrix_store_test PROPERTIES AUTOMOC ON)
    add_test(NAME komai_matrix_store_test COMMAND komai_matrix_store_test)
    komai_configure_test(komai_matrix_store_test "integration;cache")

    add_executable(komai_formatted_code_block_highlighter_test
        tests/FormattedCodeBlockHighlighterTest.cpp
        src/timeline/FormattedCodeBlockHighlighter.cpp
        src/timeline/formattedcode/HtmlRenderer.cpp
        src/timeline/formattedcode/LanguageResolver.cpp
    )
    target_include_directories(komai_formatted_code_block_highlighter_test PRIVATE src)
    target_link_libraries(komai_formatted_code_block_highlighter_test PRIVATE
        Qt::Core
        Qt::Gui
        KF6::SyntaxHighlighting
    )
    add_test(NAME komai_formatted_code_block_highlighter_test COMMAND komai_formatted_code_block_highlighter_test)
    komai_configure_test(komai_formatted_code_block_highlighter_test "unit;timeline")

    add_executable(komai_formatted_message_html_processor_test
        tests/FormattedMessageHtmlProcessorTest.cpp
        src/timeline/formattedmessage/HtmlProcessor.cpp
    )
    target_include_directories(komai_formatted_message_html_processor_test PRIVATE src)
    target_link_libraries(komai_formatted_message_html_processor_test PRIVATE Qt::Core)
    add_test(NAME komai_formatted_message_html_processor_test COMMAND komai_formatted_message_html_processor_test)
    komai_configure_test(komai_formatted_message_html_processor_test "unit;timeline")

    add_executable(komai_raw_json_formatter_test
        tests/RawJsonFormatterTest.cpp
        src/timeline/formattedcode/RawJsonFormatter.cpp
        src/timeline/FormattedCodeBlockHighlighter.cpp
        src/timeline/formattedcode/HtmlRenderer.cpp
        src/timeline/formattedcode/LanguageResolver.cpp
    )
    target_include_directories(komai_raw_json_formatter_test PRIVATE src)
    target_link_libraries(komai_raw_json_formatter_test PRIVATE
        Qt::Core
        Qt::Gui
        KF6::SyntaxHighlighting
    )
    add_test(NAME komai_raw_json_formatter_test COMMAND komai_raw_json_formatter_test)
    komai_configure_test(komai_raw_json_formatter_test "unit;timeline")

    add_executable(komai_raw_message_dialog_payload_test
        tests/RawMessageDialogPayloadTest.cpp
        src/timeline/rawmessage/RawMessageDialogPayload.cpp
        src/timeline/formattedcode/RawJsonFormatter.cpp
        src/timeline/FormattedCodeBlockHighlighter.cpp
        src/timeline/formattedcode/HtmlRenderer.cpp
        src/timeline/formattedcode/LanguageResolver.cpp
    )
    target_include_directories(komai_raw_message_dialog_payload_test PRIVATE src)
    target_link_libraries(komai_raw_message_dialog_payload_test PRIVATE
        Qt::Core
        Qt::Gui
        KF6::SyntaxHighlighting
    )
    add_test(NAME komai_raw_message_dialog_payload_test COMMAND komai_raw_message_dialog_payload_test)
    komai_configure_test(komai_raw_message_dialog_payload_test "unit;timeline")

    add_executable(komai_roomlist_preview_selection_test
        tests/RoomlistPreviewSelectionTest.cpp
        src/timeline/roomlist/RoomlistPreviewSelection.cpp
    )
    target_include_directories(komai_roomlist_preview_selection_test PRIVATE src)
    target_link_libraries(komai_roomlist_preview_selection_test PRIVATE
        Qt::Core
        MatrixClient::MatrixClient
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
        spdlog::spdlog
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

    add_executable(komai_navigation_history_test
        tests/NavigationHistoryTest.cpp
        tests/LoggingStub.cpp
        src/timeline/NavigationHistory.cpp
    )
    target_include_directories(komai_navigation_history_test PRIVATE src)
    target_link_libraries(komai_navigation_history_test PRIVATE
        Qt::Core
        spdlog::spdlog
    )
    add_test(NAME komai_navigation_history_test COMMAND komai_navigation_history_test)
    komai_configure_test(komai_navigation_history_test "unit;timeline")

    add_custom_target(komai_tests DEPENDS
        komai_yaml_settings_test
        komai_staged_load_plan_test
        komai_startup_settings_test
        komai_settings_storage_test
        komai_settings_store_test
        komai_setting_input_validation_test
        komai_settings_serializer_core_test
        komai_db_backend_test
        komai_matrix_store_test
        komai_formatted_code_block_highlighter_test
        komai_formatted_message_html_processor_test
        komai_raw_json_formatter_test
        komai_raw_message_dialog_payload_test
        komai_roomlist_preview_selection_test
        komai_theme_color_utils_test
        komai_bot_detection_test
        komai_emoji_completion_search_test
        komai_navigation_history_test
    )
endif()

target_link_libraries(komai PRIVATE
    ${COEURL_TARGET_NAME}
    MatrixClient::MatrixClient
    KF6::SyntaxHighlighting
    cmark::cmark
    fmt::fmt
    spdlog::spdlog
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
    nlohmann_json::nlohmann_json
    ${YAMLCPP_TARGET_NAME}
    komai_db
    litehtml)

if(UNIX)
    # for wayland activation tokens
    target_link_libraries(komai PRIVATE Qt::GuiPrivate)
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
    qt_generate_deploy_qml_app_script(
        TARGET komai
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

if(UNIX AND NOT APPLE)
    if(FLATPAK)
        set(APPID "cc.etke.komai")
        set_target_properties(komai PROPERTIES OUTPUT_NAME "${APPID}")
    else()
        set(APPID "komai")
        set_target_properties(komai PROPERTIES OUTPUT_NAME "komai")
    endif()

    install(TARGETS komai RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "resources/komai-16.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-32.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-48.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-64.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-256.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai-512.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/512x512/apps" RENAME "${APPID}.png")
    install(FILES "resources/komai.svg" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps" RENAME "${APPID}.svg")
    install(FILES "resources/_komai" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/zsh/site-functions")

    configure_file("resources/komai.desktop.in" "resources/komai.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/komai.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications" RENAME "${APPID}.desktop")
    configure_file("resources/komai.appdata.xml.in" "resources/komai.appdata.xml" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/komai.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/metainfo" RENAME "${APPID}.appdata.xml")

    if(NOT TARGET uninstall)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)
        add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
    endif()
endif()
# vim: tabstop=4 shiftwidth=4 expandtab
