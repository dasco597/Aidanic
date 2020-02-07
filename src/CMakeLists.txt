
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE SOURCE  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(Aidanic ${HEADERS} ${SOURCE})

target_link_libraries(Aidanic glfw)
target_link_libraries(Aidanic ${VULKAN_LIBRARY})
