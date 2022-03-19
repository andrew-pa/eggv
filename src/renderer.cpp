#include "renderer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imnodes.h"
#include <iomanip>
#include "debug_shapes.h"

// is setting the perserveAttachment counts on the subpasses necessary?

inline vk::ImageUsageFlags usage_for_type(framebuffer_type ty) {
    switch(ty) {
        case framebuffer_type::color:
            return vk::ImageUsageFlagBits::eColorAttachment;
        case framebuffer_type::depth:
        case framebuffer_type::depth_stencil:
            return vk::ImageUsageFlagBits::eDepthStencilAttachment;
        default:
            throw;
    }
}

inline vk::ImageAspectFlags aspects_for_type(framebuffer_type ty) {
    switch(ty) {
        case framebuffer_type::color:
            return vk::ImageAspectFlagBits::eColor;
        case framebuffer_type::depth:
            return vk::ImageAspectFlagBits::eDepth;
        case framebuffer_type::depth_stencil:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            throw;
    }
}

struct simple_geom_render_node_prototype : public render_node_prototype { 
    simple_geom_render_node_prototype(renderer* r, device* dev) {
        inputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color, framebuffer_mode::blend_input}
        };
        outputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
            framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth, framebuffer_mode::output},
        };

        desc_layout = dev->create_desc_set_layout({
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
        });

        vk::PushConstantRange push_consts[] = {
            vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) },
            vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(vec3) },
        };

        vk::DescriptorSetLayout desc_layouts[] = {
            desc_layout.get(),
            r->material_desc_set_layout.get()
        };

        pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
                {}, 2, desc_layouts, 2, push_consts
        });
    }

    size_t id() const override { return 0x0000fffd; }
    const char* name() const override { return "Simple Geometry"; }

    virtual void collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) override
    {
        pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1));
        layouts.push_back(desc_layout.get());
        outputs.push_back(&node->desc_set);
    }

    virtual void update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos) override 
    {
        auto b = buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)));
        writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer,
                    nullptr, b));
    }

    virtual vk::UniquePipeline generate_pipeline(renderer* r, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override {
        vk::PipelineShaderStageCreateInfo shader_stages[] = {
            vk::PipelineShaderStageCreateInfo {
                {}, vk::ShaderStageFlagBits::eVertex,
                r->dev->load_shader("full.vert.spv"), "main"
            },
            vk::PipelineShaderStageCreateInfo {
                {}, vk::ShaderStageFlagBits::eFragment,
                r->dev->load_shader("simple.frag.spv"), "main"
            }
        };

        auto vertex_binding = vk::VertexInputBindingDescription { 0, sizeof(vertex), vk::VertexInputRate::eVertex };
        auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo {
            {}, 1, &vertex_binding,
            3, vertex_attribute_description
        };

        auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo {
            {}, vk::PrimitiveTopology::eTriangleList
        };

        auto viewport_state = vk::PipelineViewportStateCreateInfo {
            {}, 1, &r->full_viewport, 1, &r->full_scissor
        };

        auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
            {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
            vk::FrontFace::eCounterClockwise, false, 0.f, 0.f, 0.f, 1.f
        };

        auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

        auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
            {}, true, true, vk::CompareOp::eLess, false, false
        };

        vk::PipelineColorBlendAttachmentState color_blend_att[] = {
            vk::PipelineColorBlendAttachmentState{}
        };
        color_blend_att[0].colorWriteMask = vk::ColorComponentFlagBits::eR
            |vk::ColorComponentFlagBits::eG |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA;
        auto color_blending_state = vk::PipelineColorBlendStateCreateInfo{
            {}, false, vk::LogicOp::eCopy, 1, color_blend_att
        };

        auto cfo = vk::GraphicsPipelineCreateInfo(
            {},
            2, shader_stages,
            &vertex_input_info,
            &input_assembly,
            nullptr,
            &viewport_state,
            &rasterizer_state,
            &multisample_state,
            &depth_stencil_state,
            &color_blending_state,
            nullptr,
            this->pipeline_layout.get(),
            render_pass, subpass
        );

        return r->dev->dev->createGraphicsPipelineUnique(nullptr, cfo);// .value;
    }

    void generate_command_buffer_inline(renderer* r, struct render_node* node, vk::CommandBuffer& cb, size_t subpass_index) override {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
                0, { node->desc_set.get() }, {});
        for(const auto&[mesh, world_transform] : r->active_meshes) {
            cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
                1, { mesh->mat->desc_set }, {});
            auto m = mesh->m;
            cb.bindVertexBuffers(0, {m->vertex_buffer->buf}, {0});
            cb.bindIndexBuffer(m->index_buffer->buf, 0, vk::IndexType::eUint16);
            cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { world_transform });
            cb.pushConstants<vec3>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4),
                    { vec3(mesh->mat ? mesh->mat->base_color : vec3(0.5f, 0.f, 0.2f)) });
            cb.drawIndexed(m->index_count, 1, 0, 0, 0);
        }
    }
};

struct output_render_node_prototype : public render_node_prototype { 
    output_render_node_prototype() {
        inputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
        };
        outputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
        };
    }

    size_t id() const override { return 0x0000ffff; }
    const char* name() const override { return "Display Output"; }

    vk::UniquePipeline generate_pipeline(renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override {
        return vk::UniquePipeline(nullptr);
    }
};


struct color_preview_render_node_prototype : public render_node_prototype { 
    struct node_data : public render_node_data {
        std::vector<ImTextureID> imtex;
        framebuffer_ref fb;
        node_data() : fb(-1) {}
        json serialize() const override  { return json{}; }
    };

    color_preview_render_node_prototype() {
        inputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
        };
        outputs = {};
    }

    size_t id() const override { return 0x0000fffe; }
    const char* name() const override { return "Preview [Color]"; }

    vk::UniquePipeline generate_pipeline(renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override {
        return vk::UniquePipeline(nullptr);
    }

    void build_gui(renderer* r, render_node* node) override {
        if(node->input_node(0) == nullptr) return;
        if(node->data == nullptr) {
            node->data = std::make_unique<node_data>();
        }
        auto* data = (node_data*)node->data.get();
        if(data->fb != node->input_framebuffer(0)) {
            auto& fb = r->buffers[node->input_framebuffer(0).value()];
            data->imtex.clear();
            for(size_t i = fb.is_array() ? 1 : 0; i < fb.image_views.size(); ++i) {
                data->imtex.emplace_back(ImGui_ImplVulkan_AddTexture(r->texture_sampler.get(), fb.image_views[i].get(),
                            (VkImageLayout)vk::ImageLayout::eGeneral));
            }
            data->fb = node->input_framebuffer(0).value();
        }
        for(auto& imtex : data->imtex) {
            if(imtex == nullptr) { ImGui::Text("invalid framebuffer"); continue; }
            ImGui::Image(imtex, ImVec2(r->full_viewport.width * 0.1,r->full_viewport.height * 0.1),
                    ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(0.7f,0.7f,0.7f,1));
        }
    }
};

render_node::render_node(std::shared_ptr<render_node_prototype> prototype)
    : visited(false),
		subpass_index(-1),
        pipeline(nullptr),
        desc_set(nullptr),

        id(rand()), prototype(prototype), inputs(prototype->inputs.size(), { {},0 }), outputs(prototype->outputs.size(), 0), data(nullptr)
{
}

render_node::render_node(renderer* r, size_t id, json data)
    : subpass_index(123456789), subpass_commands({}), id(id)
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
            ser_inputs.push_back(nullptr);
        } else {
            ser_inputs.push_back(json{
                    {"src_node", inp_node->lock()->id},
                    {"src_idx", inp_ix}
                    });
        }
    }
    std::cout << "b\n";
    return {
        { "prototype_id", this->prototype->id() },
            { "inputs", ser_inputs },
            { "data", this->data != nullptr ? this->data->serialize() : json(nullptr) }
    };
}

renderer::renderer() : dev(nullptr), next_id(10), desc_pool(nullptr), num_gpu_mats(0), should_recompile(false), log_compile(false), show_shapes(true) {}

