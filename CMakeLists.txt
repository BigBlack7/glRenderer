cmake_minimum_required(VERSION 3.20)
project(glRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compiler options
if(MSVC)
    add_compile_options(/MP)
    add_compile_options(/W4)
endif()

# output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(thirdParty)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
add_subdirectory(core)
add_subdirectory(samples)