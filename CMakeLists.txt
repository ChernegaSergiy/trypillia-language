cmake_minimum_required(VERSION 3.10)
project(trypillia)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

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

add_executable(trypillia ${SOURCES})
target_link_libraries(trypillia mbedtls mbedx509 mbedcrypto)

if(DEFINED ARTIFACT_NAME)
    set_target_properties(trypillia PROPERTIES OUTPUT_NAME "${ARTIFACT_NAME}")
endif()

if(MSVC)
    target_compile_options(trypillia PRIVATE /wd4244 /wd4267)
    target_compile_definitions(trypillia PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

