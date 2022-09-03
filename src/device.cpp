#include "device.h"
#include "app.h"
#include <set>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

device::device(app* app) {
    auto devices = app->instance.enumeratePhysicalDevices();
    // just choose the first physical device for now
    pdevice            = devices[0];
    auto pdevice_props = pdevice.getProperties();
    std::cout << "Physical Device: " << pdevice_props.deviceName << std::endl;
    qu_fam = queue_families(pdevice, app);
    assert(qu_fam.complete());

    vk::DeviceCreateInfo                   dcfo;
    std::vector<vk::DeviceQueueCreateInfo> qu_cfo;
    auto  unique_qufam = std::set<int>{qu_fam.graphics, qu_fam.present};
    float fp           = 1.f;
    for(int qf : unique_qufam) {
        qu_cfo.push_back(vk::DeviceQueueCreateInfo{
            vk::DeviceQueueCreateFlags(), (uint32_t)qf, 1, &fp});
    }
    dcfo.queueCreateInfoCount = (uint32_t)qu_cfo.size();
    dcfo.pQueueCreateInfos    = qu_cfo.data();

    vk::PhysicalDeviceFeatures devfeat;
    devfeat.samplerAnisotropy                      = VK_TRUE;
    devfeat.fillModeNonSolid                       = VK_TRUE;
    devfeat.tessellationShader                     = VK_TRUE;
    devfeat.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
    devfeat.depthBiasClamp                         = VK_TRUE;
    dcfo.pEnabledFeatures                          = &devfeat;
    std::vector<const char*> layer_names{
#ifdef _DEBUG
        "VK_LAYER_LUNARG_standard_validation",
#endif
    };
    dcfo.enabledLayerCount       = (uint32_t)layer_names.size();
    dcfo.ppEnabledLayerNames     = layer_names.data();
    std::vector<const char*> ext = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    dcfo.enabledExtensionCount   = (uint32_t)ext.size();
    dcfo.ppEnabledExtensionNames = ext.data();
    try {
        dev = pdevice.createDeviceUnique(dcfo);
    } catch(vk::FeatureNotPresentError e) {
        std::cerr << "physical device feature not present: " << e.what() << "[" << e.code()
                  << "]\n";
        exit(1);
    }

    std::cout << "Vulkan build header version: " << VK_HEADER_VERSION << "\n";

    graphics_qu = dev->getQueue(qu_fam.graphics, 0);
    present_qu  = dev->getQueue(qu_fam.present, 0);

    cmdpool = dev->createCommandPoolUnique(vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer, (uint32_t)qu_fam.graphics});

    VmaAllocatorCreateInfo cfo = {};
    cfo.instance               = (VkInstance)app->instance;
    cfo.physicalDevice         = (VkPhysicalDevice)pdevice;
    cfo.device                 = (VkDevice)dev.get();
    vmaCreateAllocator(&cfo, &allocator);
}

std::vector<vk::UniqueCommandBuffer> device::alloc_cmd_buffers(
    uint32_t num = 1, vk::CommandBufferLevel lvl
) {
    vk::CommandBufferAllocateInfo afo;
    afo.level              = lvl;
    afo.commandPool        = cmdpool.get();
    afo.commandBufferCount = num;
    return dev->allocateCommandBuffersUnique(afo);
}

vk::CommandBuffer device::alloc_tmp_cmd_buffer(vk::CommandBufferLevel lvl) {
    auto cb  = std::move(this->alloc_cmd_buffers(1, lvl)[0]);
    auto ccb = cb.get();
    tmp_cmd_buffers.emplace_back(std::move(cb));
    return ccb;
}

void device::clear_tmps() {
    if(!tmp_cmd_buffers.empty()) tmp_cmd_buffers.clear();
    if(!tmp_upload_buffers.empty()) tmp_upload_buffers.clear();
}

vk::UniqueDescriptorSetLayout device::create_desc_set_layout(
    std::vector<vk::DescriptorSetLayoutBinding> bindings
) {
    return dev->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
        vk::DescriptorSetLayoutCreateFlags(), (uint32_t)bindings.size(), bindings.data()});
}

