#pragma once
#include "cmmn.h"

typedef uint32_t trait_id;

struct frame_state {
    float t, dt;
    std::shared_ptr<class scene> current_scene;
    
    frame_state(float t, float dt, std::shared_ptr<class scene> cur_scn) : t(t), dt(dt), current_scene(cur_scn) {}
};

struct trait {
    struct trait_factory* parent;

    trait(trait_factory* p) : parent(p) {}

    virtual void update(struct scene_object*, frame_state*) {}

    virtual void append_transform(struct scene_object*, mat4& T, frame_state*) {}

    virtual void build_gui(struct scene_object*, frame_state*) { }

    virtual void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) {}

    virtual json serialize() const { return {}; }

    virtual ~trait() {}
};

struct trait_factory {
    virtual trait_id id() const = 0;
    virtual std::string name() const = 0;

    virtual void add_to(struct scene_object* obj, void* create_info) = 0;
    virtual void deserialize(struct scene_object* obj, json data) = 0;

    virtual ~trait_factory() {}
};

struct scene_object : public std::enable_shared_from_this<scene_object> {
    std::optional<std::string> name;
    std::map<trait_id, std::unique_ptr<trait>> traits;
    std::vector<std::shared_ptr<scene_object>> children;

    scene_object(std::optional<std::string> name = {}) : name(name), traits{}, children{} {}
};

class scene {
    void build_scene_graph_tree(std::shared_ptr<scene_object> obj);
public:
    std::vector<std::shared_ptr<trait_factory>> trait_factories;
    std::shared_ptr<scene_object> root;
    std::shared_ptr<scene_object> selected_object;
    std::shared_ptr<scene_object> active_camera;

    scene(std::vector<std::shared_ptr<trait_factory>> trait_factories, std::shared_ptr<scene_object> root) : trait_factories(trait_factories), root(root), selected_object(root) {}

    void update(frame_state* fs, class app*);
    void build_gui(frame_state* fs);
};

#include <glm/gtc/quaternion.hpp>

const trait_id TRAIT_ID_TRANSFORM = 0x00000001;
struct transform_trait : public trait {
    vec3 translation, scale;
    quat rotation;
    transform_trait(trait_factory* p, vec3 t, quat r, vec3 s) : translation(t), rotation(r), scale(s), trait(p) {}

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
    void deserialize(struct scene_object* obj, json data) override;
    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<transform_trait>(this,
                c==nullptr?vec3(0.f):c->translation, c==nullptr?quat(0.f,0.f,0.f,1.f):c->rotation, c==nullptr?vec3(1.f):c->scale);
    }
};

enum class light_type {
    directional, point
};

const trait_id TRAIT_ID_LIGHT = 0x0000'0010;
struct light_trait : public trait {
    light_type type;
    vec3 param;
    vec3 color;

    light_trait(trait_factory* f, light_type t, vec3 p, vec3 c) : type(t), param(p), color(c), trait(f) {}

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
    void deserialize(struct scene_object* obj, json data) override;
    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<light_trait>(this,
                c==nullptr?light_type::directional:c->type, c==nullptr?vec3():c->param, c==nullptr?vec3(1.f):c->color);
    }
};

const trait_id TRAIT_ID_CAMERA = 0x0000'0011;
struct camera_trait : public trait {
    float fov;

    camera_trait(trait_factory* f, float fov) : trait(f), fov(fov) {}

    void build_gui(struct scene_object*, frame_state*) override;
    void collect_viewport_shapes(struct scene_object*, frame_state*, const mat4& T, bool selected, std::vector<viewport_shape>& shapes) override;
};

struct camera_trait_factory : public trait_factory {
    struct create_info {
        float fov;
    };

    trait_id id() const override { return TRAIT_ID_CAMERA; }
    std::string name() const override { return "Camera"; }
    void deserialize(struct scene_object* obj, json data) override;

    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<camera_trait>(this, c!=nullptr?c->fov:pi<float>()/4.0f);
    }
};

