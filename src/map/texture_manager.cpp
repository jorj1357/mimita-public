// C:\important\go away v5\s\mimita-v5\src\map\texture_manager.cpp

#include "texture_manager.h"
#include "texture.h"
#include <iostream>

/*
nov62025
todo
i dont think we use this cuz its hardcoding and we know hard coding is bad
*/
void TextureManager::init() {
    // jan 5 2026 wtf is wrong fix 1
    
    printf("TextureManager::ok what the heck going wrong here \n");
    printf("TextureManager::init START\n");
    fflush(stdout);
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
    printf("TextureManager::we loadd all thetextures now we are gonna ids reserve \n");

    ids.reserve(paths.size());
        printf("TextureManager::ids reserver sucessful  \n");

    for (auto& p : paths) {
        printf("TextureManager::for auto paths start \n");

        GLuint tex = loadTexture(p.c_str());
        if (tex == 0) std::cerr << "Failed texture: " << p << "\n";
        ids.push_back(tex);
                printf("TextureManager::for auto paths loop it finisehd  \n");

    }
    printf("TextureManager::about to print ids size and std cout idk textuers  \n");

    std::cout << "Loaded " << ids.size() << " textures.\n";
        printf("TextureManager::ok now we do gluint texturemanager:get \n");

}


GLuint TextureManager::get(size_t i) {
    printf("TextureManager: texturemanager:get attempting \n");

    if (i >= ids.size()) i = 0;
    printf("TextureManager: texturemanager:get attempting 2 \n");

    return ids[i];
    printf("TextureManager: texturemanager:get is done i tink cuz return  \n");

}
