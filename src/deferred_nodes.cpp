#include "deferred_nodes.h"
#include "glm/gtx/component_wise.hpp"

// --- geometery buffer

gbuffer_geom_render_node_prototype::gbuffer_geom_render_node_prototype(device* dev, renderer* r) {
    inputs = {};
    outputs = {
        framebuffer_desc{"position", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::output},
        framebuffer_desc{"normal", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::output},
        framebuffer_desc{"texture_material", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::output},
        framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth, framebuffer_mode::output}
    };

    desc_layout = dev->create_desc_set_layout({
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
    });

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) },
        vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(uint32) }
    };

	vk::DescriptorSetLayout desc_layouts[] = {
		desc_layout.get(),
		r->material_desc_set_layout.get()
	};

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
        {}, 2, desc_layouts, 2, push_consts
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

void gbuffer_geom_render_node_prototype::update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos)
{
    writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)))));
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
        {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise, false, 0.f, 0.f, 0.f, 1.f
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
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
            1, { mesh->mat->desc_set }, {});
        auto m = mesh->m;
        cb.bindVertexBuffers(0, {m->vertex_buffer->buf}, {0});
        cb.bindIndexBuffer(m->index_buffer->buf, 0, vk::IndexType::eUint16);
        cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { world_transform });
        cb.pushConstants<uint32>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4),
                { mesh->mat ? mesh->mat->_render_index : 0 });
        cb.drawIndexed(m->index_count, 1, 0, 0, 0);
    }
}


// --- directional light pass
directional_light_render_node_prototype::directional_light_render_node_prototype(device* dev) {
    inputs = {
        framebuffer_desc{"input_color", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::blend_input},
        framebuffer_desc{"position", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"normal", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"texture_material", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
    };
    outputs = {
        framebuffer_desc{"color", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::output},
    };

    desc_layout = dev->create_desc_set_layout({
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
    });

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, 0, sizeof(vec4)*2 }
    };

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
        {}, 1, &desc_layout.get(), 1, push_consts
    });
}

void directional_light_render_node_prototype::build_gui(class renderer *, struct render_node *node) {
}

void directional_light_render_node_prototype::collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
        std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) 
{
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 3));
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2));
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void directional_light_render_node_prototype::update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos)
{
    for(int i = 0; i < 3; ++i) {
        writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), i, 0, 1, vk::DescriptorType::eInputAttachment,
                    img_infos.alloc(vk::DescriptorImageInfo(nullptr,
                            std::get<2>(r->buffers[node->input_framebuffer(i + 1).value()]).get(),
                            vk::ImageLayout::eShaderReadOnlyOptimal))));
    }

    writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 3, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)))));
    if(r->materials_buf) writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 4, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, buf_infos.alloc(vk::DescriptorBufferInfo(r->materials_buf->buf, 0, r->num_gpu_mats*sizeof(gpu_material)))));
}

vk::UniquePipeline directional_light_render_node_prototype::generate_pipeline(renderer* r, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eVertex,
            r->dev->load_shader("entire-screen.vert.spv"), "main"
        },
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eFragment,
            r->dev->load_shader("directional-light.frag.spv"), "main"
        }
    };

    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo { };

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
        vk::PipelineColorBlendAttachmentState(true,
                vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eSrcAlpha, vk::BlendOp::eAdd,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR|vk::ColorComponentFlagBits::eG
                    |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA) 
    };
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

void directional_light_render_node_prototype::generate_command_buffer_inline(renderer* r, struct render_node* node, vk::CommandBuffer& cb) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
            0, { node->desc_set.get() }, {});
    /* std::cout << r->active_lights.size() << "\n"; */
    for(const auto& [light, T] : r->active_lights) {
        if(light->type != light_type::directional) continue;
        cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment,
                0, { vec4(light->param,0.f), vec4(light->color,0.f) });
        cb.draw(3, 1, 0, 0);
    }
}

