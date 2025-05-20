cmake_minimum_required(VERSION 2.8.7)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
include(AppendCompilerFlags)
project(Ring)
set(CMAKE_PROJECT_NAME "Ring")
set(CMAKE_CXX_STANDARD 17)

#Change to "Debug" when you need it
set(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_BUILD_TYPE "Debug")

set(DYNAMIC_PATH ${CMAKE_HOME_DIRECTORY}/external/DYNAMIC)

# C++11 compiler Check
if(NOT CMAKE_CXX_COMPILER_VERSION) # work around for cmake versions smaller than 2.8.10
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE CMAKE_CXX_COMPILER_VERSION)
endif()
if(CMAKE_CXX_COMPILER MATCHES ".*clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()
if( (CMAKE_COMPILER_IS_GNUCXX AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 4.7) OR
(CMAKE_COMPILER_IS_CLANGXX AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 3.2))
    message(FATAL_ERROR "Your C++ compiler does not support C++11. Please install g++ 4.7 (or greater) or clang 3.2 (or greater)")
else()
    message(STATUS "Compiler is recent enough to support C++11.")
endif()
if( CMAKE_COMPILER_IS_GNUCXX )
    append_cxx_compiler_flags("-fpermissive -std=c++11" "GCC" CMAKE_CXX_FLAGS)
    append_cxx_compiler_flags("-msse4.2 -O3 -ffast-math" "GCC" CMAKE_CXX_FLAGS_RELEASE)
else()
    append_cxx_compiler_flags("-fpermissive -std=c++11" "CLANG" CMAKE_CXX_FLAGS)
    append_cxx_compiler_flags("-stdlib=libc++" "CLANG" CMAKE_CXX_FLAGS)
    append_cxx_compiler_flags("-msse4.2 -O3  -ffast-math" "CLANG" CMAKE_CXX_FLAGS_RELEASE)
endif()
include(CheckSSE)
FindSSE ()
if( SSE4_2_FOUND )
    if( CMAKE_COMPILER_IS_GNUCXX )
        append_cxx_compiler_flags("-msse4.2" "GCC" CMAKE_CXX_FLAGS)
    else()
        append_cxx_compiler_flags("-msse4.2" "CLANG" CMAKE_CXX_FLAGS)
    endif()
    message(STATUS "CPU does support SSE4.2.")
else()
    message(STATUS "CPU does NOT support SSE4.2")
endif()

message(STATUS "DYNAMIC PATH=${DYNAMIC_PATH}")
include_directories(~/include
                    ${DYNAMIC_PATH}/include
                    ${DYNAMIC_PATH}/build/hopscotch_map-prefix/src/hopscotch_map/include
                    ${CMAKE_HOME_DIRECTORY}/include)

link_directories(~/lib)

add_library(bitvector_amortized_lib STATIC
    src/bitvector_amortized/static.cpp
    src/bitvector_amortized/leaf.cpp
    src/bitvector_amortized/dynamic.cpp
    src/bitvector_amortized/hybrid.cpp
)

target_include_directories(bitvector_amortized_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bitvector_amortized
)

add_executable(build-index src/build-index.cpp)
target_link_libraries(build-index sdsl divsufsort divsufsort64 bitvector_amortized_lib)

add_executable(query-index src/query-index.cpp)
target_link_libraries(query-index sdsl divsufsort divsufsort64 bitvector_amortized_lib)

add_executable(delete-edge src/delete-edge.cpp)
target_link_libraries(delete-edge sdsl divsufsort divsufsort64 bitvector_amortized_lib)

add_executable(delete-node src/delete-node.cpp)
target_link_libraries(delete-node sdsl divsufsort divsufsort64 bitvector_amortized_lib)

add_executable(insert-edge src/insert-edge.cpp)
target_link_libraries(insert-edge sdsl divsufsort divsufsort64 bitvector_amortized_lib)

add_executable(update-query src/update-query.cpp)
target_link_libraries(update-query sdsl divsufsort divsufsort64)

add_executable(test-B src/test-B.cpp)
target_link_libraries(test-B sdsl divsufsort divsufsort64)