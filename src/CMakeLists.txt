
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE SOURCE  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
                          ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)

add_executable(Aidanic ${HEADERS} ${SOURCE} ${SHADERS})

# visual studio
source_group("shaders" FILES ${SHADERS})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Aidanic)

target_link_libraries(Aidanic glfw)
target_link_libraries(Aidanic ${VULKAN_LIBRARY})
