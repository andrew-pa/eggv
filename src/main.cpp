#include "eggv_app.h"

int main(int argc, const char* argv[]) {
    auto args = std::vector<std::string>();
    for (int i = 0; i < argc; ++i)
        args.push_back(std::string(argv[i]));
    eggv_app app(args);
    app.run();
    return 0;
}
