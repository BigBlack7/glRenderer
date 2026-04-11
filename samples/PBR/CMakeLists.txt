project(PBR)

file(GLOB_RECURSE MAIN CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_executable(PBR ${MAIN})

target_link_libraries(PBR PRIVATE core)
