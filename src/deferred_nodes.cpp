#include "deferred_nodes.h"
#include "glm/gtx/component_wise.hpp"
#include "scene_components.h"

// --- geometery buffer

gbuffer_geom_render_node_prototype::gbuffer_geom_render_node_prototype(device* dev, renderer* r) {
    inputs  = {};
    outputs = {
        framebuffer_desc{
                         "geometery", vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::output,
                         3},
        framebuffer_desc{
                         "depth",           vk::Format::eUndefined,                                      framebuffer_type::depth, framebuffer_mode::output}
    };

    desc_layout = dev->create_desc_set_layout({vk::DescriptorSetLayoutBinding(
        0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex
    )});

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex,   0,            sizeof(mat4)  },
        vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(uint32)}
    };

    vk::DescriptorSetLayout desc_layouts[] = {desc_layout.get(), r->material_desc_set_layout.get()};

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 2, desc_layouts, 2, push_consts});
}

void gbuffer_geom_render_node_prototype::build_gui(class renderer*, struct render_node* node) {}

void gbuffer_geom_render_node_prototype::collect_descriptor_layouts(
    render_node*                           node,
    std::vector<vk::DescriptorPoolSize>&   pool_sizes,
    std::vector<vk::DescriptorSetLayout>&  layouts,
    std::vector<vk::UniqueDescriptorSet*>& outputs
) {
    pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void gbuffer_geom_render_node_prototype::update_descriptor_sets(
    class renderer*                      r,
    struct render_node*                  node,
    std::vector<vk::WriteDescriptorSet>& writes,
    arena<vk::DescriptorBufferInfo>&     buf_infos,
    arena<vk::DescriptorImageInfo>&      img_infos
) {
    writes.emplace_back(
        node->desc_set.get(),
        0,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        buf_infos.alloc(vk::DescriptorBufferInfo(
            r->global_buffers[GLOBAL_BUF_FRAME_UNIFORMS]->buf, 0, sizeof(frame_uniforms)
        ))
    );
}

void gbuffer_geom_render_node_prototype::generate_pipelines(
    renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo{
                                          {}, vk::ShaderStageFlagBits::eVertex, r->dev->load_shader("full.vert.spv"), "main"},
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eFragment,
                                          r->dev->load_shader("gbuffer.frag.spv"),
                                          "main"                                                                            }
    };

    auto vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vertex), vk::VertexInputRate::eVertex};
    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &vertex_binding, 3, vertex_attribute_description};

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state
        = vk::PipelineViewportStateCreateInfo{{}, 1, &r->full_viewport, 1, &r->full_scissor};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eClockwise,
        false,
        0.f,
        0.f,
        0.f,
        1.f};

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, true, true, vk::CompareOp::eLess, false, false};

    vk::PipelineColorBlendAttachmentState color_blend_att[] = {
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
    };
    for(int i = 0; i < 3; ++i)
        color_blend_att[i].colorWriteMask
            = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
              | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    auto color_blending_state
        = vk::PipelineColorBlendStateCreateInfo{{}, false, vk::LogicOp::eCopy, 3, color_blend_att};

    auto cfo = vk::GraphicsPipelineCreateInfo(
        {},
        2,
        shader_stages,
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
        render_pass,
        subpass
    );

    this->create_pipeline(r, node, cfo);
}

void gbuffer_geom_render_node_prototype::generate_command_buffer_inline(
    renderer*           r,
    struct render_node* node,
    vk::CommandBuffer&  cb,
    size_t              subpass_index,
    const frame_state&  fs
) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline(node));
    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(), 0, {node->desc_set.get()}, {}
    );

    r->for_each_renderable([&](auto entity_id, auto mesh, auto transform) {
        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            this->pipeline_layout.get(),
            1,
            {mesh.mat->desc_set},
            {}
        );
        auto m = mesh.m;
        cb.bindVertexBuffers(0, {m->vertex_buffer->buf}, {0});
        cb.bindIndexBuffer(m->index_buffer->buf, 0, vk::IndexType::eUint16);
        cb.pushConstants<mat4>(
            this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, {transform.world}
        );
        cb.pushConstants<uint32>(
            this->pipeline_layout.get(),
            vk::ShaderStageFlagBits::eFragment,
            sizeof(mat4),
            {mesh.mat ? mesh.mat->_render_index : 0}
        );
        cb.drawIndexed(m->index_count, 1, 0, 0, 0);
    });
}