void renderer::init(device* _dev) {
    this->dev = _dev;
    frame_uniforms_buf = std::make_unique<buffer>(dev, sizeof(frame_uniforms),
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

framebuffer_ref renderer::allocate_framebuffer(const framebuffer_desc& desc, uint32_t subpass_count) {
    for(auto& [ref, fb] : buffers) {
        if(fb.in_use) {
            if(fb.img->info.format == desc.format) {
                fb.in_use = true;
                return ref;
            }
        }
    }
    vk::UniqueImageView iv;
    auto format = desc.format;
    if(format == vk::Format::eUndefined) {
        switch(desc.type) {
            case framebuffer_type::color:
                format = swpc->format;
                break;
            case framebuffer_type::depth:
                format = vk::Format::eD32Sfloat; //swpc->depth_buf->info.format;
                break;
            case framebuffer_type::depth_stencil:
                format = vk::Format::eD24UnormS8Uint;
                break;
        }
    }
    auto actual_count = desc.count == framebuffer_count_is_subpass_count ? subpass_count : desc.count;
    auto isrg = vk::ImageSubresourceRange { aspects_for_type(desc.type), 0, 1, 0, actual_count };
    auto newfb = std::make_unique<image>(this->dev,
            vk::ImageCreateFlags(),
            vk::ImageType::e2D,
            vk::Extent3D { swpc->extent.width, swpc->extent.height, 1 },
            format,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eInputAttachment |
            vk::ImageUsageFlagBits::eSampled |
            usage_for_type(desc.type),
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            1, desc.count,
            &iv, desc.count > 1 ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D, isrg);
    framebuffer_ref id = next_id ++;
    std::vector<vk::UniqueImageView> ivs;
    ivs.emplace_back(std::move(iv));
    if(actual_count > 1) {
        isrg.layerCount = 1;
        for(uint32_t i = 0; i < actual_count; ++i) {
            isrg.baseArrayLayer = i;
            ivs.emplace_back(dev->dev->createImageViewUnique(
                vk::ImageViewCreateInfo {
                    vk::ImageViewCreateFlags(),
                    newfb->img,
                    vk::ImageViewType::e2D,
                    format,
                    vk::ComponentMapping(),
                    isrg
                }
            ));
        }
    }
    buffers.emplace(id, framebuffer_values{std::move(newfb), true, std::move(ivs), desc.type});
    return id;
}


void renderer::generate_subpasses(std::shared_ptr<render_node> node, std::vector<vk::SubpassDescription>& subpasses,
        std::vector<vk::SubpassDependency>& dependencies,
        const std::map<framebuffer_ref, uint32_t>& attachement_refs, arena<vk::AttachmentReference>& reference_pool)
{
    if(node->visited) { return; }
    if(log_compile) std::cout << "generate_subpasses for " << node->inputs.size() << " children of " << node->id << ":" << node->prototype->name() << "\n";
    node->visited = true;
    for(const auto& [input_node, input_index] : node->inputs) {
        if(!input_node.has_value()) continue;
        generate_subpasses(input_node->lock(), subpasses, dependencies, attachement_refs, reference_pool);
    }
    if(log_compile) std::cout << "generate_subpasses for " << node->id << ":" << node->prototype->name() << "\n";

    if(node.get() == screen_output_node.get()) {
        if(log_compile) std::cout << "\toutput node, skipped\n";
        return;
    }

    node->subpass_index = (uint32_t)subpasses.size();
    if(log_compile) std::cout << "\tassigning node subpass index #" << node->subpass_index << " x " << node->subpass_count << "\n";
    subpass_order.push_back(node);

    for(size_t rep_index = 0; rep_index < node->subpass_count; ++rep_index) {

        vk::AttachmentReference *input_atch_start = nullptr,
            *color_atch_start = nullptr,
            *depth_atch_start = nullptr;

        uint32_t num_inputs = 0;
        for(size_t i = 0; i < node->inputs.size(); ++i) {
            num_inputs +=
                node->prototype->inputs[i].subpass_binding_order == framebuffer_subpass_binding_order::parallel
                    ? node->prototype->inputs[i].count : 1;
        }
        input_atch_start = reference_pool.alloc_array(num_inputs);
        auto* input_atch_next = input_atch_start;
        uint32_t num_input_atch = 0;
        for(size_t i = 0; i < node->inputs.size(); ++i) {
            // skip blend fbs since they only need output attachment
            if(node->prototype->inputs[i].mode == framebuffer_mode::blend_input ||
                    node->prototype->inputs[i].mode == framebuffer_mode::shader_input) continue;
            const auto& [input_node, input_index] = node->inputs[i];
            if (!input_node.has_value()) continue;
            framebuffer_ref fb = input_node->lock()->outputs[input_index];
            if(node->prototype->inputs[i].subpass_binding_order == framebuffer_subpass_binding_order::parallel) {
                for(uint32_t ai = 0; ai < node->prototype->inputs[i].count; ++ai) {
                    *(input_atch_next++) = (vk::AttachmentReference {
                        attachement_refs.at(fb) + ai,
                        vk::ImageLayout::eShaderReadOnlyOptimal
                    });
                    num_input_atch++;
                }
            } else if(node->prototype->inputs[i].subpass_binding_order == framebuffer_subpass_binding_order::sequential) {
                *(input_atch_next++) = (vk::AttachmentReference {
                    attachement_refs.at(fb) + (uint32_t)rep_index,
                    vk::ImageLayout::eShaderReadOnlyOptimal
                });
                num_input_atch++;
            }
        }

        uint32_t num_color_out = 0;
        for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
            if(node->prototype->outputs[i].type == framebuffer_type::color && node->outputs[i] != 0) {
                num_color_out += 
                    node->prototype->outputs[i].subpass_binding_order == framebuffer_subpass_binding_order::parallel
                        ? node->prototype->outputs[i].count : 1;
            }
        }
        color_atch_start = reference_pool.alloc_array(num_color_out);
        auto* color_atch_next = color_atch_start;
        for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
            if(node->prototype->outputs[i].type == framebuffer_type::color && node->outputs[i] != 0) {
                if(node->prototype->outputs[i].subpass_binding_order == framebuffer_subpass_binding_order::parallel) {
                    for(uint32_t ai = 0; ai < node->prototype->outputs[i].count; ++ai) {
                        *(color_atch_next++) = (vk::AttachmentReference {
                            attachement_refs.at(node->outputs[i]) + ai,
                            vk::ImageLayout::eColorAttachmentOptimal
                        });
                    }
                } else if(node->prototype->outputs[i].subpass_binding_order == framebuffer_subpass_binding_order::sequential) {
                    *(color_atch_next++) = (vk::AttachmentReference {
                        attachement_refs.at(node->outputs[i]) + (uint32_t)rep_index,
                        vk::ImageLayout::eColorAttachmentOptimal
                    });
                }
            }
        }

        bool has_depth = false;
        for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
            if((node->prototype->outputs[i].type == framebuffer_type::depth ||
                    node->prototype->outputs[i].type == framebuffer_type::depth_stencil) && node->outputs[i] != 0) {
                has_depth = true;
                uint32_t offset = 0;
                if(node->prototype->outputs[i].count > 1) {
                    if(node->prototype->outputs[i].subpass_binding_order == framebuffer_subpass_binding_order::sequential) {
                        offset = (uint32_t)rep_index;
                    } else {
                        throw;
                    }
                }
                depth_atch_start = reference_pool.alloc(vk::AttachmentReference {
                    attachement_refs.at(node->outputs[i]) + offset,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal
                });
                break; //only one is possible
            }
        }
        if(!has_depth) depth_atch_start = nullptr;

        subpasses.emplace_back(
            vk::SubpassDescriptionFlags(),
            vk::PipelineBindPoint::eGraphics,
            num_input_atch, input_atch_start,
            num_color_out, color_atch_start,
            nullptr, depth_atch_start
        );

        // emit dependencies
        // see: https://developer.samsung.com/galaxy-gamedev/resources/articles/renderpasses.html
        // assuming that we are always consuming inputs in fragment shaders
        for(const auto& [_input_node, input_index] : node->inputs) {
            if(!_input_node.has_value()) continue;
            auto input_node = _input_node->lock();
            if (input_node == screen_output_node) continue;
            const auto& fb_desc = input_node->prototype->outputs[input_index];
            if(fb_desc.type == framebuffer_type::color && fb_desc.mode != framebuffer_mode::blend_input) {
                dependencies.emplace_back(
                    input_node->subpass_index, node->subpass_index + rep_index,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::DependencyFlagBits::eByRegion
                );
            } else if(fb_desc.type == framebuffer_type::depth || fb_desc.type == framebuffer_type::depth_stencil) {
                dependencies.emplace_back(
                    input_node->subpass_index, node->subpass_index + rep_index,
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::DependencyFlagBits::eByRegion
                );
            }
        }
    }
}

