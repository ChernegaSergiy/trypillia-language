cmake_minimum_required(VERSION 3.10)
project(trypillia)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(trypillia ${SOURCES})
find_package(CURL)
if(CURL_FOUND)
    target_link_libraries(trypillia CURL::libcurl)
    target_compile_definitions(trypillia PRIVATE HAS_CURL=1)
else()
    message(STATUS "CURL not found. Fetching and building CURL from source...")
    include(FetchContent)
    
    if(NOT WIN32 AND NOT APPLE)
        message(STATUS "Fetching mbedTLS for CURL SSL backend...")
        FetchContent_Declare(
          mbedtls
          GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
          GIT_TAG        v3.6.0
        )
        set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
        set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(mbedtls)
        set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
    elseif(WIN32)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    elseif(APPLE)
        set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(
      curl
      GIT_REPOSITORY https://github.com/curl/curl.git
      GIT_TAG        curl-8_8_0
    )
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(curl)

    target_link_libraries(trypillia libcurl)
    target_compile_definitions(trypillia PRIVATE HAS_CURL=1)
endif()
