file(GLOB_RECURSE HEADERS   ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE SOURCE    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE SHADERS   ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rgen
                            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rmiss
                            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.rchit
                            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
                            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag)
set(IMGUI                   ${PROJECT_SOURCE_DIR}/vendor/imgui/imgui.cpp
                            ${PROJECT_SOURCE_DIR}/vendor/imgui/imgui_draw.cpp
                            ${PROJECT_SOURCE_DIR}/vendor/imgui/imgui_widgets.cpp)

add_executable(Aidanic ${HEADERS} ${SOURCE} ${SHADERS} ${IMGUI})

target_link_libraries(Aidanic glfw)
target_link_libraries(Aidanic ${Vulkan_LIBRARY})

set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
foreach(GLSL ${SHADERS})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(Configuration)/assets/spirv/${FILE_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(Configuration)/shaders"
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)
    
add_custom_target(Aidanic_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Aidanic Aidanic_Shaders)

# visual studio config
source_group("shaders" FILES ${SHADERS})
source_group("imgui" FILES ${IMGUI})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Aidanic)
set_property(TARGET Aidanic PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$(Configuration)")