#pragma once
#include "app.h"
#include "imgui.h"
#include "renderer.h"
#include "physics.h"

const float physics_fixed_time_step = 1.f / 60.f;

class eggv_app : public app {
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass gui_render_pass;

    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::vector<vk::UniqueFramebuffer> framebuffers;

    std::vector<std::tuple<std::string, void*>> plugins;

    std::shared_ptr<scene> current_scene;
    renderer r;

    bool gui_visible, cam_mouse_enabled;
    std::map<std::string, bool> gui_open_windows;

    void init_swapchain_depd();

    void init_render_pass();
    void init_gui();

    std::shared_ptr<scene> create_test_scene();
    std::vector<std::shared_ptr<trait_factory>> collect_factories();

    void build_gui();

    reactphysics3d::PhysicsCommon phys_cmmn;
    reactphysics3d::PhysicsWorld* phys_world;

    float ui_key_cooldown;
    float physics_sim_time;
public:
    eggv_app(const std::vector<std::string>& cargs);

    void load_plugin(const std::string& path);

    void resize() override;
    void build_gui(frame_state* fs);
    void update(float t, float dt) override;
    vk::CommandBuffer render(float t, float dt, uint32_t image_index) override;
    void post_submit(uint32_t) override {}
    ~eggv_app();
};
