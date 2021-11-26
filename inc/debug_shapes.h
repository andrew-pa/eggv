#pragma once
#include "cmmn.h"
#include "renderer.h"


struct debug_shape_render_node_prototype : public render_node_prototype {
    mesh frame_axis_mesh;

    debug_shape_render_node_prototype(device* dev);

    void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) override;
    void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos) override;
    vk::UniquePipeline generate_pipeline(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass);
    void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&) override;
    virtual void build_gui(class renderer*, struct render_node* node) {}

    virtual std::unique_ptr<render_node_data> deserialize_node_data(json data) { return nullptr; }
    const char* name() const override { return "Viewport Shapes"; }
    size_t id() const override { return 0x0000fffc; }

    ~debug_shape_render_node_prototype() {
//        std::cout << "goodbye debug shapes\n";
    }
};
