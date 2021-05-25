#include "renderer.h"
#include "imgui.h"
#include "imnodes.h"

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
    simple_geom_render_node_prototype(device* dev) {
        inputs = {
        };
        outputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
            framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth},
        };

        desc_layout = dev->create_desc_set_layout({
            vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
        });

        vk::PushConstantRange push_consts[] = {
            vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) },
            vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(vec4) }
        };

        pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
                {}, 1, &desc_layout.get(), 2, push_consts
        });
    }

    const char* name() const override { return "Simple Geometry"; }

    virtual void collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) override
    {
        pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1));
        layouts.push_back(desc_layout.get());
        outputs.push_back(&node->desc_set);
    }

    virtual void update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, std::vector<vk::DescriptorBufferInfo>& buf_infos, std::vector<vk::DescriptorImageInfo>& img_infos) override 
    {
        buf_infos.push_back(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)));
        writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer,
                    nullptr, &buf_infos[buf_infos.size()-1]));
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

        return r->dev->dev->createGraphicsPipelineUnique(nullptr, cfo).value;
    }

    virtual void generate_command_buffer_inline(renderer* r, struct render_node* node, vk::CommandBuffer& cb) override {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
                0, { node->desc_set.get() }, {});
        for(const auto&[mesh, world_transform] : r->active_meshes) {
            cb.bindVertexBuffers(0, {mesh->vertex_buffer->buf}, {0});
            cb.bindIndexBuffer(mesh->index_buffer->buf, 0, vk::IndexType::eUint16);
            cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { world_transform });
            cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4),
                    { vec4(0.6f,0.55f,0.5f,1.f) });
            cb.drawIndexed(mesh->index_count, 1, 0, 0, 0);
        }
    }
};

struct output_render_node_prototype : public render_node_prototype { 
    output_render_node_prototype() {
        inputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
        };
        outputs = {};
    }

    const char* name() const override { return "Display Output"; }

    virtual vk::UniquePipeline generate_pipeline(renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override {
        return vk::UniquePipeline(nullptr);
    }
};


struct color_preview_render_node_prototype : public render_node_prototype { 
    color_preview_render_node_prototype() {
        inputs = {
            framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
        };
        outputs = {};
    }

    const char* name() const override { return "Preview [Color]"; }

    virtual vk::UniquePipeline generate_pipeline(renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override {
        return vk::UniquePipeline(nullptr);
    }

    void build_gui(renderer* r, render_node* node) override {
        if(node->inputs[0].first == nullptr) return;
        auto& [fb_img, fb_alloc, fb_img_view, fb_type] = r->buffers[node->inputs[0].first->outputs[node->inputs[0].second]];
        if(fb_img == nullptr) { ImGui::Text("invalid framebuffer"); return; }
        ImGui::Image((void*)fb_img->img, ImVec2(256,256));
    }
};



render_node::render_node(std::shared_ptr<render_node_prototype> prototype)
    : prototype(prototype),
        inputs(prototype->inputs.size(), {nullptr,0}),
        outputs(prototype->outputs.size(), 0),
        data(nullptr),
        id(rand()),

