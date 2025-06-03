cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-O3)

# Find LLVM
find_package(LLVM 14 REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Add the executable
add_executable(bb_rank bb_rank.cpp)

message(STATUS "Found LLVM version: ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader native passes)
target_link_libraries(bb_rank ${llvm_libs})