// --- point light pass
#include "mesh_gen.h"
point_light_render_node_prototype::point_light_render_node_prototype(device* dev) {
    inputs = {
        framebuffer_desc{"input_color", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::blend_input},
        framebuffer_desc{"position", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"normal", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
        framebuffer_desc{"texture_material", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color},
    };
    outputs = {
        framebuffer_desc{"color", vk::Format::eR32G32B32A32Sfloat, framebuffer_type::color, framebuffer_mode::output},
    };

    desc_layout = dev->create_desc_set_layout({
        vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
        vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex),
        vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)
    });

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange { vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) },
        vk::PushConstantRange { vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(vec4)*3 }
    };

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
        {}, 1, &desc_layout.get(), 2, push_consts
    });

    sphere_mesh = std::make_unique<mesh>(mesh_gen::generate_sphere(dev, 16, 16));
}

void point_light_render_node_prototype::build_gui(class renderer *, struct render_node *node) {
}

void point_light_render_node_prototype::collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
        std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) 
{
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 3));
    pool_sizes.push_back(vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2));
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void point_light_render_node_prototype::update_descriptor_sets(class renderer* r, struct render_node* node, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos)
{
    for(int i = 0; i < 3; ++i) {
        writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), i, 0, 1, vk::DescriptorType::eInputAttachment,
                    img_infos.alloc(vk::DescriptorImageInfo(nullptr,
                            std::get<2>(r->buffers[node->input_framebuffer(i + 1).value()]).get(),
                            vk::ImageLayout::eShaderReadOnlyOptimal))));
    }

    writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 3, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)))));
    if(r->materials_buf) writes.push_back(vk::WriteDescriptorSet(node->desc_set.get(), 4, 0, 1, vk::DescriptorType::eUniformBuffer,
                nullptr, buf_infos.alloc(vk::DescriptorBufferInfo(r->materials_buf->buf, 0, r->num_gpu_mats*sizeof(gpu_material)))));
}

vk::UniquePipeline point_light_render_node_prototype::generate_pipeline(renderer* r, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eVertex,
            r->dev->load_shader("point-light.vert.spv"), "main"
        },
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eFragment,
            r->dev->load_shader("point-light.frag.spv"), "main"
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
        {}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eFront,
            vk::FrontFace::eClockwise, false, 0.f, 0.f, 0.f, 1.f
    };

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, true, true, vk::CompareOp::eLess, false, false
    };

    vk::PipelineColorBlendAttachmentState color_blend_att[] = {
        vk::PipelineColorBlendAttachmentState(true,
                vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eSrcAlpha, vk::BlendOp::eAdd,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR|vk::ColorComponentFlagBits::eG
                    |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA) 
    };
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

float light_radius(light_trait* light) {
    float i = compMax(light->color);
    return sqrt(-5.f + 256.f * i) / sqrt(5.f * light->param.x);
}

void point_light_render_node_prototype::generate_command_buffer_inline(renderer* r, struct render_node* node, vk::CommandBuffer& cb) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
            0, { node->desc_set.get() }, {});
    cb.bindVertexBuffers(0, { sphere_mesh->vertex_buffer->buf }, {0});
    cb.bindIndexBuffer(sphere_mesh->index_buffer->buf, 0, vk::IndexType::eUint16);

    /* std::cout << r->active_lights.size() << "\n"; */
    for(const auto& [light, T] : r->active_lights) {
        if(light->type != light_type::point) continue;
        vec4 light_view_pos = r->mapped_frame_uniforms->view * T * vec4(0.f, 0.f, 0.f, 1.f);
        cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { scale(T, vec3(light_radius(light))) });
        cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment,
                sizeof(mat4), { vec4(light->param,0.f), vec4(light->color,0.f), light_view_pos });
        cb.drawIndexed(sphere_mesh->index_count, 1, 0, 0, 0);
    }
}
