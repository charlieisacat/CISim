# cmake_minimum_required(VERSION 3.10)
# find_package(LLVM REQUIRED CONFIG)
# add_definitions(${LLVM_DEFINITIONS})
# include_directories(${LLVM_INCLUDE_DIRS})
# link_directories(${LLVM_LIBRARY_DIRS})
# set(CMAKE_CXX_STANDARD 17)

# include_directories(${Boost_INCLUDE_DIRS})

# add_executable(example main.cpp)

# target_link_libraries(example ${Boost_LIBRARIES})
# target_link_libraries(example ${llvm_libs})

# cmake_minimum_required(VERSION 3.10)
# project(MyProject)

# Set the C++ standard to C++11
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost
find_package(Boost 1.70 REQUIRED COMPONENTS system filesystem)

if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "Boost not found")
endif()

# Find LLVM
find_package(LLVM REQUIRED CONFIG)

if(LLVM_FOUND)
    message(STATUS "LLVM found: ${LLVM_INCLUDE_DIRS}")
    include_directories(${LLVM_INCLUDE_DIRS})
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(AddLLVM)
else()
    message(FATAL_ERROR "LLVM not found")
endif()

link_directories(${CMAKE_SOURCE_DIR}/DRAMSim2)
find_package(yaml-cpp REQUIRED)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

# Add the executable
add_executable(CISim ${SOURCES})

add_custom_command(
  TARGET CISim PRE_BUILD
  COMMAND make libdramsim.so
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/DRAMSim2/)

# Link with Boost libraries
target_link_libraries(CISim ${Boost_LIBRARIES})

# Link with LLVM libraries
llvm_map_components_to_libnames(llvm_libs support core irreader native passes)
target_link_libraries(CISim ${llvm_libs})
target_link_libraries(CISim yaml-cpp)
target_link_libraries(CISim dramsim)