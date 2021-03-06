#pragma once
#include <utility>

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
    /// framebuffer is bound as an input attachement
    input_attachment,
    /// framebuffer is bound only as shader input
    shader_input,
    /// framebuffer used as source for blending onto the output at a matching index
    blend_input,
    /// framebuffer is a render target
    output
};

enum class framebuffer_subpass_binding_order {
    parallel,
    sequential
};

const uint32_t framebuffer_count_is_subpass_count = (uint32_t)-1;

struct framebuffer_desc {
    std::string name;
    vk::Format format;
    framebuffer_type type;
    framebuffer_mode mode;
    uint32_t count;
    framebuffer_subpass_binding_order subpass_binding_order;
    framebuffer_desc(
            std::string name,
            vk::Format fmt,
            framebuffer_type ty,
            framebuffer_mode mode = framebuffer_mode::input_attachment,
            uint32_t count = 1,
            framebuffer_subpass_binding_order bo = framebuffer_subpass_binding_order::parallel
    ) : name(std::move(name)), format(fmt), type(ty), mode(mode), count(count), subpass_binding_order(bo) {}
};

struct render_node_data {
    virtual json serialize() const { return json{}; }
    virtual ~render_node_data() = default;
};

struct single_pipeline_node_data : public render_node_data {
    vk::UniquePipeline pipeline;
    json serialize() const override { return json{}; }
    ~single_pipeline_node_data() override = default;
};

struct render_node_prototype {
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    std::vector<framebuffer_desc> inputs, outputs;

    virtual size_t subpass_repeat_count(class renderer* r, struct render_node* node) { return 1; }

    virtual void collect_descriptor_layouts(struct render_node*, std::vector<vk::DescriptorPoolSize>& pool_sizes, 
            std::vector<vk::DescriptorSetLayout>& layouts, std::vector<vk::UniqueDescriptorSet*>& outputs) {}
    virtual void update_descriptor_sets(class renderer*, struct render_node*, std::vector<vk::WriteDescriptorSet>& writes, arena<vk::DescriptorBufferInfo>& buf_infos, arena<vk::DescriptorImageInfo>& img_infos) {}

    virtual void generate_pipelines(class renderer*, struct render_node*, vk::RenderPass render_pass, uint32_t subpass) {}

    virtual void generate_command_buffer_inline(class renderer*, struct render_node*, vk::CommandBuffer&, size_t subpass_index) {}
    virtual std::optional<std::vector<vk::UniqueCommandBuffer>> generate_command_buffer(class renderer*, struct render_node* node) { return {}; }

    virtual void build_gui(class renderer*, struct render_node* node) {}
    virtual std::unique_ptr<render_node_data> deserialize_node_data(json data) { return initialize_node_data(); }
    virtual std::unique_ptr<render_node_data> initialize_node_data() { return nullptr; }

    virtual const char* name() const { return "fail"; }
    virtual size_t id() const = 0;
    virtual ~render_node_prototype() = default;
};

struct render_node {
    bool visited;
    uint32_t subpass_index, subpass_count;
    std::optional<std::vector<vk::UniqueCommandBuffer>> subpass_commands;
    vk::UniqueDescriptorSet desc_set;

    size_t id;
    std::shared_ptr<render_node_prototype> prototype;
    std::vector<std::pair<std::optional<std::weak_ptr<render_node>>, size_t>> inputs;
    std::vector<framebuffer_ref> outputs;
    std::unique_ptr<render_node_data> data;

    render_node(std::shared_ptr<render_node_prototype> prototype);
    render_node(renderer*, size_t id, json data);

    inline std::shared_ptr<render_node> input_node(size_t i) const {
        if (!inputs[i].first.has_value()) return nullptr;
        return inputs[i].first->lock();
    }
    inline std::optional<framebuffer_ref> input_framebuffer(size_t i) const {
        auto inp = input_node(i);
        if(inp == nullptr) return {};
        return inp->outputs[inputs[i].second];
    }

    json serialize() const;

    virtual ~render_node() {
//        std::cout << "goodbye rendernode\n";
    }
};


struct frame_uniforms {
    mat4 view, proj;
};

struct gpu_material {
    vec4 base_color;

    gpu_material(material* mat) : base_color(mat->base_color, 1.f) {}
};

struct framebuffer_values {
    std::unique_ptr<image> img;
    bool in_use;
    std::vector<vk::UniqueImageView> image_views;
    framebuffer_type type;

    framebuffer_values()
        : img(nullptr), in_use(false), type(framebuffer_type::color) {}

    framebuffer_values(std::unique_ptr<image>&& img, bool in_use, std::vector<vk::UniqueImageView>&& image_views, framebuffer_type type)
        : img(std::move(img)), in_use(in_use), image_views(std::move(image_views)), type(type) {}

