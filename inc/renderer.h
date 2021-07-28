#pragma once
#include "cmmn.h"
#include "app.h"
#include "swap_chain.h"
#include "scene_graph.h"
#include "mem_arena.h"
#include "mesh.h"

typedef size_t framebuffer_ref;

enum class framebuffer_type {
    color, depth, depth_stencil
};

enum class framebuffer_mode {
    shader_input, blend_input, output
};

struct framebuffer_desc {
    std::string name;
    vk::Format format;
    framebuffer_type type;
    framebuffer_mode mode;
    framebuffer_desc(std::string name, vk::Format fmt, framebuffer_type ty, framebuffer_mode mode = framebuffer_mode::shader_input)
        : name(name), format(fmt), type(ty), mode(mode) {}
};

struct render_node_data {
    virtual json serialize() const = 0;
    virtual ~render_node_data() {}
};

struct render_node_prototype {
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    std::vector<framebuffer_desc> inputs, outputs;

    virtual void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) {}
    virtual void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos) {}
    virtual vk::UniquePipeline generate_pipeline(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) = 0;
    virtual void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&) {}
    virtual std::optional<vk::UniqueCommandBuffer> generate_command_buffer(class renderer*, struct render_node* node) { return {}; }
    virtual void build_gui(class renderer*, struct render_node* node) {}
    virtual std::unique_ptr<render_node_data> deserialize_node_data(json data) { return nullptr; }
    virtual const char* name() const { return "fail"; }
    virtual size_t id() const = 0;
    virtual ~render_node_prototype() {}
};

struct render_node {
    bool visited;
    uint32_t subpass_index;
    std::optional<vk::UniqueCommandBuffer> subpass_commands;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSet desc_set;

    size_t id;
    std::shared_ptr<render_node_prototype> prototype;
    std::vector<std::pair<std::shared_ptr<render_node>, size_t>> inputs;
    std::vector<framebuffer_ref> outputs;
    std::unique_ptr<render_node_data> data;

    render_node(std::shared_ptr<render_node_prototype> prototype);
    render_node(renderer*, size_t id, json data);

    inline std::optional<framebuffer_ref> input_framebuffer(size_t i) const {
        if(inputs[i].first == nullptr) return {};
        return inputs[i].first->outputs[inputs[i].second];
    }

    json serialize() const;
};

struct frame_uniforms {
    mat4 view, proj;
};

struct renderer {
    device* dev; swap_chain* swpc;
    std::vector<std::shared_ptr<render_node_prototype>> prototypes;
    std::vector<std::shared_ptr<render_node>> render_graph;
    std::shared_ptr<render_node> screen_output_node;

    framebuffer_ref next_id;
    std::map<framebuffer_ref, std::tuple<std::unique_ptr<image>, bool, vk::UniqueImageView, framebuffer_type>> buffers;

    vk::UniqueRenderPass render_pass;
    std::vector<vk::ClearValue> clear_values;
    vk::RenderPassBeginInfo render_pass_begin_info;
    std::vector<std::shared_ptr<render_node>> subpass_order;
    vk::UniqueDescriptorPool desc_pool;
    frame_uniforms* mapped_frame_uniforms;

    framebuffer_ref allocate_framebuffer(const framebuffer_desc&);
    void compile_render_graph();
    void propagate_blended_framebuffers(std::shared_ptr<render_node> node);
    void generate_subpasses(std::shared_ptr<render_node>, std::vector<vk::SubpassDescription>&, std::vector<vk::SubpassDependency>& dependencies,
            const std::map<framebuffer_ref, uint32_t>& attachement_refs, arena<vk::AttachmentReference>& reference_pool);

    void deserialize_render_graph(json data);
    json serialize_render_graph();

    void traverse_scene_graph(scene_object*, frame_state*, const mat4& T);
    bool should_recompile, log_compile;
    
    std::map<int, std::tuple<std::shared_ptr<render_node>, size_t, bool /*input|output*/>> gui_node_attribs;
    std::vector<std::pair<int, int>> gui_links;
    //---

    std::vector<vk::UniqueFramebuffer> framebuffers;
    std::shared_ptr<scene> current_scene;

    std::vector<std::tuple<struct mesh*, mat4>> active_meshes;
    std::vector<std::tuple<light_trait*, mat4>> active_lights;
    std::vector<viewport_shape> active_shapes;
    std::unique_ptr<buffer> frame_uniforms_buf;
    vk::Viewport full_viewport; vk::Rect2D full_scissor;

    bool show_shapes;

    renderer();
    void init(device* dev);
    void create_swapchain_dependencies(swap_chain* swpc);
    void build_gui(frame_state*);
    void render(vk::CommandBuffer& cb, uint32_t image_index, frame_state* fs);
    ~renderer();
};
