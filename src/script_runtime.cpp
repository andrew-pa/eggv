#include "eggv_app.h"
#include <utility>

using namespace emlisp;

/* ---- trait lifetime -----
        trait_factory
       /    trait
       init/      Render loop
       |   mount /
       |   |   update
       |   |   append_transform
       |   |   build_gui
       |   |   collect_viewport_shapes
       |   unmount
       deinit
****************************/

value assoc(value m, value key) {
    while(m != NIL) {
        value p = first(m);
        if(first(p) == key) return second(p);
        m = second(m);
    }
    return NIL;
}

struct user_trait : public trait {
    value_handle state;

    user_trait(trait_factory* p, value_handle state) : trait(p), state(std::move(state)) {}

    void update(scene_object* obj, frame_state* fs) override;
    void append_transform(scene_object* obj, mat4& T, frame_state* fs) override;
    void build_gui(scene_object* obj, frame_state* fs) override;
    void collect_viewport_shapes(
        scene_object*                ob,
        frame_state*                 fs,
        const mat4&                  T,
        bool                         selected,
        std::vector<viewport_shape>& shapes
    ) override;
};

struct user_trait_factory : public trait_factory {
    std::shared_ptr<runtime> rt;
    trait_id                 trait_subid;
    std::string              trait_name;
    std::vector<trait_id>    dependencies;

    value_handle init_fn, deinit_fn, mount_fn, unmount_fn;
    value_handle update_fn, append_transform_fn, build_gui_fn, collect_viewport_shapes_fn;

    user_trait_factory(const std::shared_ptr<runtime>& rt, value args)
        : rt(rt), trait_subid(to_int(assoc(args, rt->symbol("id")))),
          trait_name(rt->to_str(assoc(args, rt->symbol("name")))),
          init_fn(rt->handle_for(assoc(args, rt->symbol("init")))),
          deinit_fn(rt->handle_for(assoc(args, rt->symbol("deinit")))),
          mount_fn(rt->handle_for(assoc(args, rt->symbol("mount")))),
          unmount_fn(rt->handle_for(assoc(args, rt->symbol("unmount")))),
          update_fn(rt->handle_for(assoc(args, rt->symbol("update")))),
          append_transform_fn(rt->handle_for(assoc(args, rt->symbol("append-transform")))),
          build_gui_fn(rt->handle_for(assoc(args, rt->symbol("build-gui")))),
          collect_viewport_shapes_fn(rt->handle_for(assoc(args, rt->symbol("viewport-shapes")))) {
        value deps = assoc(args, rt->symbol("dependencies"));
        while(deps != NIL) {
            value dep = first(deps);
            if(type_of(dep) == value_type::sym) {
                trait_id res;
                if(dep == rt->symbol("transform"))
                    res = TRAIT_ID_TRANSFORM;
                else if(dep == rt->symbol("light"))
                    res = TRAIT_ID_LIGHT;
                else if(dep == rt->symbol("camera"))
                    res = TRAIT_ID_CAMERA;
                else if(dep == rt->symbol("mesh"))
                    res = TRAIT_ID_MESH;
                else if(dep == rt->symbol("rigid-body"))
                    res = TRAIT_ID_RIGID_BODY;
                else { /* TODO: error */
                }
            } else {
                dependencies.emplace_back(to_int(dep));
            }
            deps = second(deps);
        }
    }

    trait_id id() const override { return TRAIT_ID_USER & trait_subid; }

    std::string name() const override { return trait_name; }

    void add_to(struct scene_object* obj, void* create_info) override {
        value state = rt->apply(*mount_fn, rt->cons(rt->make_extern_reference(obj), rt->cons(NIL)));
        obj->traits[id()] = std::make_unique<user_trait>(this, rt->handle_for(state));
    }

