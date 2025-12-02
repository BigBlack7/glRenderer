add_library(tinyexr
    tinyexr.cc
    miniz/miniz.c
)

target_include_directories(tinyexr
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}        # tinyexr.h, tinyexr.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/miniz  # miniz.h
)

target_compile_features(tinyexr PUBLIC cxx_std_20)