        visited(false), subpass_index(-1), subpass_commands(std::nullopt), pipeline(nullptr), desc_set(nullptr)
{
}

renderer::renderer(device* dev, std::shared_ptr<scene> s) : dev(dev), current_scene(s), next_id(10), desc_pool(nullptr), should_recompile(false) {
    prototypes = {
        std::make_shared<output_render_node_prototype>(),
        std::make_shared<simple_geom_render_node_prototype>(dev),
        std::make_shared<color_preview_render_node_prototype>(),
    };
    screen_output_node = std::make_shared<render_node>(prototypes[0]);
    render_graph.push_back(screen_output_node);
    auto test = std::make_shared<render_node>(prototypes[1]);
    render_graph.push_back(test);
    screen_output_node->inputs[0] = {test, 0};

    frame_uniforms_buf = std::make_unique<buffer>(dev, sizeof(frame_uniforms),
            vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostCoherent,
            (void**)&mapped_frame_uniforms);
}

framebuffer_ref renderer::allocate_framebuffer(const framebuffer_desc& desc) {
    for(auto& buf : buffers) {
        if(!std::get<1>(buf.second)) {
            if(std::get<0>(buf.second)->info.format == desc.format) {
                std::get<1>(buf.second) = true;
                return buf.first;
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
    auto newfb = std::make_unique<image>(this->dev, vk::ImageType::e2D,
            vk::Extent3D { swpc->extent.width, swpc->extent.height, 1 },
            format,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eInputAttachment |
            vk::ImageUsageFlagBits::eSampled |
            usage_for_type(desc.type),
            vk::MemoryPropertyFlagBits::eDeviceLocal,
            &iv, vk::ImageViewType::e2D,
            vk::ImageSubresourceRange { aspects_for_type(desc.type), 0, 1, 0, 1 });
    framebuffer_ref id = next_id ++;
    buffers[id] = std::move(std::tuple{std::move(newfb), true, std::move(iv), desc.type});
    return id;
}


void renderer::generate_subpasses(std::shared_ptr<render_node> node, std::vector<vk::SubpassDescription>& subpasses,
        std::vector<vk::SubpassDependency>& dependencies,
        const std::map<framebuffer_ref, uint32_t>& attachement_refs, std::vector<vk::AttachmentReference>& reference_pool)
{
    if(node->visited) return;
    node->visited = true;
    for(const auto& [input_node, input_index] : node->inputs) {
        if(input_node == nullptr) continue;
        generate_subpasses(input_node, subpasses, dependencies, attachement_refs, reference_pool);
    }

    if(node.get() == screen_output_node.get()) return;

    vk::AttachmentReference *input_atch_start, *color_atch_start, *depth_atch_start;
    input_atch_start = reference_pool.data() + reference_pool.size();
    for(const auto& [input_node, input_index] : node->inputs) {
        framebuffer_ref fb = input_node->outputs[input_index];
        reference_pool.push_back(vk::AttachmentReference {
            attachement_refs.at(fb),
            vk::ImageLayout::eShaderReadOnlyOptimal 
        });
    }
    uint32_t num_color_out = 0;
    color_atch_start = reference_pool.data() + reference_pool.size();
    for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
        if(node->prototype->outputs[i].type == framebuffer_type::color && node->outputs[i] != 0) {
            reference_pool.push_back(vk::AttachmentReference {
                    attachement_refs.at(node->outputs[i]),
                    vk::ImageLayout::eColorAttachmentOptimal
            });
            num_color_out++;
        }
    }
    depth_atch_start = reference_pool.data() + reference_pool.size();
    for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
        if(node->prototype->outputs[i].type == framebuffer_type::depth ||
                node->prototype->outputs[i].type == framebuffer_type::depth_stencil && node->outputs[i] != 0) {
            reference_pool.push_back(vk::AttachmentReference {
                    attachement_refs.at(node->outputs[i]),
                    vk::ImageLayout::eDepthStencilAttachmentOptimal
            });
            break; //only one is possible
        }
    }

    node->subpass_index = (uint32_t)subpasses.size();
    subpass_order.push_back(node);
    subpasses.push_back(vk::SubpassDescription {
        vk::SubpassDescriptionFlags(),
        vk::PipelineBindPoint::eGraphics,
        (uint32_t)node->inputs.size(), input_atch_start,
        num_color_out, color_atch_start,
        nullptr, depth_atch_start,
    });

    // emit dependencies
    // see: https://developer.samsung.com/galaxy-gamedev/resources/articles/renderpasses.html
    // assuming that we are always consuming inputs in fragment shaders
    for(const auto& [input_node, input_index] : node->inputs) {
        const auto& fb_desc = input_node->prototype->outputs[input_index];
        if(fb_desc.type == framebuffer_type::color) {
            dependencies.push_back(vk::SubpassDependency {
                input_node->subpass_index, node->subpass_index,
                vk::PipelineStageFlagBits::eColorAttachmentOutput,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eColorAttachmentWrite,
                vk::AccessFlagBits::eShaderRead,
                vk::DependencyFlagBits::eByRegion
            });
        } else if(fb_desc.type == framebuffer_type::depth || fb_desc.type == framebuffer_type::depth_stencil) {
            dependencies.push_back(vk::SubpassDependency {
                input_node->subpass_index, node->subpass_index,
                vk::PipelineStageFlagBits::eLateFragmentTests,
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                vk::AccessFlagBits::eShaderRead,
                vk::DependencyFlagBits::eByRegion
            });
        }
    }
}

void renderer::compile_render_graph() {
    // free all framebuffers we still have and do other clean up
    dev->graphics_qu.waitIdle();
    for(auto& buf : buffers) {
        std::get<1>(buf.second) = false;
    }
    for(auto& n : render_graph) {
        n->visited = false;
        for(auto& ou : n->outputs) ou = 0;
        n->desc_set.release();
    }

    // allocate framebuffers to each node - for now nothing fancy, just give each output its own buffer
    // assign the actual screen backbuffers
    auto&[color_src_node, color_src_ix] = screen_output_node->inputs[0];
    if(color_src_node != nullptr) color_src_node->outputs[color_src_ix] = 1;
    prototypes[0]->inputs[0].format = swpc->format;
    for(auto& node : render_graph) {
        for(size_t i = 0; i < node->outputs.size(); ++i) {
            if(node->outputs[i] == 0)
                node->outputs[i] = this->allocate_framebuffer(node->prototype->outputs[i]);
        }
    }

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
    clear_values.push_back(vk::ClearColorValue(std::array<float,4>{0.f, 0.2f, 0.9f, 1.f}));
    for(const auto& fb : buffers) {
        std::cout << "fb " << fb.first << "\n";
        if(!std::get<1>(fb.second)) continue;
        attachement_refs[fb.first] = attachments.size();
        attachments.push_back(vk::AttachmentDescription { vk::AttachmentDescriptionFlags(),
                std::get<0>(fb.second)->info.format,
                vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                vk::ImageLayout::eUndefined, vk::ImageLayout::eUndefined });
        switch(std::get<3>(fb.second)) {
            case framebuffer_type::color:
                clear_values.push_back(vk::ClearColorValue(std::array<float,4>{0.f, 0.f, 0.f, 0.f}));
                break;
            case framebuffer_type::depth:
            case framebuffer_type::depth_stencil:
                clear_values.push_back(vk::ClearDepthStencilValue(1.f, 0));
                break;
        }
    }

    // collect all subpasses and generate subpass dependencies, one per node except output
    std::vector<vk::SubpassDescription> subpasses;
    std::vector<vk::SubpassDependency> dependencies;
    std::vector<vk::AttachmentReference> reference_pool;
    reference_pool.reserve(8);
    subpass_order.clear();
    generate_subpasses(screen_output_node, subpasses, dependencies, attachement_refs, reference_pool);

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
            for(const auto& fb : buffers) {
                if(!std::get<1>(fb.second)) continue;
                att.push_back(std::get<2>(fb.second).get());
            }
    });