void renderer::propagate_blended_framebuffers(std::shared_ptr<render_node> node) {
    for(size_t i = 0; i < node->inputs.size(); ++i) {
        if(node->input_node(i) != nullptr && node->input_node(i) != screen_output_node) {
            this->propagate_blended_framebuffers(node->input_node(i));
        }
        if(node->prototype->inputs[i].mode == framebuffer_mode::blend_input) {
            if(node->input_node(i) != nullptr) {
                node->outputs[i] = node->input_framebuffer(i).value();
            } else {
                node->outputs[i] = this->allocate_framebuffer(node->prototype->outputs[i], node->subpass_count);
            }
        }
    }
}

void renderer::compile_render_graph() {
    // free all framebuffers we still have and do other clean up
    for(auto& buf : buffers) {
        buf.second.in_use = false;
    }
    for(auto& n : render_graph) {
        n->visited = false;
        for(auto& ou : n->outputs) ou = 0;
        n->desc_set.release();
    }

    // allocate framebuffers to each node - for now nothing fancy, just give each output its own buffer
    // assign the actual screen backbuffers
    auto&[color_src_node, color_src_ix] = screen_output_node->inputs[0];
    if(color_src_node.has_value()) color_src_node->lock()->outputs[color_src_ix] = 1;
    screen_output_node->outputs[0] = 1;
    prototypes[0]->inputs[0].format = swpc->format;
    for(auto& node : render_graph) {
        // compute subpass count early so we can use it from framebuffer counts as well
        node->subpass_count = node->prototype->subpass_repeat_count(this, node.get());

        for(size_t i = 0; i < node->outputs.size(); ++i) {
            // assign a new framebuffer to each output that is unassigned and not a blend input
            if(node->outputs[i] == 0 && !(i < node->prototype->inputs.size() && node->prototype->inputs[i].mode == framebuffer_mode::blend_input))
                node->outputs[i] = this->allocate_framebuffer(node->prototype->outputs[i], node->subpass_count);
        }
    }
    // copy blend mode framebuffers so that nodes that take a blend mode framebuffer also output to the same framebuffer
    this->propagate_blended_framebuffers(screen_output_node);

    // generate attachments
    std::vector<vk::AttachmentDescription> attachments {
        { vk::AttachmentDescriptionFlags(), //swapchain color
            swpc->format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR },
    };
    std::map<framebuffer_ref, uint32_t> attachement_refs;
    attachement_refs[1] = 0;

    clear_values.clear();
    clear_values.emplace_back(vk::ClearColorValue(std::array<float,4>{0.f, 0.0f, 0.0f, 0.f}));
    for(const auto& fb : buffers) {
        /* std::cout << "fb " << fb.first << "\n"; */
        if(!fb.second.in_use) continue;
        attachement_refs[fb.first] = (uint32_t)attachments.size();
        size_t num_layers = fb.second.num_layers(); //std::get<2>(fb.second).size() == 1 ? 1 : std::get<2>(fb.second).size()-1;
        for(size_t i = 0; i < num_layers; ++i) {
            attachments.emplace_back(vk::AttachmentDescriptionFlags(),
                    fb.second.img->info.format,
                    vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
            switch(fb.second.type) {
                case framebuffer_type::color:
                    clear_values.emplace_back(vk::ClearColorValue(std::array<float,4>{0.f, 0.f, 0.f, 0.f}));
                    break;
                case framebuffer_type::depth:
                case framebuffer_type::depth_stencil:
                    clear_values.emplace_back(vk::ClearDepthStencilValue(1.f, 0));
                    break;
            }
        }
    }

    // collect all subpasses and generate subpass dependencies, one per node except output
    std::vector<vk::SubpassDescription> subpasses;
    std::vector<vk::SubpassDependency> dependencies;
    arena<vk::AttachmentReference> reference_pool;
    subpass_order.clear();
    generate_subpasses(screen_output_node, subpasses, dependencies, attachement_refs, reference_pool);

    if(log_compile) {
        std::cout << "subpass dependencies:\n";
        for(size_t i = 0; i < dependencies.size(); ++i) {
            auto& d = dependencies[i];
            std::cout << i << ": src=" << d.srcSubpass << " dst=" << d.dstSubpass << "\n";
        }
        std::cout << "----------------------\n";
    }

    // create render pass
    vk::RenderPassCreateInfo rpcfo {
        {},
        (uint32_t)attachments.size(), attachments.data(),
        (uint32_t)subpasses.size(), subpasses.data(),
        (uint32_t)dependencies.size(), dependencies.data()
    };
    render_pass = dev->dev->createRenderPassUnique(rpcfo);

    this->render_pass_begin_info = vk::RenderPassBeginInfo {
        render_pass.get(), nullptr,
        vk::Rect2D(vk::Offset2D(), swpc->extent),
        (uint32_t)clear_values.size(), clear_values.data()
    };

   // create new framebuffers
    framebuffers  = swpc->create_framebuffers(render_pass.get(), [&](size_t index, std::vector<vk::ImageView>& att) {
            for(const auto& [ref, fb] : buffers) {
                if(!fb.in_use) continue;
                if(fb.is_array()) {
                    for(size_t i = 1; i < fb.image_views.size(); ++i) {
                        att.push_back(fb.image_views[i].get());
                    }
                } else {
                    att.push_back(fb.image_views[0].get());
                }
            }
    });

    // gather information about descriptors
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    std::vector<vk::DescriptorSetLayout> layouts;
    std::vector<vk::UniqueDescriptorSet*> outputs;
    for(const auto& node : subpass_order) {
         node->prototype->collect_descriptor_layouts(node.get(), pool_sizes, layouts, outputs);
    }

    /* std::cout << "desc_pool = " << desc_pool.get() << "\n"; */
    desc_pool = dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        (uint32)outputs.size(), (uint32)pool_sizes.size(), pool_sizes.data()
    });
    /* std::cout << "desc_pool = " << desc_pool.get() << "\n"; */
    auto sets = dev->dev->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo {
        desc_pool.get(), (uint32)layouts.size(), layouts.data()
    });
    for(size_t i = 0; i < sets.size(); ++i) {
        outputs[i]->swap(sets[i]);
    }

    // create each subpass pipeline and command buffer if possible
    // also gather descriptor writes
    std::vector<vk::WriteDescriptorSet> desc_writes;
    arena<vk::DescriptorBufferInfo> buf_infos;
    arena<vk::DescriptorImageInfo> img_infos;
    for(uint32_t i = 0; i < subpass_order.size(); ++i) {
        auto node = subpass_order[i];
        node->prototype->update_descriptor_sets(this, node.get(), desc_writes, buf_infos, img_infos);
        node->pipeline =
            node->prototype->generate_pipeline(this, node.get(), render_pass.get(), i);

        // generate command buffers
        node->subpass_commands = node->prototype->generate_command_buffer(this, node.get());
    }
    dev->dev->updateDescriptorSets(desc_writes, {});
    should_recompile = false;
}

