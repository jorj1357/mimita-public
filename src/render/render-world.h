// C:\important\quiet\n\mimita-priv-v7\src\render\render-world.h
// feb 10 2026
/**
 * prupose 
 * small wrapper so that
 * renderer can cal?
 * idk
 * keepinng file size low rewrite
 */

#pragma once

#include <glm/glm.hpp>

struct World;
class Camera;

extern bool gWorldTextureDebug;

void renderWorld(const World& world, const Camera& cam);
void renderWorldDepth(const World& world, GLuint shadowShader, const glm::mat4& lightMVP);
void setWorldSolidRedDebug(bool enabled);
bool worldSolidRedDebug();
void registerWorldTextureCommands();
