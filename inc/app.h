#pragma once
#include "cmmn.h"
#include "device.h"
#include "swap_chain.h"
#define DEBUG

enum class input_action
{
	press = GLFW_PRESS,
	release = GLFW_RELEASE,
	repeat = GLFW_REPEAT,
};
typedef input_action key_action;

enum class input_mod
{
	alt = GLFW_MOD_ALT,
	ctrl = GLFW_MOD_CONTROL,
	shift = GLFW_MOD_SHIFT,
	super = GLFW_MOD_SUPER,
};
typedef input_mod key_mod;

enum class mouse_button {
	left = GLFW_MOUSE_BUTTON_LEFT,
	right = GLFW_MOUSE_BUTTON_RIGHT,
	middle = GLFW_MOUSE_BUTTON_MIDDLE,
};

class timer
{
	double last_time;
	double curr_time;
	double _ctime;
	double _deltat;
public:
	timer()
	{
		last_time = curr_time = glfwGetTime();
		_deltat = 0;
		_ctime = 0;
	}

	void reset()
	{
		last_time = curr_time = glfwGetTime();
		_ctime = 0;
		_deltat = 0;
	}

	void update()
	{
		curr_time = glfwGetTime();

		_deltat = curr_time - last_time;
		_ctime += _deltat;

		last_time = curr_time;
	}

	inline float time() const { return (float)_ctime; }
	inline float delta_time() const { return (float)_deltat; }
};

class app;

struct input_handler {
	virtual void key_handler(app* _app, uint keycode, input_action action, input_mod mods) {}
	virtual void char_handler(app* _app, uint codepoint, input_mod mods) {}
	virtual void mouse_position_handler(app* _app, vec2 p) {}
	virtual void mouse_enterleave_handler(app* _app, bool entered) {}
	virtual void mouse_button_handler(app* _app, mouse_button button, input_action action, input_mod mods) {}
};

class app
{
	bool _did_resize;
public:
	timer tm;
	GLFWwindow* wnd;
	vk::Instance instance;
	vk::SurfaceKHR surface;
#ifdef DEBUG
	VkDebugReportCallbackEXT report_callback;
#endif
	std::unique_ptr<device> dev;
	std::unique_ptr<swap_chain> swapchain;

	app(const std::string& title, vec2 winsize);
	virtual ~app();

	void run(bool print_debug_fps = true);

	virtual vk::CommandBuffer render(float t, float dt, uint32_t image_index) = 0;
	virtual void update(float t, float dt) = 0;
	virtual void make_resolution_dependent_resources(vec2 size) {}
	virtual void resize() {
		swapchain->recreate(this);
	}
	virtual void key_down(uint keycode, key_action action, key_mod mods) {}
	virtual void post_submit(uint32_t image_index) {}

	inline ivec2 size() {
		ivec2 wh;
		glfwGetWindowSize(wnd, &wh.x, &wh.y);
		return wh;
	}

	std::vector<input_handler*> input_handlers; //~app does not delete these!
};
