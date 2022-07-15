#pragma once
#include "renderer.h"

struct gbuffer_geom_render_node_prototype : public single_pipeline_render_node_prototype {
    gbuffer_geom_render_node_prototype(device* dev, renderer*);

    size_t id() const override { return 0x00010000; }

    const char* name() const override { return "Geometry Buffer"; };

    void collect_descriptor_layouts(
        render_node*                           node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) override;

    void update_descriptor_sets(
        renderer*                            r,
        render_node*                         node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) override;

    void generate_pipelines(
        renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
    ) override;

    void generate_command_buffer_inline(
        renderer*          r,
        render_node*       node,
        vk::CommandBuffer& cb,
        size_t             subpass_index,
        const frame_state& fs
    ) override;

    void build_gui(renderer* r, render_node* node) override;
};

class directional_light_render_node_prototype : public single_pipeline_render_node_prototype {
    std::shared_ptr<class directional_light_shadowmap_render_node_prototype> shadowmap_node_proto;

  public:
    directional_light_render_node_prototype(device* dev);

    size_t id() const override { return 0x00010001; }

    const char* name() const override { return "Directional Light"; };

    void collect_descriptor_layouts(
        render_node*                           node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) override;
    void update_descriptor_sets(
        renderer*                            r,
        render_node*                         node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) override;
    void generate_pipelines(
        renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
    ) override;
    void generate_command_buffer_inline(
        renderer*          r,
        render_node*       node,
        vk::CommandBuffer& cb,
        size_t             subpass_index,
        const frame_state& fs
    ) override;

    void build_gui(renderer* r, struct render_node* node) override;
};

const size_t GLOBAL_BUF_DIRECTIONAL_LIGHT_VIEWPROJ = 3;

class directional_light_shadowmap_render_node_prototype : public render_node_prototype {
    // std::unique_ptr<buffer> light_viewproj_buffer;
    mat4*                       mapped_light_viewprojs;
    std::map<size_t, entity_id> pass_to_light_map;

  public:
    float scene_radius;

    directional_light_shadowmap_render_node_prototype(device* dev);

    size_t id() const override { return 0x00010003; }

    const char* name() const override { return "Directional Light Shadowmap"; };

    std::unique_ptr<render_node_data> initialize_node_data() override;

    void collect_descriptor_layouts(
        render_node*                           node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) override;

    void update_descriptor_sets(
        renderer*                            r,
        render_node*                         node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) override;

    void generate_pipelines(
        renderer* r, render_node* n, vk::RenderPass render_pass, uint32_t subpass
    ) override;
    void generate_command_buffer_inline(
        renderer*          r,
        render_node*       n,
        vk::CommandBuffer& cb,
        size_t             subpass_index,
        const frame_state& fs
    ) override;

    size_t subpass_repeat_count(class renderer* r, struct render_node* n) override;

    void build_gui(class renderer* r, struct render_node* node) override;
    std::unique_ptr<render_node_data> deserialize_node_data(const json& data) override;
};

struct point_light_render_node_prototype : public single_pipeline_render_node_prototype {
    std::unique_ptr<mesh> sphere_mesh;
    point_light_render_node_prototype(device* dev);

    size_t id() const override { return 0x00010002; }

    const char* name() const override { return "Point Light"; };

    void collect_descriptor_layouts(
        render_node*                           node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) override;
    void update_descriptor_sets(
        renderer*                            r,
        render_node*                         node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) override;
    void generate_pipelines(
        renderer* r, render_node* node, vk::RenderPass render_pass, uint32_t subpass
    ) override;
    void generate_command_buffer_inline(
        renderer*          r,
        render_node*       node,
        vk::CommandBuffer& cb,
        size_t             subpass_index,
        const frame_state& fs
    ) override;

    void build_gui(renderer* r, render_node* node) override;
};
