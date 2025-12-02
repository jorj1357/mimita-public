// C:\important\go away v5\s\mimita-v5\src\map\texture_manager.cpp

#include "texture_manager.h"
#include "map/texture.h"
#include <iostream>

/*
nov62025
todo
i dont think we use this cuz its hardcoding and we know hard coding is bad
*/
void TextureManager::init() {
    paths = {
        "assets/textures/bluev1.png",
        "assets/textures/boringskyv1.png",
        "assets/textures/circuitryv1.png",
        "assets/textures/flutev1.png",
        "assets/textures/greenwirev1.png",
        "assets/textures/greyskyv1.png",
        "assets/textures/grossv1.png",
        "assets/textures/grossv2.png",
        "assets/textures/strayv1.png",
        "assets/textures/tricksterv2.png",
        "assets/textures/tricksterv3.png"
    };

    ids.reserve(paths.size());
    for (auto& p : paths) {
        GLuint tex = loadTexture(p.c_str());
        if (tex == 0) std::cerr << "Failed texture: " << p << "\n";
        ids.push_back(tex);
    }
    std::cout << "Loaded " << ids.size() << " textures.\n";
}

GLuint TextureManager::get(size_t i) {
    if (i >= ids.size()) i = 0;
    return ids[i];
}
