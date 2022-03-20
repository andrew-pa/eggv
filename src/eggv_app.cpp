#include "eggv_app.h"
#include "glm/gtx/quaternion.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imnodes.h"
#include "vk_mem_alloc.h"
#include "uuid.h"
#include "mesh_gen.h"
#include "deferred_nodes.h"
#include "geometry_set.h"

std::vector<std::shared_ptr<trait_factory>> eggv_app::collect_factories() {
    return std::vector<std::shared_ptr<trait_factory>>{
        std::make_shared<transform_trait_factory>(),
            std::make_shared<mesh_trait_factory>(),
            std::make_shared<light_trait_factory>(),
            std::make_shared<camera_trait_factory>(),
            std::make_shared<rigid_body_trait_factory>(&phys_cmmn, phys_world)
    };
}

std::shared_ptr<scene> eggv_app::create_test_scene() {
    auto s = std::make_shared<scene>(collect_factories(), std::make_shared<scene_object>("Root"));
    return s;
}

eggv_cmdline_args::eggv_cmdline_args(int argc, const char* argv[])
    : resolution(1920, 1080)
{
    for(int i = 0; i < argc; ++i) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'r': {
                    float w = std::atof(argv[++i]);
                    float h = std::atof(argv[++i]);
                    this->resolution = vec2(w, h);
                } break;

                case 's':
                    this->scene_path = argv[++i];
                    break;
                case 'g':
                    this->render_graph_path = argv[++i];
                    break;
            }
        }
    }
}

eggv_app::eggv_app(const eggv_cmdline_args& args)
    : app("erg", args.resolution),
        current_scene(nullptr), r(), gui_visible(true), ui_key_cooldown(0.f),
        cam_mouse_enabled(false), phys_cmmn(), physics_sim_time(0),
      gui_open_windows({
        {"Renderer", false},
        {"Scene", true},
        {"Geometry Sets", false},
        {"Materials", false},
        {"Selected Object", true},
        {"ImGui Demo", false},
        {"ImGui Metrics", false},
        {"Physics World", false}
      })
{
    r.init(dev.get());

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 256) // for ImGUI
    };

    desc_pool = dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1024, (uint32_t)pool_sizes.size(), pool_sizes.data()
    });

    this->init_swapchain_depd();
    this->init_gui();

    phys_world = phys_cmmn.createPhysicsWorld();

    r.prototypes.emplace_back(std::make_shared<gbuffer_geom_render_node_prototype>(dev.get(), &r));
    r.prototypes.emplace_back(std::make_shared<directional_light_render_node_prototype>(dev.get()));
    r.prototypes.emplace_back(std::make_shared<directional_light_shadowmap_render_node_prototype>(dev.get()));
    r.prototypes.emplace_back(std::make_shared<point_light_render_node_prototype>(dev.get()));
    r.prototypes.emplace_back(std::make_shared<physics_debug_shape_render_node_prototype>(dev.get(), phys_world));

    if(args.scene_path.has_value()) {
        std::ifstream input(args.scene_path.value());
        json data; input >> data;
        current_scene = std::make_shared<scene>(dev.get(), collect_factories(), args.scene_path.value(), data);
    } else {
        current_scene = this->create_test_scene();
    }

    r.current_scene = current_scene;

    if(args.render_graph_path.has_value()) {
        std::ifstream input(args.render_graph_path.value());
        json data; input >> data;
        r.deserialize_render_graph(data);
        r.compile_render_graph();
    }

    auto upload_cb = std::move(dev->alloc_cmd_buffers(1)[0]);
    upload_cb->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    ImGui_ImplVulkan_CreateFontsTexture(upload_cb.get());
    upload_cb->end();
    dev->graphics_qu.submit({ vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cb.get()} }, nullptr);

    dev->graphics_qu.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    dev->clear_tmps();
}