    // gather information about descriptors
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    std::vector<vk::DescriptorSetLayout> layouts;
    std::vector<vk::UniqueDescriptorSet*> outputs;
    for(size_t i = 0; i < subpass_order.size(); ++i) {
        auto node = subpass_order[i];
        node->prototype->collect_descriptor_layouts(node.get(), pool_sizes, layouts, outputs);
    }
    std::cout << "desc_pool = " << desc_pool.get() << "\n";
    desc_pool = std::move(dev->dev->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        (uint32)outputs.size(), (uint32)pool_sizes.size(), pool_sizes.data()
    }));
    std::cout << "desc_pool = " << desc_pool.get() << "\n";
    auto sets = dev->dev->allocateDescriptorSetsUnique(vk::DescriptorSetAllocateInfo {
        desc_pool.get(), (uint32)layouts.size(), layouts.data()
    });
    for(size_t i = 0; i < sets.size(); ++i) {
        outputs[i]->swap(sets[i]);
    }


    // create each subpass pipeline and command buffer if possible
    // also gather descriptor writes
    std::vector<vk::WriteDescriptorSet> desc_writes;
    std::vector<vk::DescriptorBufferInfo> buf_infos;
    std::vector<vk::DescriptorImageInfo> img_infos;
    for(size_t i = 0; i < subpass_order.size(); ++i) {
        auto node = subpass_order[i];
        node->prototype->update_descriptor_sets(this, node.get(), desc_writes, buf_infos, img_infos);
        node->pipeline =
            node->prototype->generate_pipeline(this, node.get(), render_pass.get(), i);

        // generate command buffers
        auto subpass_commands = node->prototype->generate_command_buffer(this, node.get());
        if(subpass_commands.has_value()) {
            node->subpass_commands.swap(subpass_commands);
        }
    }
    dev->dev->updateDescriptorSets(desc_writes, {});
    should_recompile = false;
}

