#pragma once
#include "app.h"
#include "imgui.h"

#include "renderer.h"

class eggv_app : public app {
    vk::UniqueDescriptorPool desc_pool;
    vk::UniqueRenderPass gui_render_pass;

    std::vector<vk::UniqueCommandBuffer> command_buffers;
    std::vector<vk::UniqueFramebuffer> framebuffers;

    std::vector<std::tuple<std::string, void*>> plugins;

    std::shared_ptr<scene> current_scene;
    renderer r;

    bool gui_visible;

    void init_swapchain_depd();

    void init_render_pass();
    void init_gui();

    void build_gui();

    float ui_key_cooldown;
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