    void deserialize(class scene* scene, struct scene_object* obj, json data) override {
        value state = rt->read(data["state"].get<std::string>());
        state = rt->apply(*mount_fn, rt->cons(rt->make_extern_reference(obj), rt->cons(state)));
        obj->traits[id()] = std::make_unique<user_trait>(this, rt->handle_for(state));
    }

    bool dependencies_loaded(scene_object* obj, const json& unloaded_trait) override {
        return std::all_of(
            std::begin(this->dependencies),
            std::end(this->dependencies),
            [&](auto t) { return obj->traits[t] != nullptr; }
        );
    }
};

void user_trait::update(scene_object* obj, frame_state* fs) {
    auto* p  = (user_trait_factory*)this->parent;
    auto  rt = p->rt;
    if(*p->update_fn == NIL) return;
    *this->state = rt->apply(
        *p->update_fn, rt->cons(rt->make_extern_reference(obj), rt->cons(*this->state))
    );
}

void user_trait::append_transform(scene_object* obj, mat4& T, frame_state* fs) {
    auto* p  = (user_trait_factory*)this->parent;
    auto  rt = p->rt;
    if(*p->append_transform_fn == NIL) return;
    value new_T = rt->apply(
        *p->append_transform_fn,
        rt->cons(
            rt->make_extern_reference(obj),
            rt->cons(
                fs->t,
                rt->cons(fs->dt, rt->cons(rt->from_fvec(16, &T[0][0]), rt->cons(*this->state)))
            )
        )
    );
    auto [n, t] = rt->to_fvec(new_T);
    memcpy(&T[0][0], t, sizeof(float) * n);
}

void user_trait::build_gui(scene_object* obj, frame_state* fs) {
    auto* p  = (user_trait_factory*)this->parent;
    auto  rt = p->rt;
    if(*p->build_gui_fn == NIL) return;
    *this->state = rt->apply(
        *p->build_gui_fn, rt->cons(rt->make_extern_reference(obj), rt->cons(*this->state))
    );
}

void user_trait::collect_viewport_shapes(
    scene_object*                ob,
    frame_state*                 fs,
    const mat4&                  T,
    bool                         selected,
    std::vector<viewport_shape>& shapes
) {
    auto* p  = (user_trait_factory*)this->parent;
    auto  rt = p->rt;
    if(*p->collect_viewport_shapes_fn == NIL) return;
    value lshapes = rt->apply(
        *p->collect_viewport_shapes_fn,
        rt->cons(
            rt->make_extern_reference(ob), rt->cons(*this->state, rt->cons(rt->from_bool(selected)))
        )
    );

    while(lshapes != NIL) {
        value               shape = first(lshapes);
        viewport_shape_type ty;
        if(first(shape) == rt->symbol("axis"))
            ty = viewport_shape_type::axis;
        else if(first(shape) == rt->symbol("box"))
            ty = viewport_shape_type::box;
        else { /*error!*/
        }
        auto [col_n, col_d] = rt->to_fvec(first(second(shape)));
        assert(col_n == 3);
        vec3* col       = (vec3*)col_d;
        auto [t_n, t_d] = rt->to_fvec(first(second(second(shape))));
        assert(t_n == 16);
        mat4* t = (mat4*)t_d;
        shapes.emplace_back(ty, *col, T * (*t));
        lshapes = second(lshapes);
    }
}

value register_user_trait(runtime* rt, value args, void* closure) {
    auto* app = (eggv_app*)closure;
    auto  f   = std::make_shared<user_trait_factory>(app->script_runtime, args);
    app->current_scene->trait_factories.emplace_back(f);
    return NIL;
}

void eggv_app::init_script_runtime() {
    script_runtime->define_fn(
        "scene/object-by-id", [](runtime* rt, value args, void* closure) { return NIL; }, this
    );

    script_runtime->define_fn("__user-trait", register_user_trait, this);

    heap_info hfo;
    script_runtime->collect_garbage(&hfo);
    std::cout << "initializing script runtime created " << (hfo.new_size - hfo.old_size)
              << "b of garbage\n";
}
