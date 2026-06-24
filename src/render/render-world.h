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
class Terminal; // forward decl for dump functions

extern bool gWorldTextureDebug;
extern bool gRenderBackfaces;

void renderSky(const World& world, const Camera& cam);
void renderWorld(const World& world, const Camera& cam);
void renderWorldDepth(const World& world, GLuint shadowShader, const glm::mat4& lightMVP);
void setWorldSolidRedDebug(bool enabled);
bool worldSolidRedDebug();
void registerWorldTextureCommands();
void setUniforms(GLuint shader);
void dumpGLBMaterials(Terminal& t);
void dumpGLBTextures(Terminal& t);
void dumpGLBLights(Terminal& t);
void validateGLB(Terminal& t);
