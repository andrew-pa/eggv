#pragma once
#include "cmmn.h"
#include "device.h"

typedef uint32_t trait_id;

struct frame_state {
    float t, dt;
    
    frame_state(float t, float dt) : t(t), dt(dt) {}
};

struct trait {
    struct trait_factory* parent;

    trait(trait_factory* p) : parent(p) {}

    virtual void update(struct scene_object*, frame_state*) {}

    virtual void append_transform(struct scene_object*, mat4& T, frame_state*) {}

    virtual void build_gui(struct scene_object*, frame_state*) { }

    virtual ~trait() {}
};

struct trait_factory {
    virtual trait_id id() const = 0;
    virtual std::string name() const = 0;

    virtual void add_to(struct scene_object* obj, void* create_info) = 0;
    virtual ~trait_factory() {}
};

struct scene_object {
    std::optional<std::string> name;
    std::map<trait_id, std::unique_ptr<trait>> traits;
    std::vector<std::shared_ptr<scene_object>> children;

    scene_object(std::optional<std::string> name = {}) : name(name), traits{}, children{} {}
};

struct camera {
    float fov;
    vec3 position, target;

    camera(vec3 pos, vec3 targ, float fov)
        :fov(fov), position(pos), target(targ) {}

    void update(frame_state* fs);

    mat4 proj(float aspect_ratio);
    mat4 view();
};

class scene {
    void build_scene_graph_tree(std::shared_ptr<scene_object> obj);
public:
    std::vector<std::shared_ptr<trait_factory>> trait_factories;
    std::shared_ptr<scene_object> root;
    std::shared_ptr<scene_object> selected_object;
    camera cam;

    scene(std::vector<std::shared_ptr<trait_factory>> trait_factories, std::shared_ptr<scene_object> root, camera cam) : trait_factories(trait_factories), root(root), cam(cam), selected_object(root) {}

    void update(frame_state* fs);
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
    void add_to(scene_object* obj, void* ci) override {
        auto c = ((create_info*)ci);
        obj->traits[id()] = std::make_unique<transform_trait>(this,
                c==nullptr?vec3(0.f):c->translation, c==nullptr?quat(0.f,0.f,0.f,1.f):c->rotation, c==nullptr?vec3(1.f):c->scale);
    }
};
