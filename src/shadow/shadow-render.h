#pragma once

#include <cstdint>

#include <glm/glm.hpp>

struct World;
namespace mimita::game::render { class ShaderProgram; }

void initShadowMap(int mapSize);
void renderShadowMap(const World& world, const glm::vec3& focusPoint);
void bindShadowMap(int textureUnit);
void updateShadowMatrix(const glm::vec3& lightDir, float distance, const glm::vec3& focusPoint, int mapSize, bool stabilize);
const glm::mat4& shadowMatrix();
std::uint32_t shadowDepthTex();

void setShowShadowMap(bool v);
bool showShadowMap();
void renderShadowMapOverlay(int screenW, int screenH);

// Per-category depth rendering (called from renderShadowMap or standalone)
void renderPlayerDepth(const class Player& player, mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj);
void renderNpcDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj);
void renderEffectDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj);
void renderParticleDepths(mimita::game::render::ShaderProgram& shadowShader, const glm::mat4& lightViewProj);
