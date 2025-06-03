cmake_minimum_required(VERSION 3.10)
find_package(LLVM 14 REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
set(CMAKE_CXX_STANDARD 17)

message(STATUS "Found LLVM version: ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

add_library(rename SHARED rename_pass.cpp)
add_library(instrument SHARED instrumentPass.cpp)
add_library(log_helper STATIC log_helper.cpp)

add_library(profile SHARED profile.cpp)
add_library(profile_helper STATIC profile_helper.cpp)

add_library(split SHARED split_bb.cpp)

add_library(inliner SHARED inliner.cpp)