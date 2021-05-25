#pragma once
#include "cmmn.h"
#include "device.h"

struct swap_chain {
	device* dev;
	vk::UniqueSwapchainKHR sch;
	std::vector<vk::Image> images;
	std::vector<vk::UniqueImageView> image_views;
	vk::Extent2D extent;
	vk::Format format;
	vk::UniqueSemaphore image_ava_sp, render_fin_sp;

	/*std::unique_ptr<image> depth_buf;
	vk::UniqueImageView depth_view;*/

	result<uint32_t, vk::Result> aquire_next();
	void present(uint32_t index);
	void recreate(app* app);

	std::vector<vk::UniqueFramebuffer> create_framebuffers(vk::RenderPass rnp, std::function<void(size_t, std::vector<vk::ImageView>&)> additional_image_views = [](auto, auto) {}, bool include_depth = true);

	swap_chain(app* app, device* dev);
	~swap_chain();
private:
	void create(app* app);
};
