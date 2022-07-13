#include "renderer.h"
#include <iomanip>
#include "debug_shapes.h"
#include "imgui.h"
#include "imnodes.h"
#include "imgui_impl_vulkan.h"
#include "renderer_basic_nodes.h"

render_node::render_node(std::shared_ptr<render_node_prototype> prototype)
    : visited(false), subpass_index(-1), desc_set(nullptr),
        id(rand()), prototype(prototype), inputs(prototype->inputs.size(), { {},0 }), outputs(prototype->outputs.size(), 0), data(prototype->initialize_node_data())
{
}

render_node::render_node(renderer* r, size_t id, json data)
    : subpass_index(123456789), id(id)
{
    auto prototype_id = data.at("prototype_id").get<int>();
    auto prototypep = std::find_if(r->prototypes.begin(), r->prototypes.end(),
            [&](auto p){ return p->id() == prototype_id; });
    if(prototypep != r->prototypes.end())
        prototype = *prototypep;
    else {
        throw std::runtime_error("failed to load render node, unknown node prototype with id=" 
                + std::to_string(prototype_id));
    }
    outputs = std::vector<framebuffer_ref>(prototype->outputs.size(), 0);
    inputs = std::vector<std::pair<std::optional<std::weak_ptr<render_node>>, size_t>>(prototype->inputs.size(), { {},0 });
    this->data = prototype->deserialize_node_data(data.at("data"));
}

json render_node::serialize() const {
    std::cout << "a\n";
    std::vector<json> ser_inputs;
    for(const auto& [inp_node, inp_ix] : this->inputs) {
        if(!inp_node.has_value()) {
            ser_inputs.emplace_back(nullptr);
        } else {
            ser_inputs.push_back(json{ {"src_node", inp_node->lock()->id}, {"src_idx", inp_ix} });
        }
    }
    std::cout << "b\n";
    return {
        { "prototype_id", this->prototype->id() },
        { "inputs", ser_inputs },
        { "data", this->data != nullptr ? this->data->serialize() : json(nullptr) }
    };
}

renderer::renderer() : dev(nullptr), next_id(10), desc_pool(nullptr), num_gpu_mats(0), should_recompile(false), log_compile(true), show_shapes(true) {}

void renderer::init(device* _dev) {
    this->dev = _dev;
    global_buffers[GLOBAL_BUF_MATERIALS] = nullptr;
    global_buffers[GLOBAL_BUF_FRAME_UNIFORMS] = std::make_unique<buffer>(dev, sizeof(frame_uniforms),
            vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent,
            (void**)&mapped_frame_uniforms);

    material_desc_set_layout = dev->create_desc_set_layout({
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler,
                1, vk::ShaderStageFlagBits::eAllGraphics)
    });

    texture_sampler = dev->dev->createSamplerUnique(vk::SamplerCreateInfo {
        {}, vk::Filter::eNearest, vk::Filter::eNearest,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.f, true, 16.f
    });

    uint32 nil_tex_data = 0xffff'ffff;
    auto uplcb = dev->alloc_tmp_cmd_buffer();
    uplcb.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    create_texture2d("nil", 1, 1, vk::Format::eR8G8B8A8Unorm, 4, &nil_tex_data, uplcb);
	uplcb.end();
	dev->graphics_qu.submit({ vk::SubmitInfo(0,nullptr,nullptr,1,&uplcb) }, nullptr);

	prototypes = {
		   std::make_shared<output_render_node_prototype>(),
		   std::make_shared<simple_geom_render_node_prototype>(this, dev),
		   std::make_shared<color_preview_render_node_prototype>(),
		   std::make_shared<debug_shape_render_node_prototype>(dev)
	};
	screen_output_node = std::make_shared<render_node>(prototypes[0]);
	render_graph.push_back(screen_output_node);
	auto test = std::make_shared<render_node>(prototypes[1]);
	render_graph.push_back(test);
	screen_output_node->inputs[0] = { test, 0 };
}

void renderer::create_swapchain_dependencies(swap_chain* swpc) {
    /* std::cout << "renderer::create_swapchain_dependencies\n"; */
    this->swpc = swpc;
    full_viewport = vk::Viewport(0, 0, (float)this->swpc->extent.width, (float)this->swpc->extent.height, 0.f, 1.f);
    full_scissor = vk::Rect2D({}, this->swpc->extent);
    buffers.clear();
    this->compile_render_graph();
}

