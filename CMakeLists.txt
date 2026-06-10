cmake_minimum_required(VERSION 3.10)
project(trypillia)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(trypillia ${SOURCES})
find_package(CURL REQUIRED)
target_link_libraries(trypillia CURL::libcurl)