    inline bool is_array() const { return image_views.size() > 1; }
    inline size_t num_layers() const { return image_views.size() == 1 ? 1 : image_views.size() - 1; }
};

const size_t GLOBAL_BUF_FRAME_UNIFORMS = 1;
const size_t GLOBAL_BUF_MATERIALS = 2;

class renderer {
    void build_gui_menu(frame_state* fs);
    void build_gui_graph_view(frame_state* fs);
    void build_gui_stats(frame_state* fs);
    void build_gui_textures(frame_state* fs);

    void generate_attachment_descriptions(std::vector<vk::AttachmentDescription>& attachments, std::map<framebuffer_ref, uint32_t>& attachment_refs);
    void generate_clear_values();
public:
    device* dev; swap_chain* swpc;
    std::vector<std::shared_ptr<render_node_prototype>> prototypes;
    std::vector<std::shared_ptr<render_node>> render_graph;
    std::shared_ptr<render_node> screen_output_node;

    framebuffer_ref next_id;
    std::map<framebuffer_ref, framebuffer_values> buffers;

    vk::UniqueRenderPass render_pass;
    std::vector<vk::ClearValue> clear_values;
    vk::RenderPassBeginInfo render_pass_begin_info;
    std::vector<std::shared_ptr<render_node>> subpass_order;
    vk::UniqueDescriptorPool desc_pool;

    std::map<size_t, std::unique_ptr<buffer>> global_buffers;
    frame_uniforms* mapped_frame_uniforms;
    gpu_material* mapped_materials;
    uint32_t num_gpu_mats;

    std::vector<std::tuple<std::string, std::shared_ptr<image>, vk::UniqueImageView, uint64_t>> texture_cache;
    size_t create_texture2d(const std::string& name, uint32_t width, uint32_t height, vk::Format fmt, size_t data_size, void* data, vk::CommandBuffer uplcb);
    size_t load_texture(const std::string&, vk::CommandBuffer uplcb);
    vk::UniqueSampler texture_sampler;
    vk::UniqueDescriptorPool material_desc_pool;
    vk::UniqueDescriptorSetLayout material_desc_set_layout;

    framebuffer_ref allocate_framebuffer(const framebuffer_desc&, uint32_t subpass_count);
    void compile_render_graph();
    void propagate_blended_framebuffers(std::shared_ptr<render_node> node);
    void generate_subpasses(const std::shared_ptr<render_node>&, std::vector<vk::SubpassDescription>&, std::vector<vk::SubpassDependency>& dependencies,
            const std::map<framebuffer_ref, uint32_t>& attachment_refs, arena<vk::AttachmentReference>& reference_pool);

    void deserialize_render_graph(json data);
    json serialize_render_graph();

    void traverse_scene_graph(scene_object*, frame_state*, const mat4& T);
    bool should_recompile, log_compile;
    
    std::map<int, std::tuple<std::shared_ptr<render_node>, size_t, bool /*input|output*/>> gui_node_attribs;
    std::vector<std::pair<int, int>> gui_links;
    //---

    std::vector<vk::UniqueFramebuffer> framebuffers;
    std::shared_ptr<scene> current_scene;

    std::vector<std::tuple<struct mesh_trait*, mat4>> active_meshes;
    std::vector<std::tuple<light_trait*, mat4>> active_lights;
    std::vector<viewport_shape> active_shapes;
    vk::Viewport full_viewport; vk::Rect2D full_scissor;

    bool show_shapes;

    renderer();
    void init(device* dev);
    void create_swapchain_dependencies(swap_chain* swpc);
    void build_gui(frame_state*);
    void update(frame_state*);
    void render(vk::CommandBuffer& cb, uint32_t image_index, frame_state* fs);
    ~renderer();
};

struct single_pipeline_render_node_prototype : public render_node_prototype {
    std::unique_ptr<render_node_data> initialize_node_data() override { return std::make_unique<single_pipeline_node_data>(); }
    ~single_pipeline_render_node_prototype() override = default;

    inline vk::Pipeline pipeline(render_node* node) {
        return ((single_pipeline_node_data*)node->data.get())->pipeline.get();
    }

    inline vk::Pipeline pipeline(const std::shared_ptr<render_node>& node) { return pipeline(node.get()); }

    inline void create_pipeline(renderer* r, render_node* node, const vk::GraphicsPipelineCreateInfo& desc) {
        ((single_pipeline_node_data*)node->data.get())->pipeline = 
            r->dev->dev->createGraphicsPipelineUnique(nullptr, desc);
    }
};
