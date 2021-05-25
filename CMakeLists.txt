cmake_minimum_required(VERSION 3.9)

project(eggv VERSION 0.1 LANGUAGES CXX)

#############################
### include dependencies ###

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(depd/glfw)

find_package(Vulkan REQUIRED)

message("${Vulkan_LIBRARY}")

# # set(USE_BINARIES OFF)
# set(USE_TESTS OFF)
# include_directories(depd/alembic/lib/)
# if(LINUX)
#     message("L")
#     include_directories(/usr/local/include/Imath)
# endif()
# add_subdirectory(depd/alembic)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(depd/json)

set(UUID_BUILD_TESTS OFF)
add_subdirectory(depd/stduuid)

#############################
###    compile shaders    ###

set(COMPILED_SHADERS "")
function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    message("${SHADER_STAGE} ${CMAKE_SOURCE_DIR}/${INPUT_FILE} ${OUTPUT_FILE}")
    source_group(shaders ${INPUT_FILE})
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND glslc -fshader-stage=${SHADER_STAGE} ${CMAKE_SOURCE_DIR}/${INPUT_FILE} -o ${OUTPUT_FILE}
        #COMMAND "notepad.exe ${CMAKE_SOURCE_DIR}/${INPUT_FILE}"
        MAIN_DEPENDENCY ${INPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR} VERBATIM)
endfunction()

add_spirv_shader(vertex src/shaders/simple.vert.glsl simple.vert.spv)
add_spirv_shader(vertex src/shaders/full.vert.glsl full.vert.spv)
add_spirv_shader(fragment src/shaders/simple.frag.glsl simple.frag.spv)
add_spirv_shader(fragment src/shaders/gbuffer.frag.glsl gbuffer.frag.spv)
add_spirv_shader(vertex src/shaders/entire-screen.vert.glsl entire-screen.vert.spv)
add_spirv_shader(fragment src/shaders/directional-light.frag.glsl directional-light.frag.spv)

message("${COMPILED_SHADERS}")

#############################
###    targets            ###

include_directories(inc/ depd/glm depd/vk_mem_alloc/src/
    depd/imgui/ depd/imgui/backends/ depd/imnodes/
    depd/ImGuiFileDialog/ImGuiFileDialog/
    depd/midifile/include/)

if(WIN32)
    include_directories(depd/dirent/include)
endif()

# ### midifile
# add_library(midifile depd/midifile/src/Binasc.cpp depd/midifile/src/MidiEvent.cpp
#     depd/midifile/src/MidiEventList.cpp depd/midifile/src/MidiFile.cpp
#     depd/midifile/src/MidiMessage.cpp)
# target_compile_features(midifile PRIVATE cxx_std_17)

### ImGui
set(IMGUI_SRC_FILES depd/imgui/imconfig.h depd/imgui/imgui.h depd/imgui/imgui.cpp depd/imgui/imgui_demo.cpp
    depd/imgui/imgui_draw.cpp depd/imgui/imgui_widgets.cpp depd/imgui/imstb_rectpack.h
    depd/imgui/imstb_textedit.h depd/imgui/imstb_truetype.h depd/imgui/imgui_tables.cpp
    depd/imgui/backends/imgui_impl_glfw.h depd/imgui/backends/imgui_impl_glfw.cpp
    depd/imgui/backends/imgui_impl_vulkan.h depd/imgui/backends/imgui_impl_vulkan.cpp
    depd/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.h depd/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.cpp
    depd/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialogConfig.h
    depd/imnodes/imnodes.h depd/imnodes/imnodes_internal.h depd/imnodes/imnodes.cpp)

add_library(imgui ${IMGUI_SRC_FILES})
target_compile_features(imgui PRIVATE cxx_std_17)
target_link_libraries(imgui glfw Vulkan::Vulkan)

add_executable(eggv inc/app.h inc/device.h inc/cmmn.h inc/swap_chain.h
    src/app.cpp src/device.cpp src/main.cpp src/swap_chain.cpp
    inc/renderer.h src/renderer.cpp
    inc/deferred_nodes.h src/deferred_nodes.cpp
    inc/mesh_gen.h inc/par_shapes.h src/mesh_gen.cpp
    inc/scene_graph.h src/scene_graph.cpp
    inc/eggv_app.h src/eggv_app.cpp
    simple.vert.spv simple.frag.spv full.vert.spv gbuffer.frag.spv entire-screen.vert.spv directional-light.frag.spv)
target_compile_features(eggv PRIVATE cxx_std_17)
target_link_libraries(eggv glfw Vulkan::Vulkan imgui nlohmann_json::nlohmann_json stduuid)



