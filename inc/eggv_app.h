#pragma once
#include "app.h"
#include "ecs.h"
#include "emlisp.h"
#include "imgui.h"
#include "physics.h"
#include "renderer.h"

const float physics_fixed_time_step = 1.f / 60.f;

struct eggv_cmdline_args {
    std::optional<std::filesystem::path> render_graph_path;
    std::optional<std::filesystem::path> scene_path;
    vec2                                 resolution;
    eggv_cmdline_args(int argc, const char* argv[]);
};

struct script_repl_window_t;

class eggv_app : public app {
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass     gui_render_pass;

    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::vector<vk::UniqueFramebuffer>   framebuffers;

    std::shared_ptr<world> w;

    renderer r;

    bool                        gui_visible, cam_mouse_enabled;
    std::map<std::string, bool> gui_open_windows;

    void init_swapchain_depd();
    void init_render_pass();
    void init_gui();
    void init_script_runtime();

    std::shared_ptr<bundle>                     create_test_scene();
    std::vector<std::shared_ptr<trait_factory>> collect_factories();

    void build_gui();

    reactphysics3d::PhysicsCommon phys_cmmn;
    reactphysics3d::PhysicsWorld* phys_world;

    float ui_key_cooldown;
    float physics_sim_time;

    std::unique_ptr<script_repl_window_t> script_repl_window;

  public:
    std::shared_ptr<emlisp::runtime> script_runtime;
    std::shared_ptr<bundle>          current_scene;

    eggv_app(const eggv_cmdline_args& args);

    void              resize() override;
    void              build_gui(frame_state* fs);
    void              update(float t, float dt) override;
    vk::CommandBuffer render(float t, float dt, uint32_t image_index) override;
    ~eggv_app() override;
};