void renderer::create_swapchain_dependencies(swap_chain* sc) {
    std::cout << "renderer::create_swapchain_dependencies\n";
    swpc = sc;
    full_viewport = vk::Viewport(0, 0, swpc->extent.width, swpc->extent.height, 0.f, 1.f);
    full_scissor = vk::Rect2D({}, swpc->extent);
    buffers.clear();
    this->compile_render_graph();
}

void renderer::build_gui() {
    if (!ImGui::Begin("Renderer")) { ImGui::End(); return; }

    if(ImGui::BeginTabBar("##rendertabs")) {
        if(ImGui::BeginTabItem("Pipeline")) {
            ImNodes::GetIO().LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

            ImNodes::BeginNodeEditor();

            gui_node_attribs.clear();
            gui_links.clear();
            int next_attrib_id = 1;
            std::vector<std::shared_ptr<render_node>> deleted_nodes;

            for (const auto& node : render_graph) {
                auto i = (size_t)node.get();
                ImNodes::BeginNode(i);
                ImNodes::BeginNodeTitleBar();
                ImGui::Text("%s", node->prototype->name());
                ImGui::SameLine();
                if(ImGui::SmallButton(" x ")) {
                    deleted_nodes.push_back(node);
                }
                ImNodes::EndNodeTitleBar();
                node->prototype->build_gui(this, node.get());

                for (int input_ix = 0; input_ix < node->prototype->inputs.size(); ++input_ix) {
                    auto id = next_attrib_id++;
                    gui_node_attribs[id] = {node, input_ix, false};
                    ImNodes::BeginInputAttribute(id, ImNodesPinShape_Circle);
                    ImGui::Text("%s", node->prototype->inputs[input_ix].name.c_str());
                    ImNodes::EndInputAttribute();
                }

                for (int output_ix = 0; output_ix < node->prototype->outputs.size(); ++output_ix) {
                    auto id = next_attrib_id++;
                    gui_node_attribs[id] = {node, output_ix, true};
                    ImNodes::BeginOutputAttribute(id, ImNodesPinShape_Circle);
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
                        return std::get<2>(p.second) && std::get<0>(p.second) == node->inputs[input_ix].first
                        && std::get<1>(p.second) == node->inputs[input_ix].second;
                        });
                if (out_attrib_id == gui_node_attribs.end()) continue;
                ImNodes::Link(gui_links.size(), attrib_id, out_attrib_id->first);
                gui_links.push_back({ attrib_id, out_attrib_id->first });
            }

            if(ImGui::BeginPopupContextWindow()) {
                ImGui::Text("Create new node...");
                ImGui::Separator();
                for(const auto& node_type : prototypes) {
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

            ImNodes::EndNodeEditor();

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
                input_node->inputs[input_ix] = {nullptr,0};
                output_node->outputs[output_ix] = 0;
            }

            for(auto n : deleted_nodes) {
                auto f = std::find(render_graph.begin(), render_graph.end(), n);
                if(f != render_graph.end())
                    render_graph.erase(f);
            }
            ImGui::EndTabItem();
        }

        if(ImGui::BeginTabItem("Statistics")) {
            ImGui::Text("%zu active meshes, %zu running subpasses", active_meshes.size(), subpass_order.size());

            ImGui::Separator();
            ImGui::Text("Framebuffers:");
            if(ImGui::BeginTable("##RenderFramebufferTable", 4)) {
                ImGui::TableSetupColumn("ID");
                ImGui::TableSetupColumn("In use?");
                ImGui::TableSetupColumn("Format");
                ImGui::TableSetupColumn("Size");
                ImGui::TableHeadersRow();
                for(const auto& [id, _fb] : buffers) {
                    const auto&[img, allocated, imv, type] = _fb;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%lu", id);
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", allocated ? "Y" : "N");
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", vk::to_string(img->info.format).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%u x %u", img->info.extent.width, img->info.extent.height);
                }
                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }


        ImGui::EndTabBar();
    }
    ImGui::End();
}