void renderer::create_swapchain_dependencies(swap_chain* sc) {
    /* std::cout << "renderer::create_swapchain_dependencies\n"; */
    swpc = sc;
    full_viewport = vk::Viewport(0, 0, (float)swpc->extent.width, (float)swpc->extent.height, 0.f, 1.f);
    full_scissor = vk::Rect2D({}, swpc->extent);
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

#include "ImGuiFileDialog.h"

//using igfd::ImGuiFileDialog;

void renderer::build_gui(frame_state* fs) {
    if(!fs->gui_open_windows->at("Renderer")) return;
    if (!ImGui::Begin("Renderer", &fs->gui_open_windows->at("Renderer"), ImGuiWindowFlags_MenuBar)) { ImGui::End(); return; }
    
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("New graph")) {
                render_graph.clear();
                render_graph.push_back(screen_output_node);
                auto test = std::make_shared<render_node>(prototypes[1]);
                render_graph.push_back(test);
                screen_output_node->inputs[0] = {test, 0};
            }
            if(ImGui::MenuItem("Load graph")) {
                ImGuiFileDialog::Instance()->OpenDialog("LoadRenderGraphDlg", "Choose Render Graph", ".json", ".");
            }
            if(ImGui::MenuItem("Save graph")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveRenderGraphDlg", "Choose Render Graph", ".json", ".");
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Compilation log", nullptr, &log_compile);
            ImGui::MenuItem("Viewport shapes", nullptr, &show_shapes);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if(ImGui::BeginTabBar("##rendertabs")) {
        if(ImGui::BeginTabItem("Pipeline")) {
            ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

            ImNodes::BeginNodeEditor();

            gui_node_attribs.clear();
            gui_links.clear();
            int next_attrib_id = 1;
            std::vector<std::shared_ptr<render_node>> deleted_nodes;

            for (const auto& node : render_graph) {
                auto i = node->id;
                ImNodes::BeginNode((int)i);
                ImNodes::BeginNodeTitleBar();
                ImGui::Text("%s[%u]", node->prototype->name(), node->subpass_count);
                ImGui::SameLine();
                if(ImGui::SmallButton(" x ")) {
                    deleted_nodes.push_back(node);
                }
                ImNodes::EndNodeTitleBar();
                node->prototype->build_gui(this, node.get());

                // TODO: could we have blended inputs be represented by having the input dot and the output dot on the same line
                for (int input_ix = 0; input_ix < node->prototype->inputs.size(); ++input_ix) {
                    auto id = next_attrib_id++;
                    gui_node_attribs[id] = {node, input_ix, false};
                    ImNodes::BeginInputAttribute(id, node->prototype->inputs[input_ix].count > 1 ? ImNodesPinShape_Quad : ImNodesPinShape_Circle);
                    ImGui::Text("%s", node->prototype->inputs[input_ix].name.c_str());
                    ImNodes::EndInputAttribute();
                }

                for (int output_ix = 0; output_ix < node->prototype->outputs.size(); ++output_ix) {
                    auto id = next_attrib_id++;
                    gui_node_attribs[id] = {node, output_ix, true};
                    ImNodes::BeginOutputAttribute(id ,node->prototype->outputs[output_ix].count > 1 ? ImNodesPinShape_Quad : ImNodesPinShape_Circle);
                    ImGui::Text("%s [%lu]", node->prototype->outputs[output_ix].name.c_str(), node->outputs[output_ix]);
                    ImNodes::EndOutputAttribute();
                }

                ImNodes::EndNode();

                /*for (int input_ix = 0; input_ix < node->prototype->inputs.size(); ++input_ix) {
                  auto input_id = (i << 16 | input_ix);
                  auto& [src_node, src_index] = node->inputs[input_ix];
                  ImNodes::Link(i + input_id + src_index, 1 << 31 | ((int)src_node.get()) << 16 | src_index, input_id);
                  }*/
            }

            for (const auto& [attrib_id, scnd] : gui_node_attribs) {
                const auto& [node, input_ix, is_output] = scnd;
                if (is_output) continue;
                auto out_attrib_id = std::find_if(gui_node_attribs.begin(), gui_node_attribs.end(), [&](const auto& p) {
					return std::get<2>(p.second) && std::get<0>(p.second) == node->input_node(input_ix)
					    && std::get<1>(p.second) == node->inputs[input_ix].second;
                });
                if (out_attrib_id == gui_node_attribs.end()) continue;
                ImNodes::Link((int)gui_links.size(), attrib_id, out_attrib_id->first);
                gui_links.emplace_back( attrib_id, out_attrib_id->first );
            }

            if(ImGui::BeginPopupContextWindow()) {
                ImGui::Text("Create new node...");
                ImGui::Separator();
                for(const auto& node_type : prototypes) {
                    /* std::cout << node_type->name() << "\n"; */
                    if(ImGui::MenuItem(node_type->name())) {
                        render_graph.push_back(std::make_shared<render_node>(node_type));
                    }
                }
                ImGui::EndPopup();
            }

            /* ImNodes::MiniMap(0.1f, ImNodesMiniMapLocation_TopRight); */

            ImGui::SetCursorPos(ImVec2(8,8));
            if(ImGui::Button("Recompile"))
                should_recompile = true;
            if(ImGuiFileDialog::Instance()->Display("SaveRenderGraphDlg")) {
                if(ImGuiFileDialog::Instance()->IsOk()) {
                    std::ofstream output(ImGuiFileDialog::Instance()->GetFilePathName());
                    output << std::setw(4) << serialize_render_graph();
                }
                ImGuiFileDialog::Instance()->Close();
            }

            ImNodes::EndNodeEditor();

            if(ImGuiFileDialog::Instance()->Display("LoadRenderGraphDlg")) {
                if(ImGuiFileDialog::Instance()->IsOk()) {
                    std::ifstream input(ImGuiFileDialog::Instance()->GetFilePathName());
                    json data;
                    input >> data;
                    this->deserialize_render_graph(data);
                }
                ImGuiFileDialog::Instance()->Close();
            }



            int start_attrib, end_attrib;
            if(ImNodes::IsLinkCreated(&start_attrib, &end_attrib)) {
                if(std::get<2>(gui_node_attribs[start_attrib])) { // make sure start=input and end=output
                    std::swap(start_attrib, end_attrib);
                }
                auto&[input_node, input_ix, is_input] = gui_node_attribs[start_attrib];
                auto&[output_node, output_ix, is_output] = gui_node_attribs[end_attrib];
                input_node->inputs[input_ix] = {output_node, output_ix};
            }

            int link_id;
            if(ImNodes::IsLinkDestroyed(&link_id)) {
                auto&[start_attrib, end_attrib] = gui_links[link_id];
                auto&[input_node, input_ix, is_input] = gui_node_attribs[start_attrib];
                auto&[output_node, output_ix, is_output] = gui_node_attribs[end_attrib];
                input_node->inputs[input_ix] = { {},0 };
                output_node->outputs[output_ix] = 0;
            }

            for(const auto& n : deleted_nodes) {
                auto f = std::find(render_graph.begin(), render_graph.end(), n);
                if(f != render_graph.end())
                    render_graph.erase(f);
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Statistics")) {
            ImGui::Text("%zu active meshes, %zu active lights, %zu active shapes, %zu running subpasses",
                    active_meshes.size(), active_lights.size(), active_shapes.size(), subpass_order.size());
            ImGui::Text("%zu temp command buffers, %zu temp upload buffers", dev->tmp_cmd_buffers.size(), dev->tmp_upload_buffers.size());

            ImGui::Separator();
            ImGui::Text("Framebuffers:");
            if(ImGui::BeginTable("##RenderFramebufferTable", 5)) {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("In use?");
                ImGui::TableSetupColumn("Format");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Count");
                ImGui::TableHeadersRow();
                for(const auto& [id, fb] : buffers) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%lu", id);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", fb.in_use ? "Y" : "N");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", vk::to_string(fb.img->info.format).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u x %u", fb.img->info.extent.width, fb.img->info.extent.height);
                    ImGui::TableNextColumn();
                    ImGui::Text("%lu", fb.num_layers());
                }
                ImGui::EndTable();
            }
			ImGui::EndTabItem();
        }
        
		if (ImGui::BeginTabItem("Textures")) {
			if (ImGui::BeginTable("#RenderTextureTable", 4, ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Format");
                ImGui::TableSetupColumn("Size");
				ImGui::TableSetupColumn("Preview");
				ImGui::TableHeadersRow();
				for (auto& [name, img, imv, imtex] : texture_cache) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%s", name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", vk::to_string(img->info.format).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u x %u", img->info.extent.width, img->info.extent.height);
					ImGui::TableNextColumn();
                    if(imtex == 0) {
                        imtex = (uint64_t)ImGui_ImplVulkan_AddTexture((VkSampler)texture_sampler.get(),
                                    imv.get(),
                                    (VkImageLayout)vk::ImageLayout::eShaderReadOnlyOptimal);
                    }
					ImGui::Image((ImTextureID)imtex, ImVec2(128, 128));
				}
				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}

        ImGui::EndTabBar();
    }
    ImGui::End();
}

