cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
find_package(LLVM 14 REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

find_package(yaml-cpp REQUIRED)

message(STATUS "Found LLVM version: ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR}")

# Add the executable
add_executable(adaptor packing.cpp)
add_executable(scheduler schedule.cpp)
add_executable(adaptor_sep packing_sep.cpp)

add_executable(dyser_offload_sep dyser_offloader_sep.cpp)
add_executable(dyser_offload dyser_offloader.cpp)
add_executable(cfu_offload cfu_offloader.cpp)

add_executable(generate_bblist generate_bblist.cpp)

target_link_libraries(adaptor yaml-cpp)
target_link_libraries(adaptor_sep yaml-cpp)
target_link_libraries(dyser_offload yaml-cpp)
target_link_libraries(dyser_offload_sep yaml-cpp)
target_link_libraries(cfu_offload yaml-cpp)

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader native passes)
target_link_libraries(adaptor ${llvm_libs})
target_link_libraries(adaptor_sep ${llvm_libs})
target_link_libraries(scheduler ${llvm_libs})
target_link_libraries(dyser_offload ${llvm_libs})
target_link_libraries(dyser_offload_sep ${llvm_libs})
target_link_libraries(cfu_offload ${llvm_libs})
target_link_libraries(generate_bblist ${llvm_libs})

