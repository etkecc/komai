namespace komai {
inline constexpr auto version          = "${PROJECT_VERSION}";
inline constexpr auto commit_hash      = "${GIT_COMMIT_HASH}";
inline constexpr auto fluent_icons_ref = "${FLUENT_ICONS_VERSION}";
inline constexpr auto fontawesome_icons_ref = "${FONTAWESOME_ICONS_VERSION}";
inline constexpr auto matrix_sdk_rev     = "${MATRIX_SDK_REV}";
inline constexpr auto matrix_sdk_version = "${MATRIX_SDK_VERSION}";
inline constexpr auto build_os         = "${CMAKE_HOST_SYSTEM_NAME}";
inline constexpr auto enable_debug_log = ${KOMAI_ENABLE_DEBUG_LOG};
inline constexpr auto desktop_id       = "${KOMAI_DESKTOP_ID}";
inline constexpr auto desktop_icon_name = "${KOMAI_DESKTOP_ICON_NAME}";
}

#cmakedefine01 HAVE_BACKTRACE_SYMBOLS_FD
