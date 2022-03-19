#include "renderer.h"

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

void make_attachment_ref(vk::AttachmentReference*& output, framebuffer_ref fb, const framebuffer_desc& fb_desc, const std::map<framebuffer_ref, uint32_t>& attachment_refs, uint32_t rep_index, vk::ImageLayout layout) {
    if(fb_desc.subpass_binding_order == framebuffer_subpass_binding_order::parallel) {
        for(uint32_t ai = 0; ai < fb_desc.count; ++ai) {
            *output = vk::AttachmentReference { attachment_refs.at(fb) + ai, layout };
            output++;
        }
    } else if(fb_desc.subpass_binding_order == framebuffer_subpass_binding_order::sequential) {
        *output = vk::AttachmentReference { attachment_refs.at(fb) + rep_index, layout };
        output++;
    }
}

void make_node_subpass_input_attachments(vk::SubpassDescription& subpass, render_node* node,
        arena<vk::AttachmentReference>& reference_pool, size_t rep_index, const std::map<framebuffer_ref, uint32_t>& attachment_refs)
{
    for(const auto& input : node->prototype->inputs) {
        // skip blend fbs since they only need output attachment
        if(input.mode == framebuffer_mode::blend_input ||
                input.mode == framebuffer_mode::shader_input) continue;
        subpass.inputAttachmentCount += input.subpass_binding_order == framebuffer_subpass_binding_order::parallel ? input.count : 1;
    }
    auto* input_atch_next = reference_pool.alloc_array(subpass.inputAttachmentCount);
    subpass.pInputAttachments = input_atch_next;
    for(size_t i = 0; i < node->inputs.size(); ++i) {
        // skip blend fbs since they only need output attachment
        if(node->prototype->inputs[i].mode == framebuffer_mode::blend_input ||
                node->prototype->inputs[i].mode == framebuffer_mode::shader_input) continue;
        const auto& [input_node, input_index] = node->inputs[i];
        if (!input_node.has_value()) continue;
        make_attachment_ref(input_atch_next, input_node->lock()->outputs[input_index], node->prototype->inputs[i],
                attachment_refs, rep_index, vk::ImageLayout::eShaderReadOnlyOptimal);
    }
}

void make_node_subpass_color_attachments(vk::SubpassDescription& subpass, render_node* node,
        arena<vk::AttachmentReference>& reference_pool, size_t rep_index, const std::map<framebuffer_ref, uint32_t>& attachment_refs)
{
    for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
        const auto& output = node->prototype->outputs[i];
        if(output.type == framebuffer_type::color && node->outputs[i] != 0) {
            subpass.colorAttachmentCount += output.subpass_binding_order == framebuffer_subpass_binding_order::parallel ? output.count : 1;
        }
    }
    auto* color_atch_next = reference_pool.alloc_array(subpass.colorAttachmentCount);
    subpass.pColorAttachments = color_atch_next;
    for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
        if(node->prototype->outputs[i].type == framebuffer_type::color && node->outputs[i] != 0) {
            make_attachment_ref(color_atch_next, node->outputs[i], node->prototype->outputs[i],
                    attachment_refs, rep_index, vk::ImageLayout::eColorAttachmentOptimal);
        }
    }
}

void make_node_subpass_depth_attachment(vk::SubpassDescription& subpass, render_node* node,
        arena<vk::AttachmentReference>& reference_pool, size_t rep_index, const std::map<framebuffer_ref, uint32_t>& attachment_refs)
{
    for(size_t i = 0; i < node->prototype->outputs.size(); ++i) {
        if((node->prototype->outputs[i].type == framebuffer_type::depth ||
                    node->prototype->outputs[i].type == framebuffer_type::depth_stencil) && node->outputs[i] != 0) {
            uint32_t offset = 0;
            if(node->prototype->outputs[i].count > 1) {
                if(node->prototype->outputs[i].subpass_binding_order == framebuffer_subpass_binding_order::sequential) {
                    offset = (uint32_t)rep_index;
                } else {
                    throw; // can't render to multiple depth buffers in parallel, at least for now
                }
            }
            subpass.pDepthStencilAttachment = reference_pool.alloc(vk::AttachmentReference {
                    attachment_refs.at(node->outputs[i]) + offset,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal
                    });
            break; //only one is possible
        }
    }
}