// --- directional light pass
directional_light_render_node_prototype::directional_light_render_node_prototype(device* dev) {
    inputs = {
        framebuffer_desc{
                         "input_color", vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::blend_input     },
        framebuffer_desc{
                         "geometry",    vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::input_attachment,
                         3},
        framebuffer_desc{
                         "shadowmap",             vk::Format::eUndefined,
                         framebuffer_type::depth,
                         framebuffer_mode::shader_input           },
    };
    outputs = {
        framebuffer_desc{
                         "color", vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::output},
    };

    desc_layout = dev->create_desc_set_layout(
        {vk::DescriptorSetLayoutBinding(
             0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment
         )}
    );

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange{
                              vk::ShaderStageFlagBits::eFragment, 0, sizeof(vec4) * 2 + sizeof(mat4)}
    };

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, &desc_layout.get(), 1, push_consts});
}

void directional_light_render_node_prototype::build_gui(class renderer*, struct render_node* node) {
}

void directional_light_render_node_prototype::collect_descriptor_layouts(
    render_node*                           node,
    std::vector<vk::DescriptorPoolSize>&   pool_sizes,
    std::vector<vk::DescriptorSetLayout>&  layouts,
    std::vector<vk::UniqueDescriptorSet*>& outputs
) {
    pool_sizes.emplace_back(vk::DescriptorType::eInputAttachment, 3);
    pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 1);
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void directional_light_render_node_prototype::update_descriptor_sets(
    renderer*                            r,
    render_node*                         node,
    std::vector<vk::WriteDescriptorSet>& writes,
    arena<vk::DescriptorBufferInfo>&     buf_infos,
    arena<vk::DescriptorImageInfo>&      img_infos
) {
    for(int i = 0; i < 3; ++i) {
        writes.emplace_back(
            node->desc_set.get(),
            i,
            0,
            1,
            vk::DescriptorType::eInputAttachment,
            img_infos.alloc(vk::DescriptorImageInfo(
                nullptr,
                r->buffers[node->input_framebuffer(1).value()].image_views[1 + i].get(),
                vk::ImageLayout::eShaderReadOnlyOptimal
            ))
        );
    }

    if(node->input_framebuffer(2).has_value()) {
        writes.emplace_back(
            node->desc_set.get(),
            5,
            0,
            1,
            vk::DescriptorType::eCombinedImageSampler,
            img_infos.alloc(vk::DescriptorImageInfo(
                r->texture_sampler.get(),
                r->buffers[node->input_framebuffer(2).value()].image_views[0].get(),
                vk::ImageLayout::eShaderReadOnlyOptimal
            ))
        );
    }

    writes.emplace_back(
        node->desc_set.get(),
        3,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        buf_infos.alloc(vk::DescriptorBufferInfo(
            r->global_buffers[GLOBAL_BUF_FRAME_UNIFORMS]->buf, 0, sizeof(frame_uniforms)
        ))
    );
    if(r->global_buffers[GLOBAL_BUF_MATERIALS])
        writes.emplace_back(
            node->desc_set.get(),
            4,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            buf_infos.alloc(vk::DescriptorBufferInfo(
                r->global_buffers[GLOBAL_BUF_MATERIALS]->buf,
                0,
                r->num_gpu_mats * sizeof(gpu_material)
            ))
        );
}

void directional_light_render_node_prototype::generate_pipelines(
    renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eVertex,
                                          r->dev->load_shader("entire-screen.vert.spv"),
                                          "main"},
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eFragment,
                                          r->dev->load_shader("directional-light.frag.spv"),
                                          "main"}
    };

    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{};

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state
        = vk::PipelineViewportStateCreateInfo{{}, 1, &r->full_viewport, 1, &r->full_scissor};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone,
        vk::FrontFace::eCounterClockwise,
        false,
        0.f,
        0.f,
        0.f,
        1.f};

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, true, true, vk::CompareOp::eLess, false, false};

    vk::PipelineColorBlendAttachmentState color_blend_att[]
        = {vk::PipelineColorBlendAttachmentState(
            true,
            vk::BlendFactor::eSrcAlpha,
            vk::BlendFactor::eSrcAlpha,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eOne,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        )};
    auto color_blending_state
        = vk::PipelineColorBlendStateCreateInfo{{}, false, vk::LogicOp::eCopy, 1, color_blend_att};

    auto cfo = vk::GraphicsPipelineCreateInfo(
        {},
        2,
        shader_stages,
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
        render_pass,
        subpass
    );

    this->create_pipeline(r, node, cfo);

    shadowmap_node_proto = nullptr;
    for(const auto& proto : r->prototypes) {
        if(proto->id() == 0x00010003) {  // directional light shadow map
            shadowmap_node_proto
                = std::dynamic_pointer_cast<directional_light_shadowmap_render_node_prototype>(proto
                );
        }
    }
}

void directional_light_render_node_prototype::generate_command_buffer_inline(
    renderer*           r,
    struct render_node* node,
    vk::CommandBuffer&  cb,
    size_t              subpass_index,
    const frame_state&  fs
) {
    /*if(node->input_framebuffer(2).has_value()) {
        cb.pipelineBarrier(vk::PipelineStageFlagBits::eLateFragmentTests,
    vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlagBits::eByRegion, {}, {}, {
                vk::ImageMemoryBarrier(
                    vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits::eShaderRead,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    r->buffers[node->input_framebuffer(2).value()].img->img,
                    vk::ImageSubresourceRange{}
                )
        });
    }*/

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline(node));
    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(), 0, {node->desc_set.get()}, {}
    );
    bool has_shadowmap = false;
    mat4 light_proj, inverse_view;
    if(shadowmap_node_proto != nullptr) {
        // TODO: could we just reuse the uniform buffer from rendering the shadowmaps instead of
        // passing this matrix as a push constant?
        has_shadowmap      = true;
        float scene_radius = shadowmap_node_proto->scene_radius;
        light_proj         = mat4(
                         0.5f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.5f,
                         0.0f,
                         0.0f,
                         0.0f,
                         0.0f,
                         1.0f,
                         0.0f,
                         0.5f,
                         0.5f,
                         0.0f,
                         1.0f
                     )  // convert from ndc to texture coords
                     * glm::ortho(
                         -scene_radius,
                         scene_radius,
                         -scene_radius,
                         scene_radius,
                         -scene_radius,
                         scene_radius
                     );
        inverse_view = glm::inverse(r->mapped_frame_uniforms->view);
    }

    auto* cur_world = r->current_world();
    auto  lights    = cur_world->system<light_system>();

    for(auto lighti = lights->begin_components(); lighti != lights->end_components(); ++lighti) {
        const auto& [id, light] = *lighti;
        if(light.type != light_type::directional) continue;
        int light_index = -1;
        if(has_shadowmap) light_index = light._render_index;
        cb.pushConstants<vec4>(
            this->pipeline_layout.get(),
            vk::ShaderStageFlagBits::eFragment,
            0,
            {r->mapped_frame_uniforms->view * vec4(light.param, 0.f), vec4(light.color, 0.f)}
        );
        cb.pushConstants<int>(
            this->pipeline_layout.get(),
            vk::ShaderStageFlagBits::eFragment,
            sizeof(vec4) + sizeof(vec3),
            {light_index}
        );
        if(has_shadowmap) {
            mat4 light_viewproj = light_proj
                                  * glm::lookAt(-light.param, vec3(0.f), vec3(0.f, -1.f, 0.f))
                                  * inverse_view;
            cb.pushConstants<mat4>(
                this->pipeline_layout.get(),
                vk::ShaderStageFlagBits::eFragment,
                sizeof(vec4) * 2,
                {light_viewproj}
            );
        }
        cb.draw(3, 1, 0, 0);
    }
}

directional_light_shadowmap_render_node_prototype::
    directional_light_shadowmap_render_node_prototype(device* dev)
    : scene_radius(11.f) {
    inputs  = {};
    outputs = {
        framebuffer_desc{
                         "depth", vk::Format::eUndefined,
                         framebuffer_type::depth,
                         framebuffer_mode::output,
                         framebuffer_count_is_subpass_count, framebuffer_subpass_binding_order::sequential}
    };

    desc_layout = dev->create_desc_set_layout({vk::DescriptorSetLayoutBinding(
        0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eVertex
    )});

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(mat4) + sizeof(uint32)},
    };

    vk::DescriptorSetLayout desc_layouts[] = {desc_layout.get()};

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, desc_layouts, 1, push_consts});
}

struct dir_light_shadowmap_node_data : public render_node_data {
    std::vector<vk::UniquePipeline> pipelines;
    float*                          scene_radius;

    dir_light_shadowmap_node_data(float* scene_radius) : scene_radius(scene_radius) {}

    json serialize() const override {
        return json{
            {"scene_radius", *this->scene_radius}
        };
    }
};

std::unique_ptr<render_node_data> directional_light_shadowmap_render_node_prototype::
    initialize_node_data() {
    return std::make_unique<dir_light_shadowmap_node_data>(&this->scene_radius);
}

std::unique_ptr<render_node_data> directional_light_shadowmap_render_node_prototype::
    deserialize_node_data(const json& data) {
    if(data.contains("scene_radius")) this->scene_radius = data["scene_radius"];
    return initialize_node_data();
}

size_t directional_light_shadowmap_render_node_prototype::subpass_repeat_count(
    renderer* r, render_node* n
) {
    size_t num_lights = 0;
    pass_to_light_map.clear();

    auto* cur_world = r->current_world();
    auto  lights    = cur_world->system<light_system>();

    for(auto lighti = lights->begin_components(); lighti != lights->end_components(); ++lighti) {
        auto& [id, light] = *lighti;
        if(light.type == light_type::directional) {
            pass_to_light_map[num_lights] = id;
            light._render_index           = num_lights;
            num_lights++;
        }
    }

    this->outputs[0].count                                   = num_lights;
    r->global_buffers[GLOBAL_BUF_DIRECTIONAL_LIGHT_VIEWPROJ] = std::make_unique<buffer>(
        r->dev,
        sizeof(mat4) * num_lights,
        vk::BufferUsageFlagBits::eStorageBuffer,
        vk::MemoryPropertyFlagBits::eHostCoherent,
        (void**)&mapped_light_viewprojs
    );

    return num_lights;
}

#include "imgui.h"

void directional_light_shadowmap_render_node_prototype::build_gui(
    class renderer* r, struct render_node* node
) {
    ImGui::SetNextItemWidth(150.f);
    ImGui::DragFloat("Scene Radius", &this->scene_radius, 1.f, 1.f, 0.f);
}

void directional_light_shadowmap_render_node_prototype::collect_descriptor_layouts(
    render_node*                           node,
    std::vector<vk::DescriptorPoolSize>&   pool_sizes,
    std::vector<vk::DescriptorSetLayout>&  layouts,
    std::vector<vk::UniqueDescriptorSet*>& outputs
) {
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void directional_light_shadowmap_render_node_prototype::update_descriptor_sets(
    renderer*                            r,
    render_node*                         node,
    std::vector<vk::WriteDescriptorSet>& writes,
    arena<vk::DescriptorBufferInfo>&     buf_infos,
    arena<vk::DescriptorImageInfo>&      img_infos
) {
    writes.emplace_back(
        node->desc_set.get(),
        0,
        0,
        1,
        vk::DescriptorType::eStorageBuffer,
        nullptr,
        buf_infos.alloc(vk::DescriptorBufferInfo(
            r->global_buffers[GLOBAL_BUF_DIRECTIONAL_LIGHT_VIEWPROJ]->buf,
            0,
            sizeof(mat4) * node->subpass_count
        ))
    );
}

void directional_light_shadowmap_render_node_prototype::generate_pipelines(
    renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eVertex,
                                          r->dev->load_shader("multiview-simple.vert.spv"),
                                          "main"                                                                             },
        vk::PipelineShaderStageCreateInfo{
                                          {}, vk::ShaderStageFlagBits::eFragment, r->dev->load_shader("nop.frag.spv"), "main"}
    };

    auto vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vertex), vk::VertexInputRate::eVertex};
    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &vertex_binding, 3, vertex_attribute_description};

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state
        = vk::PipelineViewportStateCreateInfo{{}, 1, &r->full_viewport, 1, &r->full_scissor};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eClockwise,
        VK_TRUE,
        1.25f,
        0.0f,
        1.75f,
        1.f};

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, true, true, vk::CompareOp::eLess, false, false};

    vk::PipelineColorBlendAttachmentState color_blend_att[] = {
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
        vk::PipelineColorBlendAttachmentState{},
    };
    for(int i = 0; i < 3; ++i)
        color_blend_att[i].colorWriteMask
            = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
              | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    auto color_blending_state
        = vk::PipelineColorBlendStateCreateInfo{{}, false, vk::LogicOp::eCopy, 3, color_blend_att};

    auto cfo = vk::GraphicsPipelineCreateInfo(
        {},
        2,
        shader_stages,
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
        render_pass
    );

    auto* data = (dir_light_shadowmap_node_data*)node->data.get();
    data->pipelines.clear();
    for(uint32_t i = 0; i < node->subpass_count; ++i) {
        cfo.subpass = subpass + i;
        data->pipelines.emplace_back(r->dev->dev->createGraphicsPipelineUnique(nullptr, cfo));
    }
}

void directional_light_shadowmap_render_node_prototype::generate_command_buffer_inline(
    renderer*           r,
    struct render_node* node,
    vk::CommandBuffer&  cb,
    size_t              subpass_index,
    const frame_state&  fs
) {

    /*auto lt = current_light->traits.find(TRAIT_ID_LIGHT);
    if(lt != current_light->traits.end()) {
        if(((light_trait*)(lt->second.get()))->type == light_type::directional) {
            light = (light_trait*)lt->second.get();
        }
    }*/
    auto*        cur_world = r->current_world();
    auto         light_ent = cur_world->entity(pass_to_light_map[subpass_index]);
    const light& light     = light_ent.get_component<light_system>();

    // const float scene_radius = 8.f;
    mat4 light_proj = glm::ortho(
        -scene_radius, scene_radius, -scene_radius, scene_radius, -scene_radius, scene_radius
    );
    mapped_light_viewprojs[subpass_index]
        = light_proj * glm::lookAt(-light.param, vec3(0.f), vec3(0.f, -1.f, 0.f));

    auto* data = (dir_light_shadowmap_node_data*)node->data.get();
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, data->pipelines[subpass_index].get());
    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(), 0, {node->desc_set.get()}, {}
    );
    cb.pushConstants<uint32>(
        this->pipeline_layout.get(),
        vk::ShaderStageFlagBits::eVertex,
        sizeof(mat4),
        {(uint32)subpass_index}
    );
    r->for_each_renderable([&](auto entity_id, auto mesh, auto transform) {
        auto m = mesh.m;
        cb.bindVertexBuffers(0, {m->vertex_buffer->buf}, {0});
        cb.bindIndexBuffer(m->index_buffer->buf, 0, vk::IndexType::eUint16);
        cb.pushConstants<mat4>(
            this->pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex, 0, {transform.world}
        );
        cb.drawIndexed(m->index_count, 1, 0, 0, 0);
    });
}

