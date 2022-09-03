#include "eggv_app.h"
#include <utility>

using namespace emlisp;

value assoc(value m, value key) {
    while(m != NIL) {
        value p = first(m);
        if(first(p) == key) return second(p);
        m = second(m);
    }
    return NIL;
}

inline value nth(value list, size_t n) {
    if(list == NIL) return NIL;
    value cur = list;
    while(n > 0) {
        cur = second(cur);
        if(cur == NIL) return NIL;
        n--;
    }
    return first(cur);
}

void eggv_app::init_script_runtime() {
    // find/iter/create/destroy entities
    // we return the entity ID as an integer, which isn't very nicely typed but does the job
    script_runtime->define_fn("world/root-entity", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        return rt->from_int(app->w->root().id());
    }, this);

    script_runtime->define_fn("entity/new", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        auto name = nth(args, 0) != NIL ? rt->to_str(nth(args, 0)) : "";
        auto parent = nth(args, 1) != NIL ? app->w->entity(to_int(nth(args, 1))) : app->w->root();
        return rt->from_int(parent.add_child(std::string(name)).id());
    }, this);

    script_runtime->define_fn("entity/remove", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        app->w->entity(to_int(first(args))).remove();
        return NIL;
    }, this);

    script_runtime->define_fn("entity/children", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        value res = NIL;
        app->w->entity(to_int(first(args))).for_each_child([&](auto e) {
            res = rt->cons(rt->from_int(e.id()), res);
        });
        return res;
    }, this);

    script_runtime->define_fn("entity/for-each-child", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        app->w->entity(to_int(first(args))).for_each_child([&](auto e) {
            rt->apply(nth(args, 1), rt->cons(rt->from_int(e.id())));
        });
        return NIL;
    }, this);

    script_runtime->define_fn("entity/parent", [](runtime* rt, value args, void* cx){
        auto* app = (eggv_app*)cx;
        return rt->from_int(
                app->w->entity(to_int(first(args))).parent().id()
        );
    }, this);

    // add/remove standard components

    heap_info hfo;
    script_runtime->collect_garbage(&hfo);
    std::cout << "initializing script runtime created " << (hfo.new_size - hfo.old_size)
              << "b of garbage\n";
}
