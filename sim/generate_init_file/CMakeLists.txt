cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-O3)
find_package(OpenMP)

# Find LLVM
find_package(LLVM 14 REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

message(STATUS "${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/../include")
include_directories("/staff/haoxiaoyu/uarch_exp/pkgs/yaml-cpp/include")
set(YAML_CPP_LIB "/staff/haoxiaoyu/uarch_exp/pkgs/yaml-cpp/build/libyaml-cpp.a")

# Add the executable
add_executable(generate_init_file generate_init_file.cpp ../src/task/basicblock.cpp ../src/task/dyn_insn.cpp ../src/task/instruction.cpp ../src/task/function.cpp ../src/task/dyn_insn.cpp)

message(STATUS "Found LLVM version: ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader native passes)
target_link_libraries(generate_init_file ${llvm_libs})
target_link_libraries(generate_init_file  OpenMP::OpenMP_CXX)