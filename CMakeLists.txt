cmake_minimum_required(VERSION 3.14)
project(embeddings_cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Fetch cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.23.1
)

# Fetch hnswlib
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG v0.8.0
)

# Bundle SQLite3 directly
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450300.zip
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

# Make dependencies available
FetchContent_MakeAvailable(httplib hnswlib sqlite3 nlohmann_json)

# Create SQLite3 library
add_library(sqlite3_lib ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC ${sqlite3_SOURCE_DIR})

# Create executable
add_executable(${PROJECT_NAME} 
  README.md
  settings.json
  main.cpp
  include/tokenizer.h
  include/settings.h
  include/chunker.h
  include/inference.h
  include/database.h
  include/sourceproc.h
  include/httpserver.h
  include/app.h
  src/tokenizer.cpp
  src/settings.cpp
  src/chunker.cpp
  src/inference.cpp
  src/database.cpp
  src/sourceproc.cpp
  src/httpserver.cpp
  src/app.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    httplib::httplib
    sqlite3_lib
    nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    include/
    ${hnswlib_SOURCE_DIR}
)

# Enable threading (required by hnswlib)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads )

if(WIN32)
    #set(OPENSSL_PATH "C:/OpenSSL-Win64")
    #set(OPENSSL_PATH "C:/Program Files/OpenSSL-Win64")
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
        message(STATUS "OpenSSL found - HTTPS support enabled")
        message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
        message(STATUS "  OpenSSL include: ${OPENSSL_INCLUDE_DIR}")
        message(STATUS "  OpenSSL libraries: ${OPENSSL_LIBRARIES}")
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
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
