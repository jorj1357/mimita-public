#pragma once

// Bounded jitter investigation probe. Off by default; `jitter_probe <seconds>`
// enables per-category sampling and emits `jitter.*` verdict events. This is a
// diagnostic owner only; it never changes simulation, interpolation, or render
// state.

#include <cstdint>
#include <cstddef>

#include <glm/glm.hpp>

namespace JitterProbe {

bool active();
void start(double seconds);
void stop();

// Network interpolation sample (remote actor).
void sampleInterpolation(std::uint32_t actorId,
                         double sampleAlpha,
                         double sampleAlphaDelta,
                         float finalRenderDelta,
                         float finalVerticalDelta,
                         std::uint32_t hardSnaps,
                         std::uint32_t underruns,
                         std::uint32_t holds,
                         bool holding,
                         bool extrapolating,
                         bool sampleJumped,
                         std::size_t bufferDepth);

// Actor render pose sample. `interpPosition` is the rendered (interpolated)
// position; `rawPosition` is the fixed-tick pose. `dt` is the render delta.
void sampleActorPose(std::uint32_t actorId, const char* part,
                     const glm::vec3& interpPosition,
                     const glm::vec3& rawPosition,
                     float dt);

// Camera sample, in world space and Euler degrees.
void sampleCamera(const glm::vec3& position, const glm::vec3& euler, float dt);

// HUD sample: active crosshair and expected centre in HUD pixels.
void sampleHud(float x, float y, float expectedX, float expectedY,
               bool forcedCenter);

// Call once per rendered frame after all samples for that frame.
void endFrame();

}  // namespace JitterProbe
