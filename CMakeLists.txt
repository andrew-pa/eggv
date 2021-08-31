cmake_minimum_required(VERSION 3.9)

project(eggv VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

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

add_subdirectory(depd/assimp)

add_subdirectory(depd/mio)

add_subdirectory(depd/reactphysics3d)

#############################
###    compile shaders    ###

set(COMPILED_SHADERS "")
function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    message("${SHADER_STAGE} ${CMAKE_SOURCE_DIR}/${INPUT_FILE} ${OUTPUT_FILE}")
    source_group(shaders ${INPUT_FILE})
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND glslc -fshader-stage=${SHADER_STAGE} ${CMAKE_SOURCE_DIR}/${INPUT_FILE} -o ${OUTPUT_FILE}
        MAIN_DEPENDENCY ${INPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR} VERBATIM)
endfunction()

add_spirv_shader(vertex src/shaders/simple.vert.glsl simple.vert.spv)
add_spirv_shader(vertex src/shaders/full.vert.glsl full.vert.spv)
add_spirv_shader(fragment src/shaders/simple.frag.glsl simple.frag.spv)
add_spirv_shader(fragment src/shaders/gbuffer.frag.glsl gbuffer.frag.spv)
add_spirv_shader(vertex src/shaders/entire-screen.vert.glsl entire-screen.vert.spv)
add_spirv_shader(fragment src/shaders/directional-light.frag.glsl directional-light.frag.spv)
add_spirv_shader(vertex src/shaders/point-light.vert.glsl point-light.vert.spv)
add_spirv_shader(fragment src/shaders/point-light.frag.glsl point-light.frag.spv)
add_spirv_shader(fragment src/shaders/solid-color.frag.glsl solid-color.frag.spv)

message("${COMPILED_SHADERS}")

#############################
###    targets            ###

include_directories(inc/ depd/glm depd/vk_mem_alloc/include/
    depd/imgui/ depd/imgui/backends/ depd/imnodes/
    depd/ImGuiFileDialog/
    depd/stb/)

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
    depd/ImGuiFileDialog/ImGuiFileDialog.h depd/ImGuiFileDialog/ImGuiFileDialog.cpp
    depd/ImGuiFileDialog/ImGuiFileDialogConfig.h
    depd/imnodes/imnodes.h depd/imnodes/imnodes_internal.h depd/imnodes/imnodes.cpp)

add_library(imgui ${IMGUI_SRC_FILES})
target_compile_features(imgui PRIVATE cxx_std_17)
target_link_libraries(imgui glfw Vulkan::Vulkan)

add_library(stb src/stb_impl.cpp)
target_compile_features(stb PRIVATE cxx_std_17)

add_executable(eggv inc/app.h inc/device.h inc/cmmn.h inc/swap_chain.h
    src/app.cpp src/device.cpp src/main.cpp src/swap_chain.cpp
    inc/mem_arena.h
    inc/renderer.h src/renderer.cpp inc/mesh.h
    inc/deferred_nodes.h src/deferred_nodes.cpp
    inc/debug_shapes.h src/debug_shapes.cpp
    inc/mesh_gen.h inc/par_shapes.h src/mesh_gen.cpp
    inc/geometry_set.h src/geometry_set.cpp
    inc/scene_graph.h src/scene_graph.cpp
    inc/eggv_app.h src/eggv_app.cpp
    inc/physics.h src/physics.cpp
    simple.vert.spv simple.frag.spv full.vert.spv gbuffer.frag.spv
    entire-screen.vert.spv directional-light.frag.spv point-light.vert.spv
    point-light.frag.spv solid-color.frag.spv)
target_link_libraries(eggv glfw Vulkan::Vulkan imgui nlohmann_json::nlohmann_json
    stduuid mio::mio stb ReactPhysics3D::reactphysics3d)
target_compile_features(eggv PUBLIC cxx_std_17)

add_executable(eggv_import inc/app.h inc/device.h inc/cmmn.h inc/swap_chain.h
    src/app.cpp src/device.cpp src/swap_chain.cpp
    inc/mem_arena.h
    inc/renderer.h src/renderer.cpp inc/mesh.h
    inc/deferred_nodes.h src/deferred_nodes.cpp
    inc/debug_shapes.h src/debug_shapes.cpp
    inc/mesh_gen.h inc/par_shapes.h src/mesh_gen.cpp
    inc/geometry_set.h src/geometry_set.cpp
    inc/scene_graph.h src/scene_graph.cpp
    src/import.cpp)
target_compile_features(eggv_import PUBLIC cxx_std_17)
target_link_libraries(eggv_import glfw Vulkan::Vulkan imgui nlohmann_json::nlohmann_json stduuid assimp mio::mio)




