cmake_minimum_required(VERSION 3.16)
project(phenixcode-core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.26.0
)

FetchContent_Declare(
  hnswlib
  GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
  GIT_TAG v0.8.0
)

FetchContent_Declare(
  sqlite3
  URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_Declare(
  utils_log
  GIT_REPOSITORY https://github.com/nesall/utils-log.git
  GIT_TAG main
)

# Making sure jwt-cpp does not pull nlohmann/json (an old version)
set(JWT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
  GIT_TAG v0.7.1  # or latest stable
)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.2.1 
)
FetchContent_MakeAvailable(fmt)

FetchContent_MakeAvailable(httplib hnswlib sqlite3 utils_log jwt-cpp nlohmann_json)

# Create SQLite3 library
add_library(sqlite3_lib ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})

# logging
add_library(utils_log INTERFACE)
target_include_directories(utils_log INTERFACE ${utils_log_SOURCE_DIR})


# Create executable
add_executable(${PROJECT_NAME} 
  README.md
  settings.json
  public/setup/index.html
  include/tokenizer.h
  include/settings.h
  include/chunker.h
  include/inference.h
  include/database.h
  include/sourceproc.h
  include/httpserver.h
  include/app.h
  include/auth.h
  include/instregistry.h
  src/main.cpp
  src/tokenizer.cpp
  src/settings.cpp
  src/chunker.cpp
  src/inference.cpp
  src/database.cpp
  src/sourceproc.cpp
  src/httpserver.cpp
  src/app.cpp
  src/auth.cpp
  src/instregistry.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
  httplib::httplib
  sqlite3_lib
  nlohmann_json::nlohmann_json
  utils_log
  jwt-cpp::jwt-cpp
  fmt::fmt-header-only
)

# Include directories
target_include_directories(${PROJECT_NAME} 
  PRIVATE 
  include/
  ${hnswlib_SOURCE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    FMT_HEADER_ONLY
    FMT_USE_LOCALE=0
)

# Enable threading (required by hnswlib)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads )

set(OPENSSL_PATH $ENV{OPENSSL_PATH})

if(WIN32)
  set(OPENSSL_INCLUDE_DIR "${OPENSSL_PATH}/include")
  set(OPENSSL_CRYPTO_LIBRARY "${OPENSSL_PATH}/lib/libcrypto.lib")
  set(OPENSSL_SSL_LIBRARY "${OPENSSL_PATH}/lib/libssl.lib")
endif()

# Optional OpenSSL support
option(NO_OPENSSL "Disable OpenSSL support" OFF)

# Optional OpenSSL support
if(NOT NO_OPENSSL)
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found - HTTPS support enabled." )
    message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
    message(STATUS "  OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "  OpenSSL libraries: ${OPENSSL_LIBRARIES}")
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT HAS_OPENSSL)
  else()
    message(WARNING "OpenSSL not found - HTTPS support disabled")
    if(WIN32)
      message(WARNING "  Hint: Install from https://slproweb.com/products/Win32OpenSSL.html")
      message(WARNING "  Or set: cmake -DOPENSSL_PATH=\"C:/Program Files/OpenSSL-Win64\"")
    endif()
  endif()
else()
  message(STATUS "NO_OPENSSL is set - HTTPS support disabled")
endif()

# Platform-specific settings
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 wsock32)
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(OPENSSL_BIN_DIR "${OPENSSL_PATH}/bin")
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenSSL DLLs..."
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${OPENSSL_BIN_DIR}/libcrypto-3-x64.dll"
          "${OPENSSL_BIN_DIR}/libssl-3-x64.dll"
          $<$<CONFIG:Release>:$<TARGET_FILE_DIR:${PROJECT_NAME}>>
  )
endif()

add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/public/
      ${CMAKE_BINARY_DIR}/public/
  COMMENT "Copying setup UI files"
)