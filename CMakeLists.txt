cmake_minimum_required(VERSION 3.10)
project(Aidanic VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)

set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(vendor/glfw)
add_subdirectory(vendor/glm)

find_package(Vulkan REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/vendor/glfw/include
    ${PROJECT_SOURCE_DIR}/vendor/glm/glm
    ${PROJECT_SOURCE_DIR}/vendor/spdlog/include
    ${Vulkan_INCLUDE_DIRS})

add_subdirectory(src)
