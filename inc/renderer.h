#pragma once
#include "cmmn.h"
#include "app.h"
#include "swap_chain.h"
#include "scene_graph.h"

typedef size_t framebuffer_ref;

enum class framebuffer_type {
    color, depth, depth_stencil
};

struct framebuffer_desc {
    std::string name;
    vk::Format format;
    framebuffer_type type;
    framebuffer_desc(std::string name, vk::Format fmt, framebuffer_type ty)
        : name(name), format(fmt), type(ty) {}
};

struct render_node_prototype {
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    std::vector<framebuffer_desc> inputs, outputs;

    virtual void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) {}
    virtual void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes, std::vector<vk::DescriptorBufferInfo>& buf_infos, std::vector<vk::DescriptorImageInfo>& img_infos) {}
    virtual vk::UniquePipeline generate_pipeline(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) = 0;
    virtual void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&) {}
    virtual std::optional<vk::UniqueCommandBuffer> generate_command_buffer(class renderer*, struct render_node* node) { return {}; }
    virtual void build_gui(class renderer*, struct render_node* node) {}
    virtual const char* name() const = 0;
    virtual ~render_node_prototype() {}
};

struct render_node_data {
    virtual ~render_node_data() {}
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

    inline std::optional<framebuffer_ref> input_framebuffer(size_t i) const {
        if(inputs[i].first == nullptr) return {};
        return inputs[i].first->outputs[inputs[i].second];
    }
};

struct vertex {
    vec3 position, normal;
    vec2 texcoord;
    vertex(vec3 p = vec3(0.f), vec3 n = vec3(0.f), vec2 tx = vec2(0.f))
        : position(p), normal(n), texcoord(tx) {}
};

constexpr static vk::VertexInputAttributeDescription vertex_attribute_description[] = {
    vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(vertex, position)},
    vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(vertex, normal)},
    vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32Sfloat,       offsetof(vertex, texcoord)},
};

struct mesh {
    std::unique_ptr<buffer> vertex_buffer;
    std::unique_ptr<buffer> index_buffer;
    size_t vertex_count, index_count;

    mesh(device* dev, size_t vcount, size_t icount, std::function<void(void*)> write_buffer);
    mesh(device* dev, const std::vector<vertex>& vertices, const std::vector<uint16>& indices);
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
    void generate_subpasses(std::shared_ptr<render_node>, std::vector<vk::SubpassDescription>&, std::vector<vk::SubpassDependency>& dependencies,
            const std::map<framebuffer_ref, uint32_t>& attachement_refs, std::vector<vk::AttachmentReference>& reference_pool);

    void traverse_scene_graph(scene_object*, frame_state*);
    bool should_recompile;
    
    std::map<int, std::tuple<std::shared_ptr<render_node>, size_t, bool /*input|output*/>> gui_node_attribs;
    std::vector<std::pair<int, int>> gui_links;
    //---

    std::vector<vk::UniqueFramebuffer> framebuffers;
    std::shared_ptr<scene> current_scene;

    std::vector<std::tuple<mesh*, mat4>> active_meshes;
    std::unique_ptr<buffer> frame_uniforms_buf;
    vk::Viewport full_viewport; vk::Rect2D full_scissor;

    renderer(device* dev, std::shared_ptr<scene> s);
    void create_swapchain_dependencies(swap_chain* swpc);
    void build_gui();
    void render(vk::CommandBuffer& cb, uint32_t image_index, frame_state* fs);
    ~renderer();
};

const trait_id TRAIT_ID_MESH = 0x00010001;
struct mesh_trait : public trait {
    std::shared_ptr<mesh> m;
    mesh_trait(trait_factory* p, std::shared_ptr<mesh> m) : m(m), trait(p) {}
    void append_transform(struct scene_object*, mat4& T, frame_state*) override {}
    void build_gui(struct scene_object*, frame_state*) override;
};

struct mesh_trait_factory : public trait_factory {
    struct create_info {
        std::shared_ptr<mesh> m;
    };

    trait_id id() const override { return TRAIT_ID_MESH; }
    std::string name() const override { return "Mesh"; }
    void add_to(scene_object* obj, void* ci) override {
        obj->traits[id()] = std::make_unique<mesh_trait>(this,
                ((create_info*)ci)->m);
    }
};
