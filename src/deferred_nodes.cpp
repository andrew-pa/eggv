#include "deferred_nodes.h"

gbuffer_geom_render_node_prototype::gbuffer_geom_render_node_prototype(device* dev) {
    inputs = {};
    outputs = {
        framebuffer_desc{"position", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"normal", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"texture_material", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth}
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

void gbuffer_geom_render_node_prototype::build_gui(class renderer *, struct render_node *node) {
}

void gbuffer_geom_render_node_prototype::collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
        std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) 
{
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1));
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void gbuffer_geom_render_node_prototype::update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, std::vector<vk::DescriptorBufferInfo>& buf_infos, std::vector<vk::DescriptorImageInfo>& img_infos) 
{
    buf_infos.push_back(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)));
    writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, &buf_infos[buf_infos.size()-1]));
}

vk::UniquePipeline gbuffer_geom_render_node_prototype::generate_pipeline(renderer* r, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eVertex,
            r->dev->load_shader("full.vert.spv"), "main"
        },
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eFragment,
            r->dev->load_shader("gbuffer.frag.spv"), "main"
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
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
    };
    for(int i = 0; i < 3; ++i) color_blend_att[i].colorWriteMask = vk::ColorComponentFlagBits::eR
        |vk::ColorComponentFlagBits::eG |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA;
    auto color_blending_state = vk::PipelineColorBlendStateCreateInfo{
        {}, false, vk::LogicOp::eCopy, 3, color_blend_att
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

void gbuffer_geom_render_node_prototype::generate_command_buffer_inline(renderer* r, struct render_node* node, vk::CommandBuffer& cb) {
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
