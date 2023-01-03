#include "eggv_app.h"
#include <utility>

using namespace emlisp;

void add_bindings(runtime* rt, void* cx);

void eggv_app::init_script_runtime() {

    add_bindings(script_runtime.get(), this);

    // WARN: dangerous
    script_runtime->define_global("W", script_runtime->make_extern_reference<world>(this->w.get()));

    heap_info hfo;
    script_runtime->collect_garbage(&hfo);
    std::cout << "initializing script runtime created " << (hfo.new_size - hfo.old_size)
              << "b of garbage\n";
}
