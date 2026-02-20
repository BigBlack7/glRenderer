add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# 屏蔽MSVC的sprintf安全警告
if(MSVC)
    target_compile_definitions(stb INTERFACE _CRT_SECURE_NO_WARNINGS)
endif()
