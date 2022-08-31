#pragma once
#include "app.h"
#include "bundle.h"
#include "cmmn.h"
#include "ecs.h"
#include "mem_arena.h"
#include "mesh.h"
#include "scene_components.h"
#include "swap_chain.h"
#include <utility>

using framebuffer_ref = size_t;

enum class framebuffer_type { color, depth, depth_stencil };

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

enum class framebuffer_subpass_binding_order { parallel, sequential };

const uint32_t framebuffer_count_is_subpass_count = (uint32_t)-1;

struct framebuffer_desc {
    std::string                       name;
    vk::Format                        format;
    framebuffer_type                  type;
    framebuffer_mode                  mode;
    uint32_t                          count;
    framebuffer_subpass_binding_order subpass_binding_order;

    framebuffer_desc(
        std::string                       name,
        vk::Format                        fmt,
        framebuffer_type                  ty,
        framebuffer_mode                  mode  = framebuffer_mode::input_attachment,
        uint32_t                          count = 1,
        framebuffer_subpass_binding_order bo    = framebuffer_subpass_binding_order::parallel
    )
        : name(std::move(name)), format(fmt), type(ty), mode(mode), count(count),
          subpass_binding_order(bo) {}
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
    vk::UniquePipelineLayout      pipeline_layout;
    std::vector<framebuffer_desc> inputs, outputs;

    virtual size_t subpass_repeat_count(class renderer* r, struct render_node* node) { return 1; }

    virtual void collect_descriptor_layouts(
        struct render_node*                    node,
        std::vector<vk::DescriptorPoolSize>&   pool_sizes,
        std::vector<vk::DescriptorSetLayout>&  layouts,
        std::vector<vk::UniqueDescriptorSet*>& outputs
    ) {}

    virtual void update_descriptor_sets(
        class renderer*                      r,
        struct render_node*                  node,
        std::vector<vk::WriteDescriptorSet>& writes,
        arena<vk::DescriptorBufferInfo>&     buf_infos,
        arena<vk::DescriptorImageInfo>&      img_infos
    ) {}

    virtual void generate_pipelines(
        class renderer* r, struct render_node* node, vk::RenderPass render_pass, uint32_t subpass
    ) {}

    virtual void generate_command_buffer_inline(
        class renderer*     r,
        struct render_node* node,
        vk::CommandBuffer&  cb,
        size_t              subpass_index,
        const frame_state&  fs
    ) {}

    virtual std::optional<std::vector<vk::UniqueCommandBuffer>> generate_command_buffer(
        class renderer* r, struct render_node* node
    ) {
        return {};
    }

    virtual void build_gui(class renderer* r, struct render_node* node) {}

    virtual std::unique_ptr<render_node_data> deserialize_node_data(const json& data) {
        return initialize_node_data();
    }

    virtual std::unique_ptr<render_node_data> initialize_node_data() { return nullptr; }

    virtual const char* name() const { return "fail"; }

    virtual size_t id() const        = 0;
    virtual ~render_node_prototype() = default;
};

struct render_node {
    bool                                                visited;
    uint32_t                                            subpass_index, subpass_count;
    std::optional<std::vector<vk::UniqueCommandBuffer>> subpass_commands;
    vk::UniqueDescriptorSet                             desc_set;

    size_t                                                                    id;
    std::shared_ptr<render_node_prototype>                                    prototype;
    std::vector<std::pair<std::optional<std::weak_ptr<render_node>>, size_t>> inputs;
    std::vector<framebuffer_ref>                                              outputs;
    std::unique_ptr<render_node_data>                                         data;

    render_node(std::shared_ptr<render_node_prototype> prototype);
    render_node(renderer*, size_t id, json data);

    inline std::shared_ptr<render_node> input_node(size_t i) const {
        if(!inputs[i].first.has_value()) return nullptr;
        return inputs[i].first->lock();
    }

    inline std::optional<framebuffer_ref> input_framebuffer(size_t i) const {
        auto inp = input_node(i);
        if(inp == nullptr) return {};
        return inp->outputs[inputs[i].second];
    }

    json serialize() const;

    virtual ~render_node() = default;
};

struct frame_uniforms {
    mat4 view, proj;
};

struct gpu_material {
    vec4 base_color;

    gpu_material(material* mat) : base_color(mat->base_color, 1.f) {}
};

struct framebuffer_values {
    std::unique_ptr<image>           img;
    bool                             in_use;
    std::vector<vk::UniqueImageView> image_views;
    framebuffer_type                 type;

    framebuffer_values() : img(nullptr), in_use(false), type(framebuffer_type::color) {}

    framebuffer_values(
        std::unique_ptr<image>&&           img,
        bool                               in_use,
        std::vector<vk::UniqueImageView>&& image_views,
        framebuffer_type                   type
    )
        : img(std::move(img)), in_use(in_use), image_views(std::move(image_views)), type(type) {}

    inline bool is_array() const { return image_views.size() > 1; }

    inline size_t num_layers() const {
        return image_views.size() == 1 ? 1 : image_views.size() - 1;
    }
};

struct gpu_texture {
    std::shared_ptr<image> img;
    vk::UniqueImageView    img_view;
    uint64_t               imgui_tex_id;

    gpu_texture(std::shared_ptr<image> img, vk::UniqueImageView img_view)
        : img(std::move(img)), img_view(std::move(img_view)), imgui_tex_id(0) {}
};

const size_t GLOBAL_BUF_FRAME_UNIFORMS = 1;
const size_t GLOBAL_BUF_MATERIALS      = 2;

