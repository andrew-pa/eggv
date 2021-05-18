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
                if(include_depth) att.push_back(depth_view.get());
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
	depth_buf = std::make_unique<image>(dev, vk::ImageType::e2D, vk::Extent3D{ extent.width, extent.height, 1 }, vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
	/*	auto tbf = std::move(dev->alloc_cmd_buffers()[0]);
		tbf->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		auto subresrange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0,1,0,1 };
		tbf->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, {
			vk::ImageMemoryBarrier{vk::AccessFlags(), vk::AccessFlags(),
				vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, depth_buf->operator vk::Image(), subresrange}
		});
		tbf->end();
		dev->graphics_qu.submit({ vk::SubmitInfo{0,nullptr,nullptr,1,&tbf.get()} }, nullptr);*/
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

	ivcfo.format = vk::Format::eD32Sfloat;
	ivcfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
	ivcfo.image = depth_buf->operator vk::Image();
	depth_view = dev->dev->createImageViewUnique(ivcfo);
	dev->graphics_qu.waitIdle();
}