void eggv_app::init_swapchain_depd() {
    this->init_render_pass();
    framebuffers = swapchain->create_framebuffers(gui_render_pass.get(), [&](size_t index, std::vector<vk::ImageView>& att) { }, false);
    r.create_swapchain_dependencies(swapchain.get());
    command_buffers = dev->alloc_cmd_buffers(swapchain->images.size());
}

void eggv_app::init_render_pass() {
    std::vector<vk::AttachmentDescription> attachments = {
        { vk::AttachmentDescriptionFlags(), //swapchain color
            swapchain->format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::ePresentSrcKHR },
    };

    std::vector<vk::AttachmentReference> refs {
        { 0, vk::ImageLayout::eColorAttachmentOptimal },
    };

    std::vector<vk::SubpassDescription> subpasses = {
        vk::SubpassDescription{ vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
            0, nullptr, 1, refs.data(), nullptr, nullptr }
    };

    std::vector<vk::SubpassDependency> depds = {
        { VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentRead,
            vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite },
    };

    gui_render_pass = dev->dev->createRenderPassUnique(vk::RenderPassCreateInfo{
        vk::RenderPassCreateFlags(),
        (uint32_t)attachments.size(), attachments.data(),
        (uint32_t)subpasses.size(), subpasses.data(),
        (uint32_t)depds.size(), depds.data()
    });
}

void eggv_app::init_gui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsClassic();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 6.f;
    style.TabRounding = 1.f;
    float xsc, ysc;
    glfwGetWindowContentScale(this->wnd, &xsc, &ysc);
    style.ScaleAllSizes(max(xsc, ysc));

    ImGui_ImplGlfw_InitForVulkan(this->wnd, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = this->instance;
    init_info.PhysicalDevice = this->dev->pdevice;
    init_info.Device = this->dev->dev.get();
    init_info.QueueFamily = this->dev->qu_fam.graphics;
    init_info.Queue = this->dev->graphics_qu;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = desc_pool.get();
    init_info.Allocator = nullptr;
    init_info.MinImageCount = swapchain->images.size();
    init_info.ImageCount = swapchain->images.size();
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, gui_render_pass.get());

    io.Fonts->AddFontFromFileTTF(
#if WIN32
            "C:\\Windows\\Fonts\\segoeui.ttf"
#elif APPLE
            "/System/Library/Fonts/NewYork.ttf"
#else
            "/usr/share/fonts/fira-code/FiraCode-Medium.ttf"
#endif
            , 24.0f);

    ImNodes::CreateContext();
}

void eggv_app::resize() {
    dev->present_qu.waitIdle();
    command_buffers.clear();
    framebuffers.clear();
    swapchain->recreate(this);
    this->init_swapchain_depd();
    ImGui_ImplVulkan_SetMinImageCount(swapchain->images.size());
}

void eggv_app::build_gui(frame_state* fs) {
    ImGui::Begin("Windows");
    for(auto&[name, open] : *(fs->gui_open_windows)) {
        ImGui::MenuItem(name.c_str(), nullptr, &open);
    }
    ImGui::End();
    if(fs->gui_open_windows->at("ImGui Demo")) ImGui::ShowDemoWindow(&fs->gui_open_windows->at("ImGui Demo"));
    if(fs->gui_open_windows->at("ImGui Metrics")) ImGui::ShowMetricsWindow(&fs->gui_open_windows->at("ImGui Metrics"));
    r.build_gui(fs);
    current_scene->build_gui(fs);
    build_physics_world_gui(fs, &fs->gui_open_windows->at("Physics World"), phys_world);
}

