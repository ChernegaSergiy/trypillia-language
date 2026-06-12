cmake_minimum_required(VERSION 3.10)
project(trypillia)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE CORE_SOURCES "src/*.cpp")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/lsp/LSPMain.cpp")
list(REMOVE_ITEM CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/lsp/LSP.cpp")

include(FetchContent)
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.0
)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

add_executable(trypillia src/main.cpp ${CORE_SOURCES})
target_link_libraries(trypillia mbedtls mbedx509 mbedcrypto)

add_executable(trypillia-lsp src/lsp/LSPMain.cpp src/lsp/LSP.cpp ${CORE_SOURCES})
target_link_libraries(trypillia-lsp mbedtls mbedx509 mbedcrypto)

# Explicitly link atomic for 32-bit ARM (like Android/Termux armv7)
include(CheckLibraryExists)
check_library_exists(atomic __atomic_fetch_add_4 "" HAVE_LIBATOMIC)
if(HAVE_LIBATOMIC OR ANDROID OR CMAKE_SYSTEM_NAME MATCHES "Android|Linux")
    target_link_libraries(trypillia atomic)
    target_link_libraries(trypillia-lsp atomic)
endif()

# Explicitly link filesystem for older compilers (Clang 9 in Termux)
find_library(CXXFS_LIB NAMES c++fs stdc++fs)
if(CXXFS_LIB)
    target_link_libraries(trypillia ${CXXFS_LIB})
    target_link_libraries(trypillia-lsp ${CXXFS_LIB})
endif()

if(DEFINED ARTIFACT_NAME)
    set_target_properties(trypillia PROPERTIES OUTPUT_NAME "${ARTIFACT_NAME}")
endif()

if(MSVC)
    target_compile_options(trypillia PRIVATE /wd4244 /wd4267)
    target_compile_definitions(trypillia PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

