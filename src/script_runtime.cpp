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
        auto w = to_float(nth(args, 3));
        return rt->make_owned_extern<quat>(x, y, z, w);
    });

    script_runtime->define_fn(
        "world",
        [](runtime* rt, value args, void* cx) {
            auto* self = (eggv_app*)cx;
            return rt->make_extern_reference<world>(self->w.get());
        },
        this
    );

    script_runtime->define_fn(
        "bundle",
        [](runtime* rt, value args, void* cx) {
            auto* self = (eggv_app*)cx;
            return rt->make_extern_reference<bundle>(self->bndl.get());
        },
        this
    );

    heap_info hfo;
    script_runtime->collect_garbage(&hfo);
    std::cout << "initializing script runtime created " << (hfo.old_size - hfo.new_size)
              << "b of garbage\n";

    if(!bndl->init_script.empty()) {
        try {
            script_runtime->eval_file(bndl->init_script);
        } catch(emlisp::type_mismatch_error e) {
            std::cout << "error: " << e.what() << ". expected: " << e.expected
                      << " actual: " << e.actual << "\n\tfrom: ";
            script_runtime->write(std::cout, e.trace) << "\n";
        } catch(std::runtime_error e) { std::cout << "error: " << e.what(); }

        script_runtime->collect_garbage(&hfo);
        std::cout << "running init script created " << (hfo.old_size - hfo.new_size)
                  << "b of garbage\n";
    }
}
