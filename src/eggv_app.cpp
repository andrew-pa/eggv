#include "eggv_app.h"
#include "glm/gtx/quaternion.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imnodes.h"
#include "vk_mem_alloc.h"
//#include "ImGuiFileDialog.h"
#include <uuid.h>
#include "mesh_gen.h"
#include "deferred_nodes.h"
#include "geometry_set.h"

void generate_cube(float width, float height, float depth, std::function<void(vec3, vec3, vec3, vec2)> vertex, std::function<void(size_t)> index) {
	float w2 = 0.5f * width;
	float h2 = 0.5f * height;
	float d2 = 0.5f * depth;

	// Fill in the front face vertex data.
	vertex({ -w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
	vertex({ -w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
	vertex({ +w2, +h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
	vertex({ +w2, -h2, -d2 }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
	// F{ill in the ba}c{k face vertex data}.{}{}1
	vertex({ -w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
	vertex({ +w2, -h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
	vertex({ +w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
	vertex({ -w2, +h2, +d2 }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
	// F{ill in the to}p{ face vertex data.}{}{}1
	vertex({ -w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 1.0f });
	vertex({ -w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 0.0f });
	vertex({ +w2, +h2, +d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, }, { 1.0f, 0.0f });
	vertex({ +w2, +h2, -d2 }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, }, { 1.0f, 1.0f });
	// F{ill in the bo}t{tom face vertex da}t{a.}{}1
	vertex({ -w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f });
	vertex({ +w2, -h2, -d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f });
	vertex({ +w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f });
	vertex({ -w2, -h2, +d2 }, { 0.0f, -1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f });
	// F{ill in the le}f{t face vertex data}.{}{}1
	vertex({ -w2, -h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f });
	vertex({ -w2, +h2, +d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f });
	vertex({ -w2, +h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f });
	vertex({ -w2, -h2, -d2 }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f });
	// F{ill in the ri}g{ht face vertex dat}a{.}{}1
	vertex({ +w2, -h2, -d2 }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f });
	vertex({ +w2, +h2, -d2 }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f });
	vertex({ +w2, +h2, +d2 }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f });
	vertex({ +w2, -h2, +d2 }, { 1.0f, 0.0f, 0.0f, }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f });

	// Fill in the front face index data
	index(0); index(1); index(2);
	index(0); index(2); index(3);

	// Fill in the back face index data
	index(4); index(5); index(6);
	index(4); index(6); index(7);

	// Fill in the top face index data
	index(8); index(9); index(10);
	index(8); index(10); index(11);

	// Fill in the bottom face index data
	index(12); index(13); index(14);
	index(12); index(14); index(15);

	// Fill in the left face index data
	index(16); index(17); index(18);
	index(16); index(18); index(19);

	// Fill in the right face index data
	index(20); index(21); index(22);
	index(20); index(22); index(23);
}

std::vector<std::shared_ptr<trait_factory>> collect_factories() {
    return std::vector<std::shared_ptr<trait_factory>>{
        std::make_shared<transform_trait_factory>(),
            std::make_shared<mesh_trait_factory>(),
            std::make_shared<light_trait_factory>(),
            std::make_shared<camera_trait_factory>()
    };
}

std::shared_ptr<scene> create_scene(device* dev) {
    auto s = std::make_shared<scene>(collect_factories(), std::make_shared<scene_object>("Root"));
    auto gs = std::make_shared<geometry_set>(dev, "sponza.geo");
    s->geometry_sets.push_back(gs);
    //
    // /*auto test_mesh = std::make_shared<mesh>(mesh_gen::generate_plane(dev, 32, 32));
	auto geo = std::make_shared<scene_object>("geo");
    {
        auto tfm = transform_trait_factory::create_info(vec3(), quat(1.f, 0.f, 0.f, 0.f), vec3(0.05f));
        s->trait_factories[0]->add_to(geo.get(), &tfm);
    }
    auto mat = std::make_shared<material>("default material", vec3(0.5f));
    s->materials.push_back(mat);
    for(size_t i = 0; i < gs->num_meshes(); ++i) {
        auto obj = std::make_shared<scene_object>(gs->mesh_name(i));
        auto tfm = transform_trait_factory::create_info();
        s->trait_factories[0]->add_to(obj.get(), &tfm);
        auto cfo = mesh_create_info();
        cfo.geo_src = gs;
        cfo.mesh_index = i;
        cfo.mat = mat;
        s->trait_factories[1]->add_to(obj.get(), &cfo);
        geo->children.push_back(obj);
    }
    s->root->children.push_back(geo);

    {
        auto obj = std::make_shared<scene_object>("camera");
        auto tfm = transform_trait_factory::create_info(
                vec3(0.f,-5.f,-15.f),
                quatLookAt(normalize(vec3(0.f,5.f,15.f)), vec3(0.f, 1.f, 0.f)));
        s->trait_factories[0]->add_to(obj.get(), &tfm);
        s->trait_factories[3]->add_to(obj.get(), nullptr);
        s->root->children.push_back(obj);
        s->active_camera = obj;
    }
    {
        auto obj = std::make_shared<scene_object>("camera 2");
        auto tfm = transform_trait_factory::create_info(
                vec3(-15.f,-5.f,0.f),
                quatLookAt(normalize(vec3(15.f,5.f,0.f)), vec3(0.f, 1.f, 0.f)));
        s->trait_factories[0]->add_to(obj.get(), &tfm);
        s->trait_factories[3]->add_to(obj.get(), nullptr);
        s->root->children.push_back(obj);
    }

    //
    // auto test_mesh2 = std::make_shared<mesh>(mesh_gen::generate_trefoil_knot(dev, 64, 256, 2.0f));
    // {
    //     auto obj = std::make_shared<scene_object>("trefoil knot");
    //     auto tfm = transform_trait_factory::create_info(vec3(0.f,-.65f,0.f));
    //     s->trait_factories[0]->add_to(obj.get(), &tfm);
    //     auto cfo = mesh_trait_factory::create_info(); cfo.m = test_mesh2;
    //     s->trait_factories[1]->add_to(obj.get(), &cfo);
    //     s->root->children.push_back(obj);
    // }
    //
    // auto test_mesh3 = std::make_shared<mesh>(mesh_gen::generate_sphere(dev, 64, 64));
    // for(int i = -1; i <= 1; ++i) {
    // for(int j = -1; j <= 1; ++j) {
    //     if(i == 0 && j == 0) continue;
    //     auto obj = std::make_shared<scene_object>("sphere");
    //     auto tfm = transform_trait_factory::create_info(vec3(2.0f * i,0.6f,2.0f*j),quat(),vec3(0.6f));
    //     s->trait_factories[0]->add_to(obj.get(), &tfm);
    //     auto cfo = mesh_trait_factory::create_info(); cfo.m = test_mesh3;
    //     s->trait_factories[1]->add_to(obj.get(), &cfo);
    //     s->root->children.push_back(obj);
    // }
    // }
    //
    {
        auto obj = std::make_shared<scene_object>("light");
        auto lco = light_trait_factory::create_info(light_type::directional, vec3(0.f, 1.f, 0.f), vec3(4.f,4.f,3.5f));
        s->trait_factories[2]->add_to(obj.get(), &lco);
        s->root->children.push_back(obj);
    }

    {
        auto obj = std::make_shared<scene_object>("light1");
        auto lco = light_trait_factory::create_info(light_type::directional, vec3(0.f, -.5f, 1.f), vec3(1.f,1.f,0.2f));
        s->trait_factories[2]->add_to(obj.get(), &lco);
        s->root->children.push_back(obj);
    }

    {
        auto obj = std::make_shared<scene_object>("plight");
        auto tfm = transform_trait_factory::create_info(vec3(2.0f,-0.7f,-1.5f),quat(),vec3(1.0f));
        s->trait_factories[0]->add_to(obj.get(), &tfm);
        auto lco = light_trait_factory::create_info(light_type::point, vec3(0.3f, 0.f, 0.f), vec3(.5f,.4f,.0f)*100.f);
        s->trait_factories[2]->add_to(obj.get(), &lco);
        s->root->children.push_back(obj);
    }
    {
        auto obj = std::make_shared<scene_object>("plight");
        auto tfm = transform_trait_factory::create_info(vec3(-2.0f,-0.7f,-1.5f),quat(),vec3(1.0f));
        s->trait_factories[0]->add_to(obj.get(), &tfm);
        auto lco = light_trait_factory::create_info(light_type::point, vec3(1.5f, 0.f, 0.f), vec3(.45f,.0f,.35f)*100.f);
        s->trait_factories[2]->add_to(obj.get(), &lco);
        s->root->children.push_back(obj);
    }

    // for(size_t i = 0; i < 50; ++i) {
    //     auto obj = std::make_shared<scene_object>("plight");
    //     auto tfm = transform_trait_factory::create_info(vec3(rand() % 20 - 10,rand() % 20 - 20,rand()%20 - 10),quat(),vec3(1.0f));
    //     s->trait_factories[0]->add_to(obj.get(), &tfm);
    //     auto lco = light_trait_factory::create_info(light_type::point, vec3(0.2f, 0.f, 0.f), vec3(4.5f,4.0f,3.5f));
    //     s->trait_factories[2]->add_to(obj.get(), &lco);
    //     s->root->children.push_back(obj);
    //}



    //
    //
    //
     return s;
}

#pragma region Initialization
vec2 get_resolution_from_args(const std::vector<std::string>& cargs) {
    for(int i = 0; i < cargs.size(); ++i) {
        if(cargs[i] == "-r") {
            i++;
            float w = std::atof(cargs[i++].c_str());
            float h = std::atof(cargs[i++].c_str());
            return vec2(w,h);
        }
    }
    return vec2(1920, 1080);
}

eggv_app::eggv_app(const std::vector<std::string>& cargs)
    : app("erg", get_resolution_from_args(cargs)),
        current_scene(nullptr), r(), gui_visible(true), ui_key_cooldown(0.f),
        cam_mouse_enabled(false),
      gui_open_windows({
        {"Renderer", true},
        {"Scene", true},
        {"Geometry Sets", true},
        {"Materials", true},
        {"Selected Object", true},
        {"ImGui Demo", false},
        {"ImGui Metrics", false},
      })
{
    r.init(dev.get());
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1) // for ImGUI
    };

    desc_pool = dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, (uint32_t)pool_sizes.size(), pool_sizes.data()
    });

    this->init_swapchain_depd();
    this->init_gui();

    r.prototypes.emplace_back(std::make_shared<gbuffer_geom_render_node_prototype>(dev.get(), &r));
    r.prototypes.emplace_back(std::make_shared<directional_light_render_node_prototype>(dev.get()));
    r.prototypes.emplace_back(std::make_shared<point_light_render_node_prototype>(dev.get()));


    for(int i = 1; i < cargs.size(); ++i) {
        if(cargs[i] == "-p") {
            i++;
            if(i >= cargs.size()) throw;
            load_plugin(cargs[i++]);
        } else if(cargs[i] == "-rg") {
            i++;
            if(i >= cargs.size()) throw;
            std::ifstream input(cargs[i]);
            json data; input >> data;
            r.deserialize_render_graph(data);
            r.compile_render_graph();
        } else if(cargs[i] == "-s") {
            i++;
            if(i >= cargs.size()) throw;
            std::ifstream input(cargs[i]);
            json data; input >> data;
            current_scene = std::make_shared<scene>(dev.get(), collect_factories(), data);
        }
    }

    if(current_scene == nullptr) current_scene = create_scene(dev.get());
    r.current_scene = current_scene;

    auto upload_cb = std::move(dev->alloc_cmd_buffers(1)[0]);
    upload_cb->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    std::vector<std::unique_ptr<buffer>> upload_buffers;
    //rviewport.init_upload(upload_cb.get(), upload_buffers);
    ImGui_ImplVulkan_CreateFontsTexture(upload_cb.get());

    upload_cb->end();
    dev->graphics_qu.submit({ vk::SubmitInfo{0, nullptr, nullptr, 1, &upload_cb.get()} }, nullptr);

    dev->graphics_qu.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    dev->clear_tmps();

    //frame_state fs(0.f, 0.f, current_scene, &gui_open_windows);
    //r.update(&fs);
}

void eggv_app::load_plugin(const std::string& path) {
    /*char* error = dlerror();
    auto hndl = dlopen(path.c_str(), RTLD_LAZY);
    if((error = dlerror()) != nullptr) {
        std::cout << "failed to load plugin: " << error << "\n";
        return;
    }
    auto load = (void(*)(device*, std::vector<std::shared_ptr<render_node_prototype>>*, std::vector<std::shared_ptr<trait_factory>>*))dlsym(hndl, "eggv_plugin_load");
    if((error = dlerror()) != nullptr) {
        std::cout << "failed to load plugin at " << path << " error:" << error << "\n";
        return;
    }
    load(dev.get(), &r.prototypes, &current_scene->trait_factories);
    plugins.push_back({path, hndl});*/
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

    /* rviewport.init_render_pass(attachments, refs, subpasses, depds); */

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

#pragma region GUI
/*void InputTextResizable(const char* label, std::string* str) {
	ImGui::InputText(label, (char*)str->c_str(), str->size(), ImGuiInputTextFlags_CallbackResize,
		(ImGuiTextEditCallback)([](ImGuiInputTextCallbackData* data) {
			if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
				std::string* str = (std::string*)data->UserData;
				str->resize(data->BufSize+1);
				data->Buf = (char*)str->c_str();
			}
			return 0;
		}
	), (void*)str);
}*/

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
}
#pragma endregion

#pragma region Render Loop
void eggv_app::update(float t, float dt) {
    frame_state fs(t, dt, &r, current_scene, &gui_open_windows);
    current_scene->update(&fs, this);
    r.update(&fs);

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
            exit(0);
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
            double xpos, ypos;
            glfwGetCursorPos(wnd, &xpos, &ypos);
            vec2 sz = vec2(size());
            vec2 np = ((vec2(xpos, ypos) / sz)*2.f - 1.f) * pi<float>()/2.f;
            trf->rotation = glm::normalize(
                    glm::angleAxis(clamp(np.y, -pi<float>()/3.f, pi<float>()/3.f), right)
                    *glm::angleAxis(-np.x, vec3(0.f, 1.f, 0.f)));
        }
    }
}

vk::CommandBuffer eggv_app::render(float t, float dt, uint32_t image_index) {
    auto& cb = command_buffers[image_index];
    cb->begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    frame_state fs(t, dt, &r, current_scene, &gui_open_windows);
    r.render(cb.get(), image_index, &fs);

    if(gui_visible) {
        cb->beginRenderPass(vk::RenderPassBeginInfo{
                gui_render_pass.get(), framebuffers[image_index].get(),
                vk::Rect2D(vk::Offset2D(), swapchain->extent), 0, nullptr
                }, vk::SubpassContents::eInline);

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
#pragma endregion

eggv_app::~eggv_app() {
        for(const auto&[_, hndl] : plugins) {
            /* auto unload = (void(*)(void))dlsym(hndl, "eggv_plugin_unload"); */
            /* unload(); */
            //dlclose(hndl);
        }
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImNodes::DestroyContext();
	ImGui::DestroyContext();
}
