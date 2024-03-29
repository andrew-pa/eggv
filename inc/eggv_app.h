#pragma once
#include "app.h"
#include "ecs.h"
#include "emlisp.h"
#include "imgui.h"
#include "physics.h"
#include "renderer.h"

const float physics_fixed_time_step = 1.f / 60.f;

struct eggv_cmdline_args {
    vec2                  resolution;
    std::filesystem::path bundle_path;
    eggv_cmdline_args(int argc, const char* argv[]);
};

struct script_repl_window_t;

class eggv_app : public app {
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass     gui_render_pass;

    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::vector<vk::UniqueFramebuffer>   framebuffers;

    std::shared_ptr<world> w;

    std::shared_ptr<renderer> r;

    bool        gui_visible, cam_mouse_enabled;
    frame_state fs;

    void init_swapchain_depd();
    void init_render_pass();
    void init_gui();
    void init_script_runtime();

    void build_gui();

    reactphysics3d::PhysicsCommon phys_cmmn;
    reactphysics3d::PhysicsWorld* phys_world;

    float ui_key_cooldown;
    float physics_sim_time;

    std::unique_ptr<script_repl_window_t> script_repl_window;

  public:
    std::shared_ptr<emlisp::runtime> script_runtime;
    std::shared_ptr<bundle>          bndl;

    eggv_app(const eggv_cmdline_args& args);

    void              resize() override;
    void              update(float t, float dt) override;
    vk::CommandBuffer render(float t, float dt, uint32_t image_index) override;
    ~eggv_app() override;
};
