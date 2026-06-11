cmake_minimum_required(VERSION 3.10)
project(trypillia)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(trypillia ${SOURCES})
if(DEFINED ARTIFACT_NAME)
    set_target_properties(trypillia PROPERTIES OUTPUT_NAME "${ARTIFACT_NAME}")
endif()

