project(Primitives)

file(GLOB_RECURSE PRIMITIVE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_executable(Primitives ${PRIMITIVE})

target_link_libraries(Primitives PRIVATE core)