// --- point light pass
#include "mesh_gen.h"

point_light_render_node_prototype::point_light_render_node_prototype(device* dev) {
    inputs = {
        framebuffer_desc{
                         "input_color",                    vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::blend_input},
        framebuffer_desc{
                         "geometry", vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::input_attachment,
                         3},
    };
    outputs = {
        framebuffer_desc{
                         "color", vk::Format::eR32G32B32A32Sfloat,
                         framebuffer_type::color,
                         framebuffer_mode::output},
    };

    desc_layout = dev->create_desc_set_layout(
        {vk::DescriptorSetLayoutBinding(
             0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment
         ),
         vk::DescriptorSetLayoutBinding(
             3,
             vk::DescriptorType::eUniformBuffer,
             1,
             vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex
         ),
         vk::DescriptorSetLayoutBinding(
             4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment
         )}
    );

    vk::PushConstantRange push_consts[] = {
        vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex,   0,            sizeof(mat4)    },
        vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, sizeof(mat4), sizeof(vec4) * 3}
    };

    pipeline_layout = dev->dev->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
        {}, 1, &desc_layout.get(), 2, push_consts});

    sphere_mesh = std::make_unique<mesh>(mesh_gen::generate_sphere(dev, 16, 16));
}

void point_light_render_node_prototype::build_gui(class renderer*, struct render_node* node) {}

void point_light_render_node_prototype::collect_descriptor_layouts(
    render_node*                           node,
    std::vector<vk::DescriptorPoolSize>&   pool_sizes,
    std::vector<vk::DescriptorSetLayout>&  layouts,
    std::vector<vk::UniqueDescriptorSet*>& outputs
) {
    pool_sizes.emplace_back(vk::DescriptorType::eInputAttachment, 3);
    pool_sizes.emplace_back(vk::DescriptorType::eUniformBuffer, 1);
    pool_sizes.emplace_back(vk::DescriptorType::eStorageBuffer, 1);
    layouts.push_back(desc_layout.get());
    outputs.push_back(&node->desc_set);
}

void point_light_render_node_prototype::update_descriptor_sets(
    class renderer*                      r,
    struct render_node*                  node,
    std::vector<vk::WriteDescriptorSet>& writes,
    arena<vk::DescriptorBufferInfo>&     buf_infos,
    arena<vk::DescriptorImageInfo>&      img_infos
) {
    for(int i = 0; i < 3; ++i) {
        writes.emplace_back(
            node->desc_set.get(),
            i,
            0,
            1,
            vk::DescriptorType::eInputAttachment,
            img_infos.alloc(vk::DescriptorImageInfo(
                nullptr,
                r->buffers[node->input_framebuffer(1).value()].image_views[i + 1].get(),
                vk::ImageLayout::eShaderReadOnlyOptimal
            ))
        );
    }

    writes.emplace_back(
        node->desc_set.get(),
        3,
        0,
        1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        buf_infos.alloc(vk::DescriptorBufferInfo(
            r->global_buffers[GLOBAL_BUF_FRAME_UNIFORMS]->buf, 0, sizeof(frame_uniforms)
        ))
    );
    if(r->global_buffers[GLOBAL_BUF_MATERIALS])
        writes.emplace_back(
            node->desc_set.get(),
            4,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            buf_infos.alloc(vk::DescriptorBufferInfo(
                r->global_buffers[GLOBAL_BUF_MATERIALS]->buf,
                0,
                r->num_gpu_mats * sizeof(gpu_material)
            ))
        );
}

