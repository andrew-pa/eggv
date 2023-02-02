#include "eggv_app.h"
#include "deferred_nodes.h"
#include "geometry_set.h"
#include "glm/gtx/quaternion.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imnodes.h"
#include "mesh_gen.h"
#include "scene_components.h"
#include "uuid.h"
#include "vk_mem_alloc.h"

struct script_repl_window_t {
    char                     input[256];
    bool                     scroll_to_bottom;
    bool                     reclaim_focus;
    std::vector<std::string> lines;

    script_repl_window_t() : scroll_to_bottom(true), reclaim_focus(true) { memset(&input, 0, 256); }

    void build_gui(emlisp::runtime* rt, bool* visible) {
        if(*visible) {
            ImGui::Begin("Script Console", visible, ImGuiWindowFlags_MenuBar);
            if(ImGui::BeginMenuBar()) {
                if(ImGui::BeginMenu("File")) ImGui::EndMenu();
                if(ImGui::BeginMenu("Runtime")) {
                    if(ImGui::MenuItem("Collect Garbage")) {
                        emlisp::heap_info ifo;
                        rt->collect_garbage(&ifo);
                        std::ostringstream oss;
                        oss << "garbage collected " << (ifo.old_size - ifo.new_size) << " bytes";
                        lines.emplace_back(oss.str());
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::BeginChild(
                "script-console-log",
                ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
                true,
                ImGuiWindowFlags_HorizontalScrollbar
            );

            for(const auto& line : lines)
                ImGui::TextUnformatted(line.c_str());

            if(scroll_to_bottom) ImGui::SetScrollHereY(1);
            scroll_to_bottom = false;
            ImGui::EndChild();

            ImGui::PushItemWidth(-1);
            if(ImGui::InputText("##input", input, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::ostringstream oss;
                try {
                    auto inp = rt->read(input);
                    auto res = rt->eval(inp);
                    rt->write(oss, inp) << "\n\t= ";
                    rt->write(oss, res);
                } catch(emlisp::type_mismatch_error e) {
                    oss << "error: " << e.what() << ". expected: " << e.expected
                        << " actual: " << e.actual << "\n\tfrom: ";
                    rt->write(oss, e.trace) << "\n";
                } catch(std::runtime_error e) { oss << "error: " << e.what(); }
                lines.emplace_back(oss.str());
                strcpy(input, "");
                reclaim_focus    = true;
                scroll_to_bottom = true;
            }
            ImGui::PopItemWidth();
            ImGui::SetItemDefaultFocus();
            if(reclaim_focus) {
                ImGui::SetKeyboardFocusHere(-1);
                reclaim_focus = false;
            }
            ImGui::End();
        }
    }
};

eggv_cmdline_args::eggv_cmdline_args(int argc, const char* argv[]) : resolution(1920, 1080) {
    for(int i = 1; i < argc; ++i) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'r': {
                    float w          = std::atof(argv[++i]);
                    float h          = std::atof(argv[++i]);
                    this->resolution = vec2(w, h);
                } break;
                default: throw std::runtime_error(std::string("unknown option: ") + argv[i]);
            }
        } else {
            bundle_path = argv[i];
            return;
        }
    }
    throw std::runtime_error("expected bundle path");
}

eggv_app::eggv_app(const eggv_cmdline_args& args)
    : app("erg", args.resolution), w(std::make_shared<world>()), gui_visible(true),
      cam_mouse_enabled(false), ui_key_cooldown(0.f), physics_sim_time(0),
      script_repl_window(std::make_unique<script_repl_window_t>()),
      script_runtime(std::make_shared<emlisp::runtime>()) {
    r = std::make_shared<renderer>(w);
    r->init(dev.get());

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 256)  // for ImGUI
    };

    desc_pool = dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1024,
        (uint32_t)pool_sizes.size(),
        pool_sizes.data()});

    this->init_swapchain_depd();
    this->init_gui();

    phys_world = phys_cmmn.createPhysicsWorld();

    r->prototypes.emplace_back(
        std::make_shared<gbuffer_geom_render_node_prototype>(dev.get(), r.get())
    );
    r->prototypes.emplace_back(std::make_shared<directional_light_render_node_prototype>(dev.get())
    );
    r->prototypes.emplace_back(
        std::make_shared<directional_light_shadowmap_render_node_prototype>(dev.get())
    );
    r->prototypes.emplace_back(std::make_shared<point_light_render_node_prototype>(dev.get()));
    r->prototypes.emplace_back(
        std::make_shared<physics_debug_shape_render_node_prototype>(dev.get(), phys_world)
    );

    r->current_bundle = bndl = std::make_shared<bundle>();
    bndl->load(dev.get(), args.bundle_path);

    w->add_system(std::make_shared<transform_system>(w));
    w->add_system(std::make_shared<camera_system>(w));
    w->add_system(std::make_shared<light_system>(w));
    w->add_system(r);

    auto thing = w->create_entity("Thing");
    thing.add_child("thing 1");
    thing.add_child("thing 2");
    thing.add_child("thing 3");
    auto thing2 = w->create_entity("Thing 2");

    thing.add_component<transform_system>(transform_system::component_t{});

    auto default_rg = bndl->render_graphs.find("default.json");
    if(default_rg != bndl->render_graphs.end()) {
        r->deserialize_render_graph(default_rg->second);
        r->compile_render_graph();
    }

    init_script_runtime();

    auto upload_cb = std::move(dev->alloc_cmd_buffers(1)[0]);
    upload_cb->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    ImGui_ImplVulkan_CreateFontsTexture(upload_cb.get());
    upload_cb->end();
    dev->graphics_qu.submit(
        {
            vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cb.get()}
    },
        nullptr
    );

    fs.gui_open_windows["World"]           = true;
    fs.gui_open_windows["Selected Entity"] = true;
    fs.gui_open_windows["Script Console"]  = true;

    dev->graphics_qu.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    dev->clear_tmps();
}

