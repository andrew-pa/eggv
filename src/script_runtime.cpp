#include "eggv_app.h"

using namespace emlisp;

void eggv_app::init_script_runtime() {
    script_runtime->define_fn("scene/object-by-id", [](runtime* rt, value args, void* closure) {
        return NIL;
    }, this);
}
