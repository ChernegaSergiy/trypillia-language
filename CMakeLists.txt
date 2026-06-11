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
    message(WARNING "CURL not found! HTTP module will be disabled.")
endif()