void renderer::traverse_scene_graph(scene_object* obj, frame_state* fs, const mat4& parent_T) {
    mat4 T = parent_T;
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
        auto mmt = (mesh_trait*)mt->second.get();
        if (mmt->m != nullptr)
            active_meshes.push_back({ mmt, T });
    }

    auto lt = obj->traits.find(TRAIT_ID_LIGHT);
    if(lt != obj->traits.end()) {
        auto llt = (light_trait*)lt->second.get();
        active_lights.push_back({llt, T});
    }

    if(obj == current_scene->active_camera.get()) {
        auto cam = (camera_trait*)obj->traits.find(TRAIT_ID_CAMERA)->second.get();
        mapped_frame_uniforms->proj = glm::perspective(cam->fov,
                (float)swpc->extent.width / (float)swpc->extent.height, 0.1f, 2000.f);
        mapped_frame_uniforms->view = inverse(T);
    }

    for (auto& [_, t] : obj->traits) {
        t->postprocess_transform(obj, T, fs);
    }

    for(auto c : obj->children)
        traverse_scene_graph(c.get(), fs, T);
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
	texture_cache.push_back({ name, img, std::move(img_view), 0 });
	return index;
}

void renderer::update(frame_state* fs) {
    if(should_recompile || materials_buf == nullptr || current_scene->materials_changed) {
        dev->graphics_qu.waitIdle();
        dev->present_qu.waitIdle();
    }
    
    if(materials_buf == nullptr || current_scene->materials_changed) {
        if(current_scene->materials.size() != 0) {
            // recreate material buffer if necessary
            bool recreating_mat_buf = materials_buf == nullptr || num_gpu_mats != current_scene->materials.size();
            if(recreating_mat_buf) {
                num_gpu_mats = (uint32_t)current_scene->materials.size();
                // we could probably move the materials ubuffer into the material desc set
                // and then use desc set offsets instead of push constants
                // I guess that wouldn't work well for lights
                materials_buf = std::make_unique<buffer>(dev, sizeof(gpu_material)*num_gpu_mats,
                        vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent,
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
			auto default_info = img_infos.alloc(vk::DescriptorImageInfo{
						texture_sampler.get(), std::get<2>(texture_cache[0]).get(),
						vk::ImageLayout::eShaderReadOnlyOptimal
			});

            uplcb.begin(vk::CommandBufferBeginInfo { vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
            for(uint32_t i = 0; i < current_scene->materials.size(); ++i) {
                mapped_materials[i] = gpu_material(current_scene->materials[i].get());
                current_scene->materials[i]->_render_index = i;
                if(current_scene->materials[i]->diffuse_texpath.has_value()) {
                    auto ix = load_texture(current_scene->materials[i]->diffuse_texpath.value(), uplcb);
					auto info = img_infos.alloc(vk::DescriptorImageInfo {
						texture_sampler.get(), std::get<2>(texture_cache[ix]).get(),
						vk::ImageLayout::eShaderReadOnlyOptimal
					});
					desc_writes.push_back(vk::WriteDescriptorSet(current_scene->materials[i]->desc_set,
						0, 0, 1,
						vk::DescriptorType::eCombinedImageSampler, info));
                } else {
					desc_writes.push_back(vk::WriteDescriptorSet(current_scene->materials[i]->desc_set,
						0, 0, 1,
						vk::DescriptorType::eCombinedImageSampler, default_info));
                }
			}
			uplcb.end();
			dev->graphics_qu.submit({ vk::SubmitInfo(0,nullptr,nullptr,1,&uplcb) }, nullptr);

            if(!should_recompile && recreating_mat_buf) {
                // make sure render node descriptor sets are up to date
                for(size_t i = 0; i < subpass_order.size(); ++i) {
                    auto node = subpass_order[i];
                    node->prototype->update_descriptor_sets(this, node.get(), desc_writes, buf_infos, img_infos);
                }
            }
            dev->dev->updateDescriptorSets(desc_writes, {});
        }
    }
    if(should_recompile) compile_render_graph();
}

void renderer::render(vk::CommandBuffer& cb, uint32_t image_index, frame_state* fs) {
    active_meshes.clear();
    active_lights.clear();
    active_shapes.clear();
    traverse_scene_graph(current_scene->root.get(), fs, mat4(1));

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
    auto staging_map = staging_buffer->map();
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
