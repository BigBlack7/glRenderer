cmake_minimum_required(VERSION 3.20)
project(glRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# compiler options
if(MSVC)
    add_compile_options(/MP) # 多核CPU并行编译
    add_compile_options(/W4) # 设置 /W4 最高警告等级

    # 编译器优化: Release模式下启用/O2优化(最大速度优化)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /O2")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(thirdParty)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
add_subdirectory(core)
add_subdirectory(samples)