json renderer::serialize_render_graph() {
    json nodes;
    for(const auto& n : render_graph) {
        std::cout << n->id << "\n";
        nodes[std::to_string(n->id)] = n->serialize();
        std::cout << nodes << "\n\n";
    }
    return json {
        {"nodes", nodes},
        {"ui_state", ImNodes::SaveCurrentEditorStateToIniString()}
    };
}

void renderer::deserialize_render_graph(json data) {
    render_graph.clear();
    for(const auto& [id, node] : data.at("nodes").items()) {
        render_graph.push_back(std::make_shared<render_node>(this, std::atoll(id.c_str()), node));
    }
    for(const auto& [id, node_data] : data.at("nodes").items()) {
        auto idn = std::atoll(id.c_str());
        auto node = *std::find_if(render_graph.begin(), render_graph.end(), [&](auto n) { return n->id == idn; });
        auto inputs = node_data.at("inputs");
        assert(inputs.size() == node->prototype->inputs.size());
        for(size_t i = 0; i < node->prototype->inputs.size(); ++i) {
            if(inputs[i].is_null()) continue;
            auto src_id = inputs[i].at("src_node").get<size_t>();
            auto src = *std::find_if(render_graph.begin(), render_graph.end(), [&](auto n) { return n->id == src_id; });
            node->inputs[i] = { src, inputs[i].at("src_idx").get<size_t>() };
        }
        if(node->prototype == prototypes[0])
            screen_output_node = node;
    }
    auto ui_state = data.at("ui_state").get<std::string>();
    ImNodes::LoadCurrentEditorStateFromIniString(ui_state.c_str(), ui_state.size());
}

void renderer::traverse_scene_graph(scene_object* obj, frame_state* fs, const mat4& parent_T) {
    /*mat4 T = parent_T;
    for(auto&[_, t] : obj->traits) {
        t->append_transform(obj, T, fs);
    }

    if(show_shapes) {
        for(auto&[_, t] : obj->traits) {
            t->collect_viewport_shapes(obj, fs, T,
                    obj == current_scene->selected_object.get(), this->active_shapes);
        }
    }

    auto mt = obj->traits.find(TRAIT_ID_MESH);
    if(mt != obj->traits.end()) {
        auto* mmt = (mesh_trait*)mt->second.get();
        if (mmt->m != nullptr)
            active_meshes.emplace_back( mmt, T );
    }

    auto lt = obj->traits.find(TRAIT_ID_LIGHT);
    if(lt != obj->traits.end()) {
        auto* llt = (light_trait*)lt->second.get();
        active_lights.emplace_back(llt, T);
    }

    if(obj == current_scene->active_camera.get()) {
        auto* cam = (camera_trait*)obj->traits.find(TRAIT_ID_CAMERA)->second.get();
        mapped_frame_uniforms->proj = glm::perspective(cam->fov,
                (float)swpc->extent.width / (float)swpc->extent.height, 0.1f, 2000.f);
        mapped_frame_uniforms->view = inverse(T);
    }

    for (auto& [_, t] : obj->traits) {
        t->postprocess_transform(obj, T, fs);
    }

    for(const auto& c : obj->children)
        traverse_scene_graph(c.get(), fs, T);*/
}

#include "stb_image.h"
size_t renderer::load_texture(const std::string& path, vk::CommandBuffer uplcb) {
    auto existing_texture = std::find_if(texture_cache.begin(), texture_cache.end(),
            [&](const auto& p) { return std::get<0>(p) == path; });
    if(existing_texture != texture_cache.end())
        return std::distance(texture_cache.begin(), existing_texture);
    else {
        int width, height, channels;
        auto data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        channels = 4;
        vk::Format fmt = vk::Format::eUndefined;
        switch(channels) {
            case 1: fmt = vk::Format::eR8Unorm; break;
            case 2: fmt = vk::Format::eR8G8Unorm; break;
            case 3: fmt = vk::Format::eR8G8B8Unorm; break;
            case 4: fmt = vk::Format::eR8G8B8A8Unorm; break;
        };
        std::cout << "image " << path << " -> " << width << "x" << height << "/" << channels << "\n";
        return create_texture2d(path, width, height, fmt, width * height * channels, data, uplcb);
   }
}