void point_light_render_node_prototype::generate_pipelines(
    renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
) {
    vk::PipelineShaderStageCreateInfo shader_stages[] = {
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eVertex,
                                          r->dev->load_shader("point-light.vert.spv"),
                                          "main"},
        vk::PipelineShaderStageCreateInfo{
                                          {},
                                          vk::ShaderStageFlagBits::eFragment,
                                          r->dev->load_shader("point-light.frag.spv"),
                                          "main"}
    };

    auto vertex_binding
        = vk::VertexInputBindingDescription{0, sizeof(vertex), vk::VertexInputRate::eVertex};
    auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{
        {}, 1, &vertex_binding, 3, vertex_attribute_description};

    auto input_assembly
        = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList};

    auto viewport_state
        = vk::PipelineViewportStateCreateInfo{{}, 1, &r->full_viewport, 1, &r->full_scissor};

    auto rasterizer_state = vk::PipelineRasterizationStateCreateInfo{
        {},
        false,
        false,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eFront,
        vk::FrontFace::eClockwise,
        false,
        0.f,
        0.f,
        0.f,
        1.f};

    auto multisample_state = vk::PipelineMultisampleStateCreateInfo{};

    auto depth_stencil_state = vk::PipelineDepthStencilStateCreateInfo{
        {}, true, true, vk::CompareOp::eLess, false, false};

    vk::PipelineColorBlendAttachmentState color_blend_att[]
        = {vk::PipelineColorBlendAttachmentState(
            true,
            vk::BlendFactor::eSrcAlpha,
            vk::BlendFactor::eSrcAlpha,
            vk::BlendOp::eAdd,
            vk::BlendFactor::eOne,
            vk::BlendFactor::eOne,
            vk::BlendOp::eAdd,
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        )};
    auto color_blending_state
        = vk::PipelineColorBlendStateCreateInfo{{}, false, vk::LogicOp::eCopy, 1, color_blend_att};

    auto cfo = vk::GraphicsPipelineCreateInfo(
        {},
        2,
        shader_stages,
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
        render_pass,
        subpass
    );

    this->create_pipeline(r, node, cfo);
}

float light_radius(const light& light) {
    float i = compMax(light.color);
    return sqrt(-5.f + 256.f * i) / sqrt(5.f * light.param.x);
}

void point_light_render_node_prototype::generate_command_buffer_inline(
    renderer*           r,
    struct render_node* node,
    vk::CommandBuffer&  cb,
    size_t              subpass_index,
    const frame_state&  fs
) {
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline(node));
    cb.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, this->pipeline_layout.get(), 0, {node->desc_set.get()}, {}
    );
    cb.bindVertexBuffers(0, {sphere_mesh->vertex_buffer->buf}, {0});
    cb.bindIndexBuffer(sphere_mesh->index_buffer->buf, 0, vk::IndexType::eUint16);

    auto* cur_world  = r->current_world();
    auto  lights     = cur_world->system<light_system>();
    auto  transforms = cur_world->system<transform_system>();

    for(auto lighti = lights->begin_components(); lighti != lights->end_components(); ++lighti) {
        auto& [id, light] = *lighti;
        if(light.type != light_type::point) continue;
        const auto& T              = transforms->get_data_for_entity(id).world;
        vec4        light_view_pos = r->mapped_frame_uniforms->view * T * vec4(0.f, 0.f, 0.f, 1.f);
        cb.pushConstants<mat4>(
            this->pipeline_layout.get(),
            vk::ShaderStageFlagBits::eVertex,
            0,
            {scale(T, vec3(light_radius(light)))}
        );
        cb.pushConstants<vec4>(
            this->pipeline_layout.get(),
            vk::ShaderStageFlagBits::eFragment,
            sizeof(mat4),
            {vec4(light.param, 0.f), vec4(light.color, 0.f), light_view_pos}
        );
        cb.drawIndexed(sphere_mesh->index_count, 1, 0, 0, 0);
    }
}
