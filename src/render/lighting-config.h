// C:\important\mimita-priv-v8\src\render\lighting-config.h
// 6 4 2026
/*
purpose
lighting config for basic.frag and basic.vert
bc its like nvidia propreoietoeary format with no commenting etc so its easier to jst do it here 
*/

#pragma once

#include <glm/glm.hpp>

struct LightingConfig
{
    glm::vec3 lightDir =
        glm::normalize(glm::vec3(-0.35f, -0.1f, -1.0f));

    float ambientStrength   = 0.72f;
    float diffuseStrength   = 0.45f;

    float edgeDarkness      = 0.10f;
    float edgeWidth         = 1.2f;

    float aoDarkness        = 0.05f;
    float aoContrast        = 0.8f;

    float textureContrast   = 1.02f;
    float textureBrightness = 1.35f;
};

extern LightingConfig gLighting;