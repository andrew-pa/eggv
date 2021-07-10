
#define EGGV_IMPORT
#include "cmmn.h"
#include "mesh.h"
#include <assimp/Importer.hpp>
#include "assimp/postprocess.h"
#include <assimp/scene.h>

int main(int argc, char* argv[]) {
    char *input_path = nullptr, *output_path = nullptr;

    for(int i = 0; i < argc; ++i) {
        if(strcmp(argv[i], "-i") == 0) {
            input_path = argv[++i];
        }
        if(strcmp(argv[i], "-o") == 0) {
            output_path = argv[++i];
        }
    }

    std::cout << input_path << " => " << output_path << "\n";

    Assimp::Importer imp;
    imp.ReadFile(input_path, aiProcess_GenBoundingBoxes | aiProcessPreset_TargetRealtime_MaxQuality);

    return 0;
}