class renderer : public entity_system<mesh_component> {
    // THOUGHT: in some sense, the renderer is really another inner ECS `world` with its own
    // subsystems and components...

    // GUI subcomponents
    void build_gui_menu(const frame_state& fs);
    void build_gui_graph_view(const frame_state& fs);
    void build_gui_stats(const frame_state& fs);
    void build_gui_textures(const frame_state& fs);

    // render graph compilation helpers
    void generate_attachment_descriptions(
        std::vector<vk::AttachmentDescription>& attachments,
        std::map<framebuffer_ref, uint32_t>&    attachment_refs
    );
    void generate_clear_values();

  public:  // TODO: a lot of this stuff should be private
    static const system_id id = (system_id)static_systems::renderer;
    device*                dev;
    swap_chain*            swpc;

    // the render graph
    std::vector<std::shared_ptr<render_node_prototype>> prototypes;
    std::string                                         render_graph_name;
    std::vector<std::shared_ptr<render_node>>           render_graph;
    std::shared_ptr<render_node>                        screen_output_node;

    // "framebuffers" aka vk::Images that can be rendered to
    framebuffer_ref                               next_id;
    std::map<framebuffer_ref, framebuffer_values> buffers;
    framebuffer_ref allocate_framebuffer(const framebuffer_desc&, uint32_t subpass_count);
    // the actual Vulkan framebuffer objects that contain *all* attachments for a frame
    std::vector<vk::UniqueFramebuffer> framebuffers;

    // the render pass that was created from the render graph
    vk::UniqueRenderPass                      render_pass;
    std::vector<vk::ClearValue>               clear_values;
    vk::RenderPassBeginInfo                   render_pass_begin_info;
    std::vector<std::shared_ptr<render_node>> subpass_order;
    vk::UniqueDescriptorPool                  desc_pool;

    // uniform/storage buffers for shader parameters
    std::map<size_t, std::unique_ptr<buffer>> global_buffers;
    frame_uniforms*                           mapped_frame_uniforms;
    gpu_material*                             mapped_materials;
    uint32_t                                  num_gpu_mats;

    // texturing/materials
    std::unordered_map<std::string, gpu_texture> texture_cache;
    gpu_texture&                                 create_texture2d(
                                        const std::string& name,
                                        uint32_t           width,
                                        uint32_t           height,
                                        vk::Format         fmt,
                                        size_t             data_size,
                                        void*              data,
                                        vk::CommandBuffer  uplcb
                                    );
    gpu_texture&      load_texture_from_bundle(const std::string& name, vk::CommandBuffer uplcb);
    vk::UniqueSampler texture_sampler;
    vk::UniqueDescriptorPool      material_desc_pool;
    vk::UniqueDescriptorSetLayout material_desc_set_layout;

    // render graph "compilation" from graph -> Vulkan render pass
    void compile_render_graph();
    void propagate_blended_framebuffers(std::shared_ptr<render_node> node);
    void generate_subpasses(
        const std::shared_ptr<render_node>&,
        std::vector<vk::SubpassDescription>&,
        std::vector<vk::SubpassDependency>&        dependencies,
        const std::map<framebuffer_ref, uint32_t>& attachment_refs,
        arena<vk::AttachmentReference>&            reference_pool
    );

    void load_initial_render_graph();

    void deserialize_render_graph(const json& data);
    json serialize_render_graph();

    // to trigger a recompile at the next possible time
    bool should_recompile;

    // log messages about render graph compilation to stdout
    bool log_compile;

    // render viewport shapes
    bool show_shapes;

    // render graph editor state
    std::map<int, std::tuple<std::shared_ptr<render_node>, size_t, bool /*input|output*/>>
                                     gui_node_attribs;
    std::vector<std::pair<int, int>> gui_links;

    std::shared_ptr<bundle> current_bundle;

    // call `f` on each renderable entity ie every entity with a mesh and transform
    // provided as a helper for render nodes
    void for_each_renderable(
        const std::function<void(entity_id, const mesh_component&, const transform&)>& f
    );

    // viewport settings
    vk::Viewport full_viewport;
    vk::Rect2D   full_scissor;

    // renderer lifecycle
    renderer(const std::shared_ptr<world>& w);
    void init(device* dev);
    void create_swapchain_dependencies(swap_chain* swpc);
    void build_gui(frame_state& fs) override;
    void build_gui_for_entity(const frame_state& fs, entity_id selected_entity) override;
    void update(const frame_state& fs) override;
    void render(vk::CommandBuffer& cb, uint32_t image_index, const frame_state& fs);
    ~renderer() override;

    // generate viewport shapes for meshes
    void generate_viewport_shapes(
        const std::function<void(viewport_shape)>& add_shape, const frame_state& fs
    ) override;

    std::string_view name() const override { return "Renderer"; }

    inline world* current_world() const { return cur_world.lock().get(); }
};

struct single_pipeline_render_node_prototype : public render_node_prototype {
    std::unique_ptr<render_node_data> initialize_node_data() override {
        return std::make_unique<single_pipeline_node_data>();
    }

    ~single_pipeline_render_node_prototype() override = default;

    inline vk::Pipeline pipeline(render_node* node) {
        return ((single_pipeline_node_data*)node->data.get())->pipeline.get();
    }

    inline vk::Pipeline pipeline(const std::shared_ptr<render_node>& node) {
        return pipeline(node.get());
    }

    inline void create_pipeline(
        renderer* r, render_node* node, const vk::GraphicsPipelineCreateInfo& desc
    ) {
        ((single_pipeline_node_data*)node->data.get())->pipeline
            = r->dev->dev->createGraphicsPipelineUnique(nullptr, desc);
    }
};