vk::ShaderModule device::load_shader(const std::filesystem::path& path) {
    auto f = shader_module_cache.find(path);
    if(f != shader_module_cache.end()) return f->second.get();

    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if(!file) throw std::runtime_error(std::string("failed to load shader at: ") + path.c_str());

    std::vector<char> buffer((size_t)file.tellg());
    file.seekg(0);
    file.read(buffer.data(), buffer.size());
    file.close();

    vk::ShaderModuleCreateInfo cfo;
    cfo.codeSize = buffer.size();
    cfo.pCode    = (uint32_t*)buffer.data();

    shader_module_cache[path] = this->dev->createShaderModuleUnique(cfo);
    return shader_module_cache[path].get();
}

vk::UniquePipeline device::create_graphics_pipeline(const vk::GraphicsPipelineCreateInfo& cfo) {
    auto res = dev->createGraphicsPipelineUnique(nullptr, cfo);
    if(res.result != vk::Result::eSuccess) {
        throw res;
    }
    return std::move(res.value);
}

device::~device() {
    graphics_qu.waitIdle();
    present_qu.waitIdle();
    for(auto& s : shader_module_cache)
        s.second.reset();
    vmaDestroyAllocator(allocator);
    cmdpool.reset();
    dev.reset();
}

device::queue_families::queue_families(vk::PhysicalDevice pd, app* app) {
    auto qufams = pd.getQueueFamilyProperties();
    for(uint32_t i = 0; i < qufams.size() && !complete(); ++i) {
        if(qufams[i].queueCount <= 0) continue;
        if(qufams[i].queueFlags & vk::QueueFlagBits::eGraphics) graphics = i;
        if(pd.getSurfaceSupportKHR(i, app->surface)) present = i;
    }
}

static uint64 counter = 1;

buffer::buffer(
    device*                 dev,
    vk::DeviceSize          size,
    vk::BufferUsageFlags    bufuse,
    vk::MemoryPropertyFlags memuse,
    void**                  persistent_map
)
    : dev(dev)
{
    VmaAllocationCreateInfo mreq = {};
    mreq.flags                   = persistent_map ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;
    mreq.requiredFlags           = (VkMemoryPropertyFlags)memuse;
    VmaAllocationInfo alli;
    auto              bco = vk::BufferCreateInfo{vk::BufferCreateFlags(), size, bufuse};

    auto              res
        = vmaCreateBuffer(dev->allocator, (VkBufferCreateInfo*)&bco, &mreq, &buf, &alloc, &alli);
    if(persistent_map != nullptr) *persistent_map = alli.pMappedData;
    if(res != VK_SUCCESS) {
        std::cerr << "failed to create buffer " << res << "\n";
        assert(res == VK_SUCCESS);
    }
    id = counter++;
#ifdef BUFFER_ALLOC_RELEASE_DEBUG
    std::cout << "buffer #" << id << "!" << size << "\n";
#endif
}

void* buffer::map() {
    void* data;
    auto  res = vmaMapMemory(dev->allocator, alloc, &data);
    assert(res == VK_SUCCESS);
    return data;
}

void buffer::unmap() { vmaUnmapMemory(dev->allocator, alloc); }

/*buffer& buffer::operator=(const buffer&& b) {
    if (buf != VK_NULL_HANDLE) {
        vmaDestroyBuffer(dev->allocator, buf, alloc);
    }
    dev = b.dev;
    buf = b.buf;
    alloc = b.alloc;
    return *this;
}*/

buffer::~buffer() {
    if(buf == VK_NULL_HANDLE) return;
    vmaDestroyBuffer(dev->allocator, buf, alloc);
#ifdef BUFFER_ALLOC_RELEASE_DEBUG
    std::cout << "~buffer #" << this->id << "\n";
#endif
    buf = VK_NULL_HANDLE;
}

