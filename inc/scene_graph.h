#pragma once
#include "cmmn.h"
#include "uuid.h"

struct frame_state {
    float t, dt;
    std::shared_ptr<class scene> current_scene;
    std::map<std::string, bool>* gui_open_windows;
    
    frame_state(float t, float dt, std::shared_ptr<class scene> cur_scn, std::map<std::string, bool>* gow) : t(t), dt(dt), current_scene(cur_scn), gui_open_windows(gow) {}
};

struct trait {
    struct trait_factory* parent;

    trait(trait_factory* p) : parent(p) {}
 
    virtual void update(struct scene_object*, frame_state*, const mat4& T) {}

    virtual void append_transform(struct scene_object*, mat4& T, frame_state*) {}
    virtual void postprocess_transform(struct scene_object*, const mat4& T, frame_state*) {}

    virtual void build_gui(struct scene_object*, frame_state*) { }

    virtual void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) {}

    virtual json serialize() const { return {}; }

    // only called when a trait is removed from an object while running. Only the destructor will be called at exit
    virtual void remove_from(struct scene_object*) {}

    virtual ~trait() {}
};

struct trait_factory {
    virtual trait_id id() const = 0;
    virtual std::string name() const = 0;

    virtual void add_to(struct scene_object* obj, void* create_info) = 0;
    virtual void deserialize(class scene* scene, struct scene_object* obj, json data) = 0;

    virtual bool dependencies_loaded(struct scene_object* obj, const json& unloaded_trait) {
        return true;
    }

    virtual ~trait_factory() {}
};

struct scene_object : public std::enable_shared_from_this<scene_object> {
    uuids::uuid id;
    std::optional<std::string> name;
    std::map<trait_id, std::unique_ptr<trait>> traits;
    std::vector<std::shared_ptr<scene_object>> children;
    bool should_delete;

    scene_object(std::optional<std::string> name = {}, uuids::uuid id = uuids::uuid());
};

struct material {
    uuids::uuid id;
    std::string name;

    vec3 base_color;
    std::optional<std::string> diffuse_texpath;

    material(std::string name, vec3 base_color = vec3(.1f),
            std::optional<std::string> diffuse_texpath = {},
            uuids::uuid id = uuids::uuid());

    material(uuids::uuid id, json data);

    json serialize() const;

    bool build_gui(frame_state*);

    uint32 _render_index;
    vk::DescriptorSet desc_set;
};

class scene {
    void build_scene_graph_tree(std::shared_ptr<scene_object> obj);
    json serialize_graph(std::shared_ptr<scene_object> obj) const;
public:
    std::vector<std::shared_ptr<trait_factory>> trait_factories;
    std::shared_ptr<scene_object> root;
    std::shared_ptr<scene_object> selected_object;
    std::shared_ptr<scene_object> active_camera;
    std::vector<std::shared_ptr<class geometry_set>> geometry_sets;
    std::vector<std::shared_ptr<material>> materials;
    std::shared_ptr<material> selected_material;
    bool materials_changed;

    scene(std::vector<std::shared_ptr<trait_factory>> trait_factories, std::shared_ptr<scene_object> root) : trait_factories(trait_factories), root(root), selected_object(root), selected_material(nullptr), materials_changed(true) {}

    scene(class device* dev, std::vector<std::shared_ptr<trait_factory>> trait_factories,
        std::filesystem::path path, json data);

    void update(frame_state* fs, class app*);
    void build_gui(frame_state* fs);

    std::shared_ptr<scene_object> find_object_by_id(const uuids::uuid& id);
    
    void for_each_object(std::function<void(std::shared_ptr<scene_object>)> f);

    json serialize() const;

    ~scene() {
//        std::cout << "goodbye scene\n";
    }
};

#include <glm/gtc/quaternion.hpp>

struct transform_trait : public trait {
    vec3 translation, scale;
    quat rotation;
    transform_trait(trait_factory* p, vec3 t, quat r, vec3 s) : translation(t), rotation(r), scale(s), trait(p) {}

    json serialize() const override;
    void append_transform(struct scene_object*, mat4& T, frame_state*) override;
    void build_gui(struct scene_object*, frame_state*) override;
};

struct transform_trait_factory : public trait_factory {
    struct create_info {
        vec3 translation, scale;
        quat rotation;
        
        create_info(vec3 t = vec3(0.f), quat r = quat(0.f,0.f,0.f,1.f), vec3 s = vec3(1.f)) : translation(t), rotation(r), scale(s) {}
    };

    trait_id id() const override { return TRAIT_ID_TRANSFORM; }
    std::string name() const override { return "Transform"; }
    void deserialize(class scene*, struct scene_object* obj, json data) override;
    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<transform_trait>(this,
                c==nullptr?vec3(0.f):c->translation, c==nullptr?quat(0.f,0.f,0.f,1.f):c->rotation, c==nullptr?vec3(1.f):c->scale);
    }
};

struct light_trait : public trait {
    light_type type;
    vec3 param;
    vec3 color;
    size_t _render_index;

    light_trait(trait_factory* f, light_type t, vec3 p, vec3 c) : trait(f), type(t), param(p), color(c) {}

    json serialize() const override;
    void build_gui(struct scene_object*, frame_state*) override;
    void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) override;
};

struct light_trait_factory : public trait_factory {
    struct create_info {
        light_type type;
        vec3 param, color;
        create_info(light_type t, vec3 p, vec3 c) : type(t), param(p), color(c) {}
    };

    trait_id id() const override { return TRAIT_ID_LIGHT; }
    std::string name() const override { return "Light"; }
    void deserialize(class scene*, struct scene_object* obj, json data) override;
    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<light_trait>(this,
                c==nullptr?light_type::directional:c->type, c==nullptr?vec3(1.f, 0.f, 0.f):c->param, c==nullptr?vec3(1.f):c->color);
    }
};

struct camera_trait : public trait {
    float fov;

    camera_trait(trait_factory* f, float fov) : trait(f), fov(fov) {}

    json serialize() const override;
    void build_gui(struct scene_object*, frame_state*) override;
    void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) override;
};

struct camera_trait_factory : public trait_factory {
    struct create_info {
        float fov;
        create_info(float fov = 3.14159 / 4.0) : fov(fov) {}
    };

    trait_id id() const override { return TRAIT_ID_CAMERA; }
    std::string name() const override { return "Camera"; }
    void deserialize(class scene*, struct scene_object* obj, json data) override;

    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<camera_trait>(this, c!=nullptr?c->fov:pi<float>()/4.0f);
    }
};
