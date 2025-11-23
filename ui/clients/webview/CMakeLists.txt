cmake_minimum_required(VERSION 3.16)
project(phenixcode-ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  webview
  GIT_REPOSITORY https://github.com/webview/webview
  GIT_TAG 0.12.0
)

set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.26.0
)

FetchContent_Declare(
  utils_log
  GIT_REPOSITORY https://github.com/nesall/utils-log.git
  GIT_TAG main
)

FetchContent_Declare(
  win_dark
  GIT_REPOSITORY https://github.com/nesall/WinDarkTitlebarImpl.git
  GIT_TAG main
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(webview httplib utils_log win_dark nlohmann_json)

# logging
add_library(utils_log INTERFACE)
target_include_directories(utils_log INTERFACE ${utils_log_SOURCE_DIR})

# win-dark-titlebar
add_library(win_dark INTERFACE)
target_include_directories(win_dark INTERFACE ${win_dark_SOURCE_DIR})

# Path to sibling spa-svelte project
set(SPA_CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../spa-svelte)
set(SPA_DIST_DIR ${SPA_CLIENT_DIR}/dist)

# Main executable
add_executable(${PROJECT_NAME} src/main.cpp src/procmngr.h appconfig.json app.rc)

# using fmt lib instead of std::format (for older compiler compatibility)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FMT_HEADER_ONLY
    FMT_USE_LOCALE=0
)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
	message(INFO " Win32 Release build")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()

target_link_libraries(${PROJECT_NAME} webview::core httplib::httplib utils_log win_dark nlohmann_json::nlohmann_json)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ole32 Comctl32)
elseif(APPLE)
    find_library(COCOA Cocoa)
    find_library(WEBKIT WebKit)
    find_library(COREFOUNDATION CoreFoundation REQUIRED)
    target_link_libraries(${PROJECT_NAME} ${COCOA} ${WEBKIT} ${COREFOUNDATION})
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
    target_include_directories(${PROJECT_NAME} PRIVATE ${WEBKIT2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${WEBKIT2_LIBRARIES} pthread)
endif()

# Copy pre-built web assets if they exist
if(EXISTS ${SPA_DIST_DIR})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SPA_DIST_DIR}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/web_assets
        COMMENT "Copying web assets to executable directory"
    )
else()
    message(WARNING "Web assets not found at ${SPA_DIST_DIR} - build spa-svelte manually first")
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(OPENSSL_BIN_DIR "$ENV{OPENSSL_PATH}/bin")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenSSL DLLs..."
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${OPENSSL_BIN_DIR}/libcrypto-3-x64.dll"
          "${OPENSSL_BIN_DIR}/libssl-3-x64.dll"
          $<$<CONFIG:Release>:$<TARGET_FILE_DIR:${PROJECT_NAME}>>
  )
endif()