image::image(
    device*                             dev,
    vk::ImageCreateFlags                flg,
    vk::ImageType                       type,
    vk::Extent3D                        size,
    vk::Format                          fmt,
    vk::ImageTiling                     til,
    vk::ImageUsageFlags                 use,
    vk::MemoryPropertyFlags             memuse,
    uint32_t                            mip_count,
    uint32_t                            array_layers,
    std::optional<vk::UniqueImageView*> iv,
    vk::ImageViewType                   iv_type,
    vk::ImageSubresourceRange           iv_sr
)
    : dev(dev) {
    VmaAllocationCreateInfo mreq = {};
    mreq.requiredFlags           = (VkMemoryPropertyFlags)memuse;
    VmaAllocationInfo alli;
    auto              ico = vk::ImageCreateInfo{
        flg, type, fmt, size, mip_count, array_layers, vk::SampleCountFlagBits::e1, til, use};
    info     = ico;
    auto res = vmaCreateImage(dev->allocator, (VkImageCreateInfo*)&ico, &mreq, &img, &alloc, &alli);
    assert(res == VK_SUCCESS);
    if(iv) {
        **iv = dev->dev->createImageViewUnique(vk::ImageViewCreateInfo(
            vk::ImageViewCreateFlags(), vk::Image(img), iv_type, fmt, vk::ComponentMapping(), iv_sr
        ));
    }
}

void image::generate_mipmaps(
    uint32_t w, uint32_t h, vk::CommandBuffer cb, uint32_t layer_count, vk::ImageLayout final_layout
) {
    auto subresrange
        = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, layer_count};
    // transition the biggest mipmap (the loaded src image) so that it can be copied from
    cb.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {
    },
        {},
        {vk::ImageMemoryBarrier{
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferSrcOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            vk::Image(img),
            subresrange}}
    );

    const uint32_t count = calculate_mipmap_count(w, h);

    // transition all the other mip levels to write mode so we can blit to them
    subresrange.baseMipLevel = 1;
    subresrange.levelCount   = (uint32_t)(count - 1);
    cb.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eTransfer,
        vk::DependencyFlags(),
        {
    },
        {},
        {vk::ImageMemoryBarrier{
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            vk::Image(img),
            subresrange}}
    );
    subresrange.levelCount = 1;

    vk::ImageBlit region;
    region.dstSubresource.aspectMask = region.srcSubresource.aspectMask
        = vk::ImageAspectFlagBits::eColor;
    region.dstSubresource.layerCount = region.srcSubresource.layerCount = layer_count;
    for(uint32_t i = 1; i < count; ++i) {
        region.srcSubresource.mipLevel = i - 1;
        region.dstSubresource.mipLevel = i;
        region.srcOffsets[1].x         = glm::max(w >> (i - 1), (uint32_t)1);
        region.srcOffsets[1].y         = glm::max(h >> (i - 1), (uint32_t)1);
        region.srcOffsets[1].z         = 1;
        region.dstOffsets[1].x         = glm::max(w >> i, (uint32_t)1);
        region.dstOffsets[1].y         = glm::max(h >> i, (uint32_t)1);
        region.dstOffsets[1].z         = 1;

        // copy the last mip level to the next mip level while filtering
        cb.blitImage(
            vk::Image(img),
            vk::ImageLayout::eTransferSrcOptimal,
            vk::Image(img),
            vk::ImageLayout::eTransferDstOptimal,
            {region},
            vk::Filter::eLinear
        );

        // transition the last mip level to the final layout as we don't need it anymore
        subresrange.baseMipLevel = i - 1;
        cb.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlags(),
            {
        },
            {},
            {vk::ImageMemoryBarrier{
                vk::AccessFlagBits::eTransferRead,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eTransferSrcOptimal,
                final_layout,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                vk::Image(img),
                subresrange}}
        );

        subresrange.baseMipLevel = i;
        if(i + 1 < count) {
            // transition this mip level to read mode from write mode
            cb.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                vk::DependencyFlags(),
                {
            },
                {},
                {vk::ImageMemoryBarrier{
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead,
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eTransferSrcOptimal,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    vk::Image(img),
                    subresrange}}
            );
        } else {
// transtion this mip level to the final layout as it's the last mip level
            cb.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::DependencyFlags(),
                {
            },
                {},
                {vk::ImageMemoryBarrier{
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::ImageLayout::eTransferDstOptimal,
                    final_layout,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    vk::Image(img),
                    subresrange}}
            );
        }
    }
}

image::~image() { vmaDestroyImage(dev->allocator, img, alloc); }
