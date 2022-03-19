#pragma once
#include "renderer.h"
#include "imgui.h"

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
