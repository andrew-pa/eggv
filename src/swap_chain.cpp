#include "swap_chain.h"
#include "app.h"

result<uint32_t, vk::Result> swap_chain::aquire_next() {
	auto v = dev->dev->acquireNextImageKHR(sch.get(),
		std::numeric_limits<uint64_t>::max(), image_ava_sp.get(), vk::Fence(nullptr));
	if (v.result == vk::Result::eSuccess || v.result == vk::Result::eSuboptimalKHR)
		return result<uint32_t, vk::Result>(v.value);
	else
		return result<uint32_t, vk::Result>(v.result);
}

void swap_chain::present(uint32_t index) {
	vk::PresentInfoKHR ifo{ 1, &render_fin_sp.get(), 1, &sch.get(), &index };
	dev->present_qu.presentKHR(ifo);
	dev->present_qu.waitIdle(); //it seems this line isn't stricly necessary?
}

void swap_chain::recreate(app* app) {
	for (auto& iv : image_views) iv.reset();
	sch.reset();
	dev->dev->waitIdle();
	create(app);
}

std::vector<vk::UniqueFramebuffer> swap_chain::create_framebuffers(vk::RenderPass rnp, std::function<void(size_t, std::vector<vk::ImageView>&)> additional_image_views, bool include_depth) {
	std::vector<vk::UniqueFramebuffer> framebuffers(image_views.size());
	for (size_t i = 0; i < image_views.size(); ++i) {
		std::vector<vk::ImageView> att = {
			image_views[i].get(),
		};
		additional_image_views(i, att);
		framebuffers[i] = dev->dev->createFramebufferUnique(vk::FramebufferCreateInfo{
			vk::FramebufferCreateFlags(),
			rnp, (uint32_t)att.size(), att.data(),
			extent.width, extent.height, 1 });
	}
	return framebuffers;
}

swap_chain::swap_chain(app* app, device* dev) : dev(dev) {
	create(app);
	vk::SemaphoreCreateInfo spcfo;
	image_ava_sp = dev->dev->createSemaphoreUnique(spcfo);
	render_fin_sp = dev->dev->createSemaphoreUnique(spcfo);
}

swap_chain::~swap_chain() {}

void swap_chain::create(app* app) {
	auto surf_caps = dev->pdevice.getSurfaceCapabilitiesKHR(app->surface);
        /*std::cout << "Surface capabilities:\n"
            << "\tMin images  = " << surf_caps.minImageCount << "\n"
            << "\tMax images  = " << surf_caps.maxImageCount << "\n"
            << "\tComp alpha  = " << vk::to_string(surf_caps.supportedCompositeAlpha) << "\n"
            << "\tUsage flags = " << vk::to_string(surf_caps.supportedUsageFlags) << "\n";*/
	uint32_t image_count = surf_caps.minImageCount + 1;
	if (surf_caps.maxImageCount > 0 && image_count > surf_caps.maxImageCount)
		image_count = surf_caps.maxImageCount;
	vk::SwapchainCreateInfoKHR cfo;
	cfo.surface = app->surface;
	cfo.minImageCount = image_count;
	format = cfo.imageFormat = vk::Format::eB8G8R8A8Unorm; //basically assume device is not garbage
	cfo.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
	auto winsize = app->size();
	extent = cfo.imageExtent = vk::Extent2D(winsize.x, winsize.y);
	cfo.imageArrayLayers = 1;
	cfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	if (dev->qu_fam.graphics != dev->qu_fam.present) {
		cfo.imageSharingMode = vk::SharingMode::eConcurrent;
		cfo.queueFamilyIndexCount = 2;
		uint32_t qfi[] = { (uint32_t)dev->qu_fam.graphics, (uint32_t)dev->qu_fam.present };
		cfo.pQueueFamilyIndices = qfi;
	}
	else {
		cfo.imageSharingMode = vk::SharingMode::eExclusive;
	}
	cfo.preTransform = surf_caps.currentTransform;
	cfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	cfo.presentMode = vk::PresentModeKHR::eFifo;
	cfo.clipped = true;

	sch = dev->dev->createSwapchainKHRUnique(cfo);

	images = dev->dev->getSwapchainImagesKHR(sch.get());
        vk::ImageViewCreateInfo ivcfo;
	ivcfo.viewType = vk::ImageViewType::e2D;
	ivcfo.format = format;
	ivcfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	ivcfo.subresourceRange.baseMipLevel = 0;
	ivcfo.subresourceRange.levelCount = 1;
	ivcfo.subresourceRange.baseArrayLayer = 0;
	ivcfo.subresourceRange.layerCount = 1;
	image_views.clear();
	for (auto img : images) {
		ivcfo.image = img;
		image_views.push_back(dev->dev->createImageViewUnique(ivcfo));
	}
}