void emit_node_subpass_dependencies(render_node* node, std::vector<vk::SubpassDependency>& dependencies, size_t rep_index) {
    // see: https://developer.samsung.com/galaxy-gamedev/resources/articles/renderpasses.html
    // assuming that we are always consuming inputs in fragment shaders
    // TODO: create dependencies between repeated subpasses?
    for(const auto& [_input_node, input_index] : node->inputs) {
        if(!_input_node.has_value()) continue;
        auto input_node = _input_node->lock();
        if (input_node->prototype->id() == 0x0000ffff) continue; // screen output node
        const auto& fb_desc = input_node->prototype->outputs[input_index];
        if(fb_desc.type == framebuffer_type::color && fb_desc.mode != framebuffer_mode::blend_input) {
            dependencies.emplace_back(
                    input_node->subpass_index, node->subpass_index + rep_index,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eColorAttachmentWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::DependencyFlagBits::eByRegion);
        } else if(fb_desc.type == framebuffer_type::depth || fb_desc.type == framebuffer_type::depth_stencil) {
            dependencies.emplace_back(
                    input_node->subpass_index, node->subpass_index + rep_index,
                    vk::PipelineStageFlagBits::eLateFragmentTests,
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::DependencyFlagBits::eByRegion);
        }
    }
}

void renderer::generate_subpasses(const std::shared_ptr<render_node>& node, std::vector<vk::SubpassDescription>& subpasses,
        std::vector<vk::SubpassDependency>& dependencies,
        const std::map<framebuffer_ref, uint32_t>& attachment_refs, arena<vk::AttachmentReference>& reference_pool)
{
    if(node->visited) { return; }
    if(log_compile) std::cout << "generate_subpasses for " << node->inputs.size() << " children of " << node->id << ":" << node->prototype->name() << "\n";
    node->visited = true;
    for(const auto& [input_node, input_index] : node->inputs) {
        if(!input_node.has_value()) continue;
        generate_subpasses(input_node->lock(), subpasses, dependencies, attachment_refs, reference_pool);
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
        vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics);

        make_node_subpass_input_attachments(subpass, node.get(), reference_pool, rep_index, attachment_refs);
        make_node_subpass_color_attachments(subpass, node.get(), reference_pool, rep_index, attachment_refs);
        make_node_subpass_depth_attachment(subpass, node.get(), reference_pool, rep_index, attachment_refs);

        subpasses.emplace_back(subpass);

        emit_node_subpass_dependencies(node.get(), dependencies, rep_index);
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

void renderer::generate_attachment_descriptions(std::vector<vk::AttachmentDescription>& attachments, std::map<framebuffer_ref, uint32_t>& attachment_refs) {
    attachments.emplace_back(vk::AttachmentDescriptionFlags(), //swapchain color attachment
            swpc->format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR
    );
    attachment_refs[1] = 0;

    for(const auto& fb : buffers) {
        if(!fb.second.in_use) continue;
        attachment_refs[fb.first] = (uint32_t)attachments.size();
        size_t num_layers = fb.second.num_layers(); //std::get<2>(fb.second).size() == 1 ? 1 : std::get<2>(fb.second).size()-1;
        for(size_t i = 0; i < num_layers; ++i) {
            attachments.emplace_back(vk::AttachmentDescriptionFlags(),
                    fb.second.img->info.format,
                    vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
                    vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
        }
    }
}

void renderer::generate_clear_values() {
    clear_values.clear();
    clear_values.emplace_back(vk::ClearColorValue(std::array<float,4>{0.f, 0.0f, 0.0f, 0.f}));
    for(const auto& fb : buffers) {
        if(!fb.second.in_use) continue;
        size_t num_layers = fb.second.num_layers();
        for(size_t i = 0; i < num_layers; ++i) {
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

    std::vector<vk::AttachmentDescription> attachments;
    std::map<framebuffer_ref, uint32_t> attachment_refs;
    generate_attachment_descriptions(attachments, attachment_refs);

    generate_clear_values();

    // collect all subpasses and generate subpass dependencies, one per node except output
    std::vector<vk::SubpassDescription> subpasses;
    std::vector<vk::SubpassDependency> dependencies;
    arena<vk::AttachmentReference> reference_pool;
    subpass_order.clear();
    generate_subpasses(screen_output_node, subpasses, dependencies, attachment_refs, reference_pool);

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

    // allocate descriptors sets and pools
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
        if(log_compile) std::cout << "initializing pipeline objects for " << node->id << ":" << node->prototype->name() << "\n";
        node->prototype->update_descriptor_sets(this, node.get(), desc_writes, buf_infos, img_infos);
        node->pipeline =
            node->prototype->generate_pipeline(this, node.get(), render_pass.get(), i);

        // generate command buffers
        node->subpass_commands = node->prototype->generate_command_buffer(this, node.get());
        if(log_compile) std::cout << "----------------------\n";
    }
    dev->dev->updateDescriptorSets(desc_writes, {});
    should_recompile = false;
}