void renderer::traverse_scene_graph(scene_object* obj, frame_state* fs) {
    auto mt = obj->traits.find(TRAIT_ID_MESH);
    if(mt != obj->traits.end()) {
        auto mmt = (mesh_trait*)mt->second.get();
        mat4 T = mat4(1); //need to get T from parent
        for(auto&[_, t] : obj->traits) {
            t->append_transform(obj, T, fs);
        }
        active_meshes.push_back({mmt->m.get(), T});
    }
    
    for(auto c : obj->children)
        traverse_scene_graph(c.get(), fs);
}

void renderer::render(vk::CommandBuffer& cb, uint32_t image_index, frame_state* fs) {
    mapped_frame_uniforms->proj = current_scene->cam.proj((float)swpc->extent.width / (float)swpc->extent.height);
    mapped_frame_uniforms->view = current_scene->cam.view();

    active_meshes.clear();
    traverse_scene_graph(current_scene->root.get(), fs);

    render_pass_begin_info.framebuffer = framebuffers[image_index].get();
    cb.beginRenderPass(render_pass_begin_info,
            subpass_order[0]->subpass_commands.has_value() ? vk::SubpassContents::eSecondaryCommandBuffers
                : vk::SubpassContents::eInline);

    for(size_t i = 0; i < subpass_order.size(); ++i) {
        if(subpass_order[i]->subpass_commands.has_value()) {
            cb.executeCommands({subpass_order[i]->subpass_commands.value().get()});
        } else {
            subpass_order[i]->prototype->generate_command_buffer_inline(this, subpass_order[i].get(), cb);
        }

        if(i+1 < subpass_order.size()) {
            cb.nextSubpass(subpass_order[i+1]->subpass_commands.has_value() ? vk::SubpassContents::eSecondaryCommandBuffers
                    : vk::SubpassContents::eInline);
        }
    }

    cb.endRenderPass();
}

renderer::~renderer() {
    for(auto& n : render_graph) {
        n->desc_set.release();
    }
}

mesh::mesh(device* dev, size_t vcount, size_t icount, std::function<void(void*)> write_buffer)
    : vertex_count(vcount), index_count(icount)
{
    auto vsize = sizeof(vertex)*vcount,
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

mesh::mesh(device* dev, const std::vector<vertex>& vertices, const std::vector<uint16>& indices)
    : mesh(dev, vertices.size(), indices.size(), [&](void* staging_map) {
        memcpy(staging_map, vertices.data(), sizeof(vertex)*vertices.size());
        memcpy((char*)staging_map + sizeof(vertex)*vertices.size(), indices.data(), sizeof(uint16)*indices.size());
    })
{
}

void mesh_trait::build_gui(struct scene_object*, frame_state*) {
    ImGui::Text("%zu vertices, %zu indices", m->vertex_count, m->index_count);
}