size_t renderer::create_texture2d(const std::string& name, uint32_t width, uint32_t height, vk::Format fmt, size_t data_size, void* data, vk::CommandBuffer uplcb) {
	auto existing_texture = std::find_if(texture_cache.begin(), texture_cache.end(),
            [&](const auto& p) { return std::get<0>(p) == name; });
    if(existing_texture != texture_cache.end())
        return std::distance(texture_cache.begin(), existing_texture);
     
     auto staging_buffer = std::make_unique<buffer>(dev, data_size,
                vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible);
        memcpy(staging_buffer->map(), data, data_size);
        staging_buffer->unmap();
	vk::UniqueImageView img_view;
	auto subresource_range = vk::ImageSubresourceRange(
		vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
	);
	auto img = std::make_shared<image>(dev, vk::ImageType::e2D, vk::Extent3D(width, height, 1),
		fmt, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&img_view, vk::ImageViewType::e2D, subresource_range);
	uplcb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
			{}, {}, {}, {
				vk::ImageMemoryBarrier({}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined,
					vk::ImageLayout::eTransferDstOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					img->img, subresource_range)
			}
			);
	uplcb.copyBufferToImage(staging_buffer->buf, img->img, vk::ImageLayout::eTransferDstOptimal,
		{ vk::BufferImageCopy{0, 0, 0, vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0,0,0}, {(uint32_t)width,(uint32_t)height,1}} });
	uplcb.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
			{}, {}, {}, {
				vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					img->img, subresource_range)
			}
			);
	dev->tmp_upload_buffers.emplace_back(std::move(staging_buffer));
	size_t index = texture_cache.size();
	texture_cache.emplace_back( name, img, std::move(img_view), 0 );
	return index;
}

void renderer::update(const frame_state& fs, world* w) {
    cur_world = w;

    if(should_recompile || global_buffers[GLOBAL_BUF_MATERIALS] == nullptr || current_scene->materials_changed) {
        dev->graphics_qu.waitIdle();
        dev->present_qu.waitIdle();
    }
    
    if(global_buffers[GLOBAL_BUF_MATERIALS] == nullptr || current_scene->materials_changed) {
        if(current_scene->materials.size() != 0) {
            // recreate material buffer if necessary
            bool recreating_mat_buf = global_buffers[GLOBAL_BUF_MATERIALS] == nullptr || num_gpu_mats != current_scene->materials.size();
            if(recreating_mat_buf) {
                num_gpu_mats = (uint32_t)current_scene->materials.size();
                // we could probably move the materials ubuffer into the material desc set
                // and then use desc set offsets instead of push constants
                // I guess that wouldn't work well for lights
                global_buffers[GLOBAL_BUF_MATERIALS] = std::make_unique<buffer>(dev, sizeof(gpu_material)*num_gpu_mats,
                        vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostCoherent,
                        (void**)&mapped_materials);
                vk::DescriptorPoolSize pool_sizes[] = {
                    vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, num_gpu_mats)
                };
                auto new_pool = dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{ {},
                        (uint32)num_gpu_mats, 1, pool_sizes
                });
                material_desc_pool.swap(new_pool);
                std::vector<vk::DescriptorSetLayout> material_desc_set_layout_per_set(
                    num_gpu_mats, material_desc_set_layout.get());
                auto new_sets = dev->dev->allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                    material_desc_pool.get(),
                    (uint32)num_gpu_mats, material_desc_set_layout_per_set.data()
                });
                for (size_t i = 0; i < num_gpu_mats; ++i)
                    current_scene->materials[i]->desc_set = new_sets[i];
            }

            // copy new materials to mapping
            auto uplcb = dev->alloc_tmp_cmd_buffer();
            std::vector<vk::WriteDescriptorSet> desc_writes;
            arena<vk::DescriptorBufferInfo> buf_infos;
            arena<vk::DescriptorImageInfo> img_infos;
            auto* default_info = img_infos.alloc(vk::DescriptorImageInfo {
                    texture_sampler.get(), std::get<2>(texture_cache[0]).get(), vk::ImageLayout::eShaderReadOnlyOptimal });

            uplcb.begin(vk::CommandBufferBeginInfo { vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            for(uint32_t i = 0; i < current_scene->materials.size(); ++i) {
                mapped_materials[i] = gpu_material(current_scene->materials[i].get());
                current_scene->materials[i]->_render_index = i;
                if(current_scene->materials[i]->diffuse_texpath.has_value()) {
                    auto ix = load_texture(current_scene->materials[i]->diffuse_texpath.value(), uplcb);
                    auto* info = img_infos.alloc(vk::DescriptorImageInfo {
                            texture_sampler.get(), std::get<2>(texture_cache[ix]).get(),
                            vk::ImageLayout::eShaderReadOnlyOptimal
                            });
                    desc_writes.emplace_back(current_scene->materials[i]->desc_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, info);
                } else {
                    desc_writes.emplace_back(current_scene->materials[i]->desc_set, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, default_info);
                }
            }
            uplcb.end();
            dev->graphics_qu.submit({ vk::SubmitInfo(0,nullptr,nullptr,1,&uplcb) }, nullptr);

            if(!should_recompile && recreating_mat_buf) {
                // make sure render node descriptor sets are up to date
                for(const auto& node : subpass_order) {
                     node->prototype->update_descriptor_sets(this, node.get(), desc_writes, buf_infos, img_infos);
                }
            }
            dev->dev->updateDescriptorSets(desc_writes, {});
        }
    }
    if(should_recompile) compile_render_graph();
}