void eggv_app::init_swapchain_depd() {
    this->init_render_pass();
    framebuffers = swapchain->create_framebuffers(
        gui_render_pass.get(), [&](size_t index, std::vector<vk::ImageView>& att) {}, false
    );
    r->create_swapchain_dependencies(swapchain.get());
    command_buffers = dev->alloc_cmd_buffers(swapchain->images.size());
}

void eggv_app::init_render_pass() {
    std::vector<vk::AttachmentDescription> attachments = {
        {vk::AttachmentDescriptionFlags(), // swapchain color
         swapchain->format,
         vk::SampleCountFlagBits::e1,
         vk::AttachmentLoadOp::eLoad,
         vk::AttachmentStoreOp::eStore,
         vk::AttachmentLoadOp::eDontCare,
         vk::AttachmentStoreOp::eDontCare,
         vk::ImageLayout::ePresentSrcKHR,
         vk::ImageLayout::ePresentSrcKHR},
    };

    std::vector<vk::AttachmentReference> refs{
        {0, vk::ImageLayout::eColorAttachmentOptimal},
    };

    std::vector<vk::SubpassDescription> subpasses = {
        vk::SubpassDescription{
                               vk::SubpassDescriptionFlags(),
                               vk::PipelineBindPoint::eGraphics,
                               0, nullptr,
                               1, refs.data(),
                               nullptr, nullptr}
    };

    std::vector<vk::SubpassDependency> depds = {
        {VK_SUBPASS_EXTERNAL,
         0, vk::PipelineStageFlagBits::eColorAttachmentOutput,
         vk::PipelineStageFlagBits::eColorAttachmentOutput,
         vk::AccessFlagBits::eColorAttachmentRead,
         vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite},
    };

    gui_render_pass = dev->dev->createRenderPassUnique(vk::RenderPassCreateInfo{
        vk::RenderPassCreateFlags(),
        (uint32_t)attachments.size(),
        attachments.data(),
        (uint32_t)subpasses.size(),
        subpasses.data(),
        (uint32_t)depds.size(),
        depds.data()});
}

void eggv_app::init_gui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsClassic();
    auto& style          = ImGui::GetStyle();
    style.WindowRounding = 6.f;
    style.TabRounding    = 1.f;

    float xsc, ysc;
    glfwGetWindowContentScale(this->wnd, &xsc, &ysc);
    style.ScaleAllSizes(max(xsc, ysc));

    ImGui_ImplGlfw_InitForVulkan(this->wnd, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance                  = this->instance;
    init_info.PhysicalDevice            = this->dev->pdevice;
    init_info.Device                    = this->dev->dev.get();
    init_info.QueueFamily               = this->dev->qu_fam.graphics;
    init_info.Queue                     = this->dev->graphics_qu;
    init_info.PipelineCache             = VK_NULL_HANDLE;
    init_info.DescriptorPool            = desc_pool.get();
    init_info.Allocator                 = nullptr;
    init_info.MinImageCount             = swapchain->images.size();
    init_info.ImageCount                = swapchain->images.size();
    init_info.CheckVkResultFn           = nullptr;
    ImGui_ImplVulkan_Init(&init_info, gui_render_pass.get());

#ifdef WIN32
    std::cout << "W";
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
#endif
#ifdef __APPLE__
    std::cout << "A";
    io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Menlo.ttc", 16.0f);
#else
    std::cout << "U";
    io.Fonts->AddFontFromFileTTF(
        "/usr/share/fonts/levien-inconsolata/Inconsolata-Regular.ttf", 16.0f
    );
#endif

    ImNodes::CreateContext();
}

void eggv_app::resize() {
    dev->present_qu.waitIdle();
    command_buffers.clear();
    framebuffers.clear();
    swapchain->recreate((app*)this);
    this->init_swapchain_depd();
    ImGui_ImplVulkan_SetMinImageCount(swapchain->images.size());
}

