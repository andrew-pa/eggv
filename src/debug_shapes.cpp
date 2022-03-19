#include "debug_shapes.h"
#include "imgui.h"

std::vector<vec3> generate_box_frame_and_axis_vertices() {
  return {
      vec3(0.5f, 0.5f, 0.5f),    vec3(0.5f, -0.5f, 0.5f),
      vec3(-0.5f, -0.5f, 0.5f),  vec3(-0.5f, 0.5f, 0.5f),

      vec3(0.5f, 0.5f, -0.5f),   vec3(0.5f, -0.5f, -0.5f),
      vec3(-0.5f, -0.5f, -0.5f), vec3(-0.5f, 0.5f, -0.5f),

      vec3(.25f, 0.f, 0.f),      vec3(-.25f, 0.f, 0.f),
      vec3(.0f, .25f, 0.f),      vec3(.0f, -.25f, 0.f),
      vec3(.0f, .0f, .25f),      vec3(.0f, .0f, -.25f),
  };
}

std::vector<uint16> generate_box_frame_and_axis_indices() {
  return {
      0, 1, 1, 2, 2, 3, 3, 0,
      4, 5, 5, 6, 6, 7, 7, 4,
      0, 4, 1, 5, 2, 6, 3, 7,
      0, 1, 2, 3, 4, 5,
  };
}

debug_shape_render_node_prototype::debug_shape_render_node_prototype(device* dev)
    : frame_axis_mesh(dev, generate_box_frame_and_axis_vertices(), generate_box_frame_and_axis_indices())
{
    inputs = {
        framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color, framebuffer_mode::blend_input},
        // framebuffer_desc{"depth", vk::Format::eUndefined, framebuffer_type::depth}, // how do we indicate we want this bound as the depth buffer??????????
    };
    outputs = {
        framebuffer_desc{"color", vk::Format::eUndefined, framebuffer_type::color},
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

void debug_shape_render_node_prototype::collect_descriptor_layouts(render_node* node, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
        std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs)
{
    pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void debug_shape_render_node_prototype::update_descriptor_sets(renderer* r, render_node* node,
        std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos,
        arena<vk::DescriptorImageInfo>& img_infos)
{
    auto* b = buf_infos.alloc(vk::DescriptorBufferInfo(r->frame_uniforms_buf->buf, 0, sizeof(frame_uniforms)));
    writes.emplace_back(node->desc_set.get(), 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, b);
}

vk::UniquePipeline debug_shape_render_node_prototype::generate_pipeline(renderer* r, render_node* node,
        vk::RenderPass render_pass, uint32_t subpass)
{
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eVertex,
            r->dev->load_shader("simple.vert.spv"), "main"
        },
        vk::PipelineShaderStageCreateInfo {
            {}, vk::ShaderStageFlagBits::eFragment,
            r->dev->load_shader("solid-color.frag.spv"), "main"
        }
    };

    auto vertex_binding = vk::VertexInputBindingDescription { 0, sizeof(vec3), vk::VertexInputRate::eVertex };
    constexpr static vk::VertexInputAttributeDescription ds_vertex_attribute_description[] = {
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, 0 },
    };

    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo {
        {}, 1, &vertex_binding,
            1, ds_vertex_attribute_description
    };

    auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo {
        {}, vk::PrimitiveTopology::eLineList
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
        {}, false, true, vk::CompareOp::eLess, false, false
    };

    vk::PipelineColorBlendAttachmentState color_blend_att[] = {
        vk::PipelineColorBlendAttachmentState(false,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
                vk::ColorComponentFlagBits::eR|vk::ColorComponentFlagBits::eG
                    |vk::ColorComponentFlagBits::eB |vk::ColorComponentFlagBits::eA) 
    };
    auto color_blending_state = vk::PipelineColorBlendStateCreateInfo {
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

void debug_shape_render_node_prototype::generate_command_buffer_inline(renderer* r, render_node* node, vk::CommandBuffer& cb, size_t subpass_index) {
    vec3 global_scale = vec3(node->data == nullptr ? 1.f :
        ((node_data*)node->data.get())->global_scale);
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, node->pipeline.get());
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(),
            0, { node->desc_set.get() }, {});
    cb.bindVertexBuffers(0, {frame_axis_mesh.vertex_buffer->buf}, {0});
    cb.bindIndexBuffer(frame_axis_mesh.index_buffer->buf, 0, vk::IndexType::eUint16);
    for(const auto& shape: r->active_shapes) {
        cb.pushConstants<mat4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, { scale(shape.T, global_scale) });
        cb.pushConstants<vec4>(this->pipeline_layout.get(), vk::ShaderStageFlagBits::eFragment, sizeof(mat4), {shape.color});
        if(shape.type == viewport_shape_type::box) {
            cb.drawIndexed(24, 1, 0, 0, 0);
        } else if(shape.type == viewport_shape_type::axis) {
            cb.drawIndexed(6, 1, 24, 8, 0);
        }
    }
}

void debug_shape_render_node_prototype::build_gui(class renderer* r, struct render_node* node) {
    if(node->data == nullptr) {
        node->data = std::make_unique<node_data>();
    }
    float* global_scale = &((node_data*)node->data.get())->global_scale;
    ImGui::SetNextItemWidth(100.f);
    ImGui::DragFloat("Scale", global_scale, 0.01f, 0.f, 1000.f, "%.1f");
}

std::unique_ptr<render_node_data> debug_shape_render_node_prototype::deserialize_node_data(json data) {
    if(data == nullptr) return nullptr;
    return std::make_unique<node_data>(data["scale"]);
}

json debug_shape_render_node_prototype::node_data::serialize() const {
    return json{
        {"scale", this->global_scale}
    };
}
