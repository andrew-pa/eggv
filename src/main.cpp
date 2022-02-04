#include "eggv_app.h"

int main(int argc, const char* argv[]) {
    eggv_app app(eggv_cmdline_args(argc, argv));
    app.run();
    return 0;
}
