#pragma once

#include <glm/glm.hpp>

struct World;

void initShadowMap(int mapSize);
void renderShadowMap(const World& world, const glm::vec3& focusPoint);
void bindShadowMap(int textureUnit);
void updateShadowMatrix(const glm::vec3& lightDir, float distance, const glm::vec3& focusPoint, int mapSize, bool stabilize);
const glm::mat4& shadowMatrix();
GLuint shadowDepthTex();

void setShowShadowMap(bool v);
bool showShadowMap();
void renderShadowMapOverlay(int screenW, int screenH);

// Per-category depth rendering (called from renderShadowMap or standalone)
void renderWorldDepth(const World& world, GLuint shadowShader, const glm::mat4& lightMVP);
void renderPlayerDepth(const class Player& player, GLuint shadowShader, const glm::mat4& lightViewProj);
void renderNpcDepths(GLuint shadowShader, const glm::mat4& lightViewProj);
void renderEffectDepths(GLuint shadowShader, const glm::mat4& lightViewProj);
void renderParticleDepths(GLuint shadowShader, const glm::mat4& lightViewProj);
