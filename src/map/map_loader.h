// C:\important\go away v5\s\mimita-v5\src\map\map_loader.h

#pragma once
#include "map_common.h"
#include <string>
#include <vector>

struct GLBMaterialInfo {
    int index;
    std::string name;
    int baseColorTextureIndex;
    double baseColorFactor[4];
    bool hasTexture;
    bool hasColorFactor;
    bool hasKhrTextureTransform;
    double texTransformOffset[2];
    double texTransformScale[2];
};

struct GLBImageInfo {
    int index;
    std::string name;
    int width;
    int height;
    int components;
    bool embedded;
    std::string uri;
};

struct GLBLightInfo {
    std::string name;
    std::string type; // "point", "sun", "spot", "area"
    double color[3];
    double intensity;
    double range;
    double position[3];
    double direction[3];
    double innerConeAngle;
    double outerConeAngle;
};

struct GLBDebugData {
    std::vector<GLBMaterialInfo> materials;
    std::vector<GLBImageInfo> images;
    std::vector<GLBLightInfo> lights;
    int meshCount = 0;
    int totalPrimitives = 0;
    bool loaded = false;

    void clear();
};

extern GLBDebugData gGLBDebug;

Mesh loadOBJ(const std::string& path);
Mesh loadGLB(const std::string& path, bool storeDebugInfo = true);
void releaseMeshGLResources(Mesh& mesh);
