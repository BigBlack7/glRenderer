# glm
option(GLM_BUILD_LIBRARY "Build dynamic/static library" OFF)
option(GLM_BUILD_TESTS "Build the test programs" OFF)
option(GLM_ENABLE_CXX_98 "Enable C++ 98" OFF)
option(GLM_ENABLE_CXX_11 "Enable C++ 11" OFF)
option(GLM_ENABLE_CXX_14 "Enable C++ 14" OFF)
option(GLM_ENABLE_CXX_17 "Enable C++ 17" OFF)
option(GLM_ENABLE_CXX_20 "Enable C++ 20" ON)
option(GLM_ENABLE_LANG_EXTENSIONS "Enable language extensions" OFF)
option(GLM_DISABLE_AUTO_DETECTION "Disable platform, compiler, arch and C++ language detection" OFF)
add_subdirectory(glm)

# glad
add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

# glfw
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory(glfw)

# stb
add_subdirectory(stb)

# imgui
file(GLOB IMGUI CONFIGURE_DEPENDS
    imgui/*.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)
add_library(imgui ${IMGUI})
target_include_directories(imgui PUBLIC imgui imgui/backends)
target_link_libraries(imgui PUBLIC glfw glad)

# assimp
option(ASSIMP_BUILD_SAMPLES OFF)
option(ASSIMP_BUILD_TESTS OFF)
option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
option(ASSIMP_BUILD_DOCS OFF)
option(ASSIMP_BUILD_DRACO OFF)
option(ASSIMP_WARNINGS_AS_ERRORS OFF)
option(ASSIMP_INSTALL OFF)
option(ASSIMP_NO_EXPORT OFF)
add_subdirectory(assimp)

# tinyexr
add_subdirectory(tinyexr)

# spdlog
option(SPDLOG_BUILD_EXAMPLE "Build spdlog examples" OFF)
option(SPDLOG_BUILD_EXAMPLE_HO "Build spdlog header-only examples" OFF)
option(SPDLOG_BUILD_TESTS "Build spdlog tests" OFF)
option(SPDLOG_BUILD_TESTS_HO "Build spdlog header-only tests" OFF)
option(SPDLOG_BUILD_BENCH "Build spdlog benchmark" OFF)
option(SPDLOG_INSTALL "Install spdlog" OFF)
add_subdirectory(spdlog)