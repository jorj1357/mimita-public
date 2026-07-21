// 07 21 2026, 10 54
/* purpose
* Declares the shared walking, gravity, jump, impulse, and special movement kernel.
* Exposes pure movement helpers so legacy Player wrappers and shared state reuse one formula.
* Keeps the collision phase represented as explicit feedback instead of owning collision.
* Does NOT implement collision sweeps, contact generation, or universal contact reset migration.
* Does NOT include Player, server, renderer, audio, weapon, packet, or authority logic.
* Does NOT replace the complete local physics orchestrator.
*/

#pragma once

#include <glm/glm.hpp>

#include "physics/movement/movement-types.h"

struct MovementCollisionFeedback {
    bool onGround = false;
    bool hasWorldContact = false;
    bool realWorldContactThisFrame = false;
    glm::vec3 groundNormal{0.0f, 0.0f, 1.0f};
};

float movementClampStepDelta(float dt, const MovementConfig& config);
float movementSafeSizeScale(float sizeScale);
float movementSizeScaleFactor(float sizeScale, float exponent);
float movementScaledGroundSpeed(const MovementConfig& config, float sizeScale);
float movementScaledAirSpeed(const MovementConfig& config, float sizeScale);
float movementScaledJumpVelocity(const MovementConfig& config, float sizeScale);
float movementScaledJumpVelocity(float jumpVelocity, float sizeScale, float jumpHeightExponent);
float movementDashImpulse(const MovementConfig& config);
bool movementHasMoveInput(glm::vec2 axes, float epsilon = 0.001f);
glm::vec2 movementHorizontalForwardFromYaw(float yawDegrees);
glm::vec2 movementDashDirection(const MovementCommand& command,
                                float epsilon = MOVEMENT_INPUT_EPSILON);
bool movementDirectionsEquivalent(glm::vec2 a,
                                  glm::vec2 b,
                                  float epsilon = MOVEMENT_INPUT_EPSILON);

float movementApplyGravityZ(float velocityZ,
                            float gravityZ,
                            float maximumFallSpeed,
                            float dt);

glm::vec2 movementWalkVelocityXY(glm::vec2 currentVelocity,
                                 glm::vec2 wishMoveXY,
                                 bool onGround,
                                 float sizeScale,
                                 const MovementConfig& config);

void movementApplyWalkVelocity(glm::vec3& baseVelocity,
                               glm::vec2 wishMoveXY,
                               bool onGround,
                               float sizeScale,
                               float groundSpeed,
                               float airSpeed,
                               float movementSpeedSizeExponent);

glm::vec2 movementApplyBaseFrictionXY(glm::vec2 velocityXY,
                                      bool onGround,
                                      bool hasMoveInput,
                                      float sizeScale,
                                      float frictionOverride,
                                      const MovementConfig& config,
                                      float dt);

glm::vec2 movementApplyBaseFrictionXY(glm::vec2 velocityXY,
                                      bool onGround,
                                      bool hasMoveInput,
                                      float sizeScale,
                                      float frictionOverride,
                                      float groundFrictionAmount,
                                      float airFrictionAmount,
                                      float frictionSizeExponent,
                                      float almostZeroSpeed,
                                      float dt);

void movementDecayAndClampExternalImpulse(glm::vec3& externalImpulse,
                                          const MovementConfig& config,
                                          float frictionOverride,
                                          float dt);

void movementDecayAndClampExternalImpulse(glm::vec3& externalImpulse,
                                          float externalImpulseDecay,
                                          float frictionOverride,
                                          float maximumExternalImpulseSpeed,
                                          float almostZeroSpeed,
                                          float dt);

bool movementCanGroundJump(const MovementState& state);
bool movementCanAirJump(const MovementState& state);
float freezeHorizontalPassThrough(const MovementFreezeState& freeze,
                                  const MovementConfig& config);
glm::vec2 effectiveHorizontalVelocity(const MovementState& state,
                                      float horizontalPassThrough);
MovementVelocityView movementVelocityViewForCollision(const MovementState& state,
                                                      const MovementConfig& config);
glm::vec3 calculateEffectiveMovementVelocity(const MovementState& state,
                                             const MovementConfig& config);
void restoreSpecialAbilityAvailability(MovementState& state);
void resetSpecialMovementLifecycleState(MovementState& state);
bool shouldWalkingOverwriteDashMomentum(const MovementState& state,
                                        const MovementCommand& command,
                                        float epsilon = MOVEMENT_INPUT_EPSILON);
void updateDashMomentumProtectionForWalk(MovementState& state,
                                         const MovementCommand& command,
                                         float epsilon = MOVEMENT_INPUT_EPSILON);
bool tryActivateDash(MovementState& state,
                     const MovementCommand& command,
                     const MovementConfig& config,
                     MovementStepEvents& events);
bool tryActivateDownDash(MovementState& state,
                         const MovementCommand& command,
                         const MovementConfig& config,
                         MovementStepEvents& events);
void updateFreeze(MovementState& state,
                  const MovementCommand& command,
                  const MovementConfig& config,
                  float fixedDt,
                  MovementStepEvents& events);

void applyBasicGravity(MovementState& state,
                       const MovementConfig& config,
                       float fixedDt);

void applyBasicExternalImpulseControl(MovementState& state,
                                      const MovementCommand& command);

void applyBasicWalk(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config);

void applyBasicJump(MovementState& state,
                    const MovementCommand& command,
                    const MovementConfig& config,
                    float fixedDt,
                    MovementStepEvents* events = nullptr);

void applyBasicFriction(MovementState& state,
                        const MovementConfig& config,
                        float fixedDt);

void applyPreCollisionBasicMovement(MovementState& state,
                                    const MovementCommand& command,
                                    const MovementConfig& config,
                                    float fixedDt);

void applySpecialMovementPreCollision(MovementState& state,
                                      const MovementCommand& command,
                                      const MovementConfig& config,
                                      float fixedDt,
                                      MovementStepEvents& events);

void applySpecialMovementPostCollision(MovementState& state,
                                       const MovementCommand& command,
                                       const MovementConfig& config,
                                       float fixedDt,
                                       MovementStepEvents& events);

MovementStepResult applyPostCollisionBasicMovement(MovementState& state,
                                                   const MovementCommand& command,
                                                   const MovementConfig& config,
                                                   const MovementCollisionFeedback& collision,
                                                   float fixedDt);

MovementStepResult applyPostCollisionMovementWithSpecials(
    MovementState& state,
    const MovementCommand& command,
    const MovementConfig& config,
    const MovementCollisionFeedback& collision,
    float fixedDt,
    MovementStepEvents preCollisionEvents = MovementStepEvents{});

MovementStepResult simulateBasicMovementStep(MovementState& state,
                                             const MovementCommand& command,
                                             const MovementConfig& config,
                                             const MovementCollisionFeedback& collision,
                                             float fixedDt);

MovementStepResult simulateMovementStepWithSpecials(MovementState& state,
                                                    const MovementCommand& command,
                                                    const MovementConfig& config,
                                                    const MovementCollisionFeedback& collision,
                                                    float fixedDt);