void eggv_app::build_gui() {
    if(ImGui::BeginPopupContextVoid("#mainmenu")) {
        if(ImGui::BeginMenu("Windows")) {
            for(auto& [name, open] : (fs.gui_open_windows))
                ImGui::MenuItem(name.c_str(), nullptr, &open);
            ImGui::EndMenu();
        }
        if(ImGui::MenuItem("Save bundle")) bndl->save();
        ImGui::EndPopup();
    }
    if(fs.gui_open_windows["ImGui Demo"])
        ImGui::ShowDemoWindow(&fs.gui_open_windows.at("ImGui Demo"));
    if(fs.gui_open_windows["ImGui Metrics"])
        ImGui::ShowMetricsWindow(&fs.gui_open_windows.at("ImGui Metrics"));
    w->build_gui(fs);
    bndl->build_gui(fs);
    script_repl_window->build_gui(script_runtime.get(), &fs.gui_open_windows["Script Console"]);
}

void eggv_app::update(float t, float dt) {
    fs.set_time(t, dt);
    w->update(fs);
    r->update(fs);

    physics_sim_time += dt;
    while(physics_sim_time > physics_fixed_time_step) {
        phys_world->update(physics_fixed_time_step);
        physics_sim_time -= physics_fixed_time_step;
    }

    if(ui_key_cooldown <= 0.f) {
        if(glfwGetKey(this->wnd, GLFW_KEY_F2) == GLFW_PRESS) {
            gui_visible     = !gui_visible;
            ui_key_cooldown = 0.2f;
        } else if(glfwGetKey(this->wnd, GLFW_KEY_F3) == GLFW_PRESS) {
            cam_mouse_enabled = !cam_mouse_enabled;
            ui_key_cooldown   = 0.2f;
            if(cam_mouse_enabled)
                glfwSetInputMode(this->wnd, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            else
                glfwSetInputMode(this->wnd, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if(glfwGetKey(this->wnd, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(this->wnd, GLFW_TRUE);
        }
    } else {
        ui_key_cooldown -= dt;
    }

    auto cam_system = this->w->system<camera_system>();
    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
       && cam_system->active_camera_id.has_value()) {
        static float speed = 5.0f;
        auto         cam   = cam_system->active_camera();
        auto&        trf   = this->w->system<transform_system>()->get_data_for_entity(
            cam_system->active_camera_id.value()
        );
        mat3  rot   = glm::toMat3(trf.rotation);
        auto& look  = rot[2];
        auto& right = rot[0];
        auto& up    = rot[1];
        if(glfwGetKey(wnd, GLFW_KEY_W) != GLFW_RELEASE)
            trf.translation -= speed * look * dt;
        else if(glfwGetKey(wnd, GLFW_KEY_S) != GLFW_RELEASE)
            trf.translation += speed * look * dt;
        if(glfwGetKey(wnd, GLFW_KEY_A) != GLFW_RELEASE)
            trf.translation -= speed * right * dt;
        else if(glfwGetKey(wnd, GLFW_KEY_D) != GLFW_RELEASE)
            trf.translation += speed * right * dt;
        if(glfwGetKey(wnd, GLFW_KEY_Q) != GLFW_RELEASE)
            trf.translation -= speed * up * dt;
        else if(glfwGetKey(wnd, GLFW_KEY_E) != GLFW_RELEASE)
            trf.translation += speed * up * dt;
        if(glfwGetKey(wnd, GLFW_KEY_1) != GLFW_RELEASE)
            speed += 1.f;
        else if(glfwGetKey(wnd, GLFW_KEY_2) != GLFW_RELEASE)
            speed -= 1.f;
        if(glfwGetKey(wnd, GLFW_KEY_R) != GLFW_RELEASE) trf.rotation = quat(0.f, 0.f, 0.f, 1.f);

        if(cam_mouse_enabled) {
            static double last_xpos = 0, last_ypos = 0;
            double        xpos, ypos;
            glfwGetCursorPos(wnd, &xpos, &ypos);
            vec2 sz      = vec2(size());
            vec2 np      = ((vec2(xpos - last_xpos, ypos - last_ypos) / sz)) * pi<float>() / 2.f;
            last_xpos    = xpos;
            last_ypos    = ypos;
            trf.rotation = glm::angleAxis(np.x, vec3(0.f, 1.f, 0.f)) * trf.rotation;
            trf.rotation = trf.rotation * glm::angleAxis(np.y, vec3(1.f, 0.f, 0.f));
            trf.rotation = glm::normalize(trf.rotation);
        }
    }
}

vk::CommandBuffer eggv_app::render(float t, float dt, uint32_t image_index) {
    auto& cb = command_buffers[image_index];
    cb->begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    fs.set_time(t, dt);
    r->render(cb.get(), image_index, fs);

    if(gui_visible) {
        cb->beginRenderPass(
            vk::RenderPassBeginInfo{
                gui_render_pass.get(),
                framebuffers[image_index].get(),
                vk::Rect2D(vk::Offset2D(), swapchain->extent),
                0,
                nullptr},
            vk::SubpassContents::eInline
        );

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        this->build_gui();
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb.get());
        cb->endRenderPass();
    }

    cb->end();

    return cb.get();
}

eggv_app::~eggv_app() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
}