void renderer::render(vk::CommandBuffer& cb, uint32_t image_index, const frame_state& fs, world* w) {
    cur_world = w;

    render_pass_begin_info.framebuffer = framebuffers[image_index].get();
    cb.beginRenderPass(render_pass_begin_info,
            !subpass_order[0]->subpass_commands.has_value() ? vk::SubpassContents::eInline : vk::SubpassContents::eSecondaryCommandBuffers);

    for(size_t i = 0; i < subpass_order.size(); ++i) {
        for(size_t x = 0; x < subpass_order[i]->subpass_count; ++x) {
            if(subpass_order[i]->subpass_commands.has_value()) {
                cb.executeCommands({subpass_order[i]->subpass_commands.value()[x].get()});
                if(x+1 < subpass_order[i]->subpass_count) cb.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
            } else {
                subpass_order[i]->prototype->generate_command_buffer_inline(this, subpass_order[i].get(), cb, x);
                if(x+1 < subpass_order[i]->subpass_count) cb.nextSubpass(vk::SubpassContents::eInline);
            }
        }

        if(i+1 < subpass_order.size()) {
            cb.nextSubpass(!subpass_order[i+1]->subpass_commands.has_value() ? vk::SubpassContents::eInline : vk::SubpassContents::eSecondaryCommandBuffers);
        }
    }

    cb.endRenderPass();
}

renderer::~renderer() {
    subpass_order.clear();
    for(auto& n : render_graph) {
        n->desc_set.release();
        n.reset();
    }
    screen_output_node.reset();
}

mesh::mesh(device* dev, uint32_t vcount, size_t _vsize, uint32_t icount, std::function<void(void*)> write_buffer)
    : vertex_count(vcount), index_count(icount)
{
    auto vsize = _vsize*vcount,
         isize = sizeof(uint16)*icount;
    auto staging_buffer = std::make_unique<buffer>(dev, vsize+isize,
            vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible);
    auto* staging_map = staging_buffer->map();
    write_buffer(staging_map);
    staging_buffer->unmap();

    vertex_buffer = std::make_unique<buffer>(dev, vsize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
    index_buffer  = std::make_unique<buffer>(dev, isize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto upload_commands = dev->alloc_tmp_cmd_buffer();
    upload_commands.begin(vk::CommandBufferBeginInfo { vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    // could definitly collect all the staging information + copy commands and then do this all at once
    upload_commands.copyBuffer(staging_buffer->buf, vertex_buffer->buf, { vk::BufferCopy(0,0,vsize) });
    upload_commands.copyBuffer(staging_buffer->buf, index_buffer->buf,  { vk::BufferCopy(vsize,0,isize) });
    upload_commands.end();
    dev->graphics_qu.submit({ vk::SubmitInfo{0,nullptr,nullptr,1,&upload_commands} }, nullptr);
    dev->tmp_upload_buffers.emplace_back(std::move(staging_buffer));
}

void renderer::for_each_renderable(const std::function<void(entity_id, const mesh_component&, const transform&)>& f) {
    auto transforms = cur_world->system<transform_system>();

    for(auto meshi = this->begin_components(); meshi != this->end_components(); ++meshi) {
        const auto& [id, mesh] = *meshi;
        if(!transforms->has_data_for_entity(id)) continue;
        const auto& transform = transforms->get_data_for_entity(id);
        f(id, mesh, transform);
    }
}