void eggv_app::update(float t, float dt) {
    frame_state fs(t, dt, current_scene, &gui_open_windows);
    current_scene->update(&fs, this);
    r.update(&fs);

    physics_sim_time += dt;
    while (physics_sim_time > physics_fixed_time_step) {
        phys_world->update(physics_fixed_time_step);
        physics_sim_time -= physics_fixed_time_step;
    }

    if(ui_key_cooldown <= 0.f) {
        if (glfwGetKey(this->wnd, GLFW_KEY_F2) == GLFW_PRESS) {
            gui_visible = !gui_visible;
            ui_key_cooldown = 0.2f;
        }
        else if (glfwGetKey(this->wnd, GLFW_KEY_F3) == GLFW_PRESS) {
            cam_mouse_enabled = !cam_mouse_enabled;
            ui_key_cooldown = 0.2f;
            if (cam_mouse_enabled) {
                glfwSetInputMode(this->wnd, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            } else {
                glfwSetInputMode(this->wnd, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        else if (glfwGetKey(this->wnd, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(this->wnd, GLFW_TRUE);
        }
    } else {
        ui_key_cooldown -= dt;
    }

    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) && current_scene->active_camera) {
        static float speed = 5.0f;
        auto cam = (camera_trait*)current_scene->active_camera->traits.find(TRAIT_ID_CAMERA)->second.get();
        auto trf = (transform_trait*)current_scene->active_camera->traits.find(TRAIT_ID_TRANSFORM)->second.get();
        mat3 rot = glm::toMat3(trf->rotation);
        auto& look = rot[2];
        auto& right = rot[0];
        auto& up = rot[1];
        if(glfwGetKey(wnd, GLFW_KEY_W) != GLFW_RELEASE) {
            trf->translation -= speed*look*dt;
        } else if(glfwGetKey(wnd, GLFW_KEY_S) != GLFW_RELEASE) {
            trf->translation += speed*look*dt;
        }
        if(glfwGetKey(wnd, GLFW_KEY_A) != GLFW_RELEASE) {
            trf->translation -= speed*right*dt;
        } else if(glfwGetKey(wnd, GLFW_KEY_D) != GLFW_RELEASE) {
            trf->translation += speed*right*dt;
        }
        if(glfwGetKey(wnd, GLFW_KEY_Q) != GLFW_RELEASE) {
            trf->translation -= speed*up*dt;
        } else if(glfwGetKey(wnd, GLFW_KEY_E) != GLFW_RELEASE) {
            trf->translation += speed*up*dt;
        }
        if(glfwGetKey(wnd, GLFW_KEY_1) != GLFW_RELEASE) {
            speed += 1.f;
        } else if(glfwGetKey(wnd, GLFW_KEY_2) != GLFW_RELEASE) {
            speed -= 1.f;
        }
        if(glfwGetKey(wnd, GLFW_KEY_R) != GLFW_RELEASE) {
            trf->rotation = quat(0.f, 0.f, 0.f, 1.f);
        }

        if(cam_mouse_enabled) {
            static double last_xpos = 0, last_ypos = 0;
            double xpos, ypos;
            glfwGetCursorPos(wnd, &xpos, &ypos);
            vec2 sz = vec2(size());
            vec2 np = ((vec2(xpos - last_xpos, ypos - last_ypos) / sz)) * pi<float>()/2.f;
            last_xpos = xpos; last_ypos = ypos;
            trf->rotation = glm::angleAxis(np.x, vec3(0.f, 1.f, 0.f)) * trf->rotation;
            trf->rotation = trf->rotation * glm::angleAxis(np.y, vec3(1.f ,0.f, 0.f));
            trf->rotation = glm::normalize(trf->rotation);
        }
    }
}

vk::CommandBuffer eggv_app::render(float t, float dt, uint32_t image_index) {
    auto& cb = command_buffers[image_index];
    cb->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    frame_state fs(t, dt, current_scene, &gui_open_windows);
    r.render(cb.get(), image_index, &fs);

    if(gui_visible) {
        cb->beginRenderPass(
            vk::RenderPassBeginInfo{
                gui_render_pass.get(), framebuffers[image_index].get(),
                vk::Rect2D(vk::Offset2D(), swapchain->extent), 0, nullptr
            },
            vk::SubpassContents::eInline
        );

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        this->build_gui(&fs);
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb.get());
        cb->endRenderPass();
    }

	cb->end();

    return cb.get();
}

eggv_app::~eggv_app() {
    current_scene.reset();
    r.current_scene.reset();
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();
}
