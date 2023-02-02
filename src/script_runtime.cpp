#include "eggv_app.h"
#include <utility>

using namespace emlisp;

void add_bindings(runtime* rt, void* cx);

void eggv_app::init_script_runtime() {

    add_bindings(script_runtime.get(), this);

    script_runtime->define_fn("v3", [](runtime* rt, value args, void* cx) {
        auto x = to_float(nth(args, 0));
        auto y = to_float(nth(args, 1));
        auto z = to_float(nth(args, 2));
        return rt->make_owned_extern<vec3>(x, y, z);
    });

    script_runtime->define_fn("quat", [](runtime* rt, value args, void* cx) {
        auto x = to_float(nth(args, 0));
        auto y = to_float(nth(args, 1));
        auto z = to_float(nth(args, 2));
        auto w = to_float(nth(args, 4));
        return rt->make_owned_extern<quat>(x, y, z, w);
    });

    // WARN: dangerous
    script_runtime->define_global("W", script_runtime->make_extern_reference<world>(this->w.get()));

    heap_info hfo;
    script_runtime->collect_garbage(&hfo);
    std::cout << "initializing script runtime created " << (hfo.new_size - hfo.old_size)
              << "b of garbage\n";
}
