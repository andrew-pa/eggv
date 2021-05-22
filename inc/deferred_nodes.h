#pragma once
#include "renderer.h"

struct gbuffer_geom_render_node_prototype : public render_node_prototype {
    gbuffer_geom_render_node_prototype(device* dev);
    const char* name() const override { return "Geometry Buffer"; };

    virtual void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) override; 
    virtual void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes,
            std::vector<vk::DescriptorBufferInfo>& buf_infos, std::vector<vk::DescriptorImageInfo>& img_infos) override;
    virtual vk::UniquePipeline generate_pipeline(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) override;
    virtual void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&) override;

    virtual void build_gui(class renderer*, struct render_node* node) override;
};
