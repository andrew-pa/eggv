#include "app.h"
VkResult CreateDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackCreateInfoEXT* pCreateInfo, VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData)
{
	vk::DebugReportFlagsEXT flg{ (vk::DebugReportFlagBitsEXT)flags };
	if (flg & vk::DebugReportFlagBitsEXT::eDebug) std::cout << "[DEBUG]";
	else if (flg & vk::DebugReportFlagBitsEXT::eError) std::cout << "\n[\033[31mERROR\033[0m]";
	else if (flg & vk::DebugReportFlagBitsEXT::eInformation) std::cout << "[\033[32mINFO\033[0m]";
	else if (flg & vk::DebugReportFlagBitsEXT::ePerformanceWarning) std::cout << "[PERF]";
	else if (flg & vk::DebugReportFlagBitsEXT::eWarning) std::cout << "[\033[33mWARN\033[0m]";
	std::cout << ' ' /*<< layerPrefix << " | "*/ << msg << "\n";
	return VK_FALSE;
}

app::app(const std::string& title, vec2 winsize)
{
	if (!glfwInit()) throw std::runtime_error("GLFW init failed!");
	glfwSetErrorCallback([](int ec, const char* em) {
                std::cout << "GLFW error: " << em << " << error code = " << std::ios_base::hex << ec << "\n";
                //sprintf_s(s, 64, "GLFW error: %s, (error code: %08X)", em, ec);
		throw std::runtime_error(em);
        });

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_FOCUSED, false);

	wnd = glfwCreateWindow((int)floor(winsize.x), (int)floor(winsize.y), title.c_str(), nullptr, nullptr);
	if (!wnd)
	{
		glfwTerminate();
		throw std::runtime_error("GLFW window creation failed");
	}

	glfwSetWindowUserPointer(wnd, this);

	glfwSetWindowSizeCallback(wnd, [](GLFWwindow* wnd, int w, int h)
		{
			auto t = (app*)glfwGetWindowUserPointer(wnd);
			t->resize();
			t->make_resolution_dependent_resources(vec2(w, h));
		});

	glfwSetKeyCallback(wnd, [](GLFWwindow* wnd, int key, int scancode, int action, int mods)
		{
			auto t = (app*)glfwGetWindowUserPointer(wnd);
			t->key_down(key, (key_action)action, (key_mod)mods);
			for (auto& ih : t->input_handlers) {
				ih->key_handler(t, key, (input_action)action, (input_mod)mods);
			}
		});

	glfwSetCharModsCallback(wnd, [](GLFWwindow* wnd, uint cp, int mods) {
		auto t = (app*)glfwGetWindowUserPointer(wnd);
		for (auto& ih : t->input_handlers) {
			ih->char_handler(t, cp, (input_mod)mods);
		}
		});

	glfwSetCursorPosCallback(wnd, [](GLFWwindow* wnd, double x, double y) {
		auto t = (app*)glfwGetWindowUserPointer(wnd);
		for (auto& ih : t->input_handlers) {
			ih->mouse_position_handler(t, vec2(x, y));
		}
		});

	glfwSetCursorEnterCallback(wnd, [](GLFWwindow* wnd, int entered) {
		auto t = (app*)glfwGetWindowUserPointer(wnd);
		for (auto& ih : t->input_handlers) {
			ih->mouse_enterleave_handler(t, entered > 0);
		}
		});

	glfwSetMouseButtonCallback(wnd, [](GLFWwindow* wnd, int button, int action, int mods) {
		auto t = (app*)glfwGetWindowUserPointer(wnd);
		for (auto& ih : t->input_handlers) {
			ih->mouse_button_handler(t, (mouse_button)button, (input_action)action, (input_mod)mods);
		}
		});

	/* print validation layers
	auto ava_layers = vk::enumerateInstanceLayerProperties();
	for (auto ly : ava_layers) {
		cout << "avaliable instance layer " << ly.layerName << " description={" << ly.description
			<< "} version=" << ly.specVersion << ":" << ly.implementationVersion << endl;
	}*/

	vk::InstanceCreateInfo icfo;
        auto app_info = vk::ApplicationInfo{
		title.c_str(), VK_MAKE_VERSION(0,1,0),
		"eggv", VK_MAKE_VERSION(0,1,0),
		VK_API_VERSION_1_0
	};
	icfo.pApplicationInfo = &app_info;
	uint glfw_ext_cnt = 0;
	const char** glfw_ext;
	glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_ext_cnt);
	std::vector<const char*> extentions(glfw_ext, glfw_ext + glfw_ext_cnt);
#ifdef DEBUG
	extentions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
	icfo.enabledExtensionCount = extentions.size();
	icfo.ppEnabledExtensionNames = extentions.data();
	std::vector<const char*> layer_names{
#ifdef DEBUG
            "VK_LAYER_KHRONOS_validation"
#endif
	};
	icfo.enabledLayerCount = layer_names.size();
	icfo.ppEnabledLayerNames = layer_names.data();
	instance = vk::createInstance(icfo);

#ifdef DEBUG
        auto cbco = vk::DebugReportCallbackCreateInfoEXT{
            vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning,
                debugCallback
        };
        CreateDebugReportCallbackEXT((VkInstance)instance, (VkDebugReportCallbackCreateInfoEXT*)&cbco, nullptr, &report_callback);
#endif


	VkSurfaceKHR sf;
	auto res = glfwCreateWindowSurface((VkInstance)instance,
		wnd, nullptr, &sf);
	surface = vk::SurfaceKHR(sf);

	dev = std::make_unique<device>(this);
	swapchain = std::make_unique<swap_chain>(this, dev.get());
}

void app::run(bool pdfps)
{
	const float target_delta_time = 1.f / 120.f;

	uint fc = 0;
	float ft = 0.f;
	tm.reset();
	while (!glfwWindowShouldClose(wnd))
	{
		tm.update();

		auto image_index = swapchain->aquire_next();
		if (!image_index.ok() && image_index.err() == vk::Result::eErrorOutOfDateKHR
			|| image_index.err() == vk::Result::eSuboptimalKHR) {
			resize();
		}
		auto cb = render(tm.time(), tm.delta_time(), image_index.unwrap());
		vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::SubmitInfo sfo{ 1, &swapchain->image_ava_sp.get(), wait_stages, 1, &cb,
								1, &swapchain->render_fin_sp.get() };
		dev->graphics_qu.submit(sfo, nullptr);
		swapchain->present(image_index);
		post_submit(image_index);
		update(tm.time(), tm.delta_time());

		fc++;
		ft += tm.delta_time();
		glfwPollEvents();
		if (tm.delta_time() < target_delta_time) {
			auto missing_delta = target_delta_time - tm.delta_time();
			//if(missing_delta > 0.f)
				//this_thread::sleep_for(chrono::milliseconds((long)ceil(missing_delta)));
		}
	}
}

app::~app()
{
	swapchain.reset();
	dev.reset();
	vkDestroySurfaceKHR((VkInstance)instance, (VkSurfaceKHR)surface, nullptr);
#ifdef _DEBUG
	DestroyDebugReportCallbackEXT((VkInstance)instance, report_callback, nullptr);
#endif
	instance.destroy();
	glfwDestroyWindow(wnd);
	glfwTerminate();
}
