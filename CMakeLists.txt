cmake_minimum_required(VERSION 3.20)
project(glRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(thirdParty)
add_subdirectory(core)
add_subdirectory(samples)