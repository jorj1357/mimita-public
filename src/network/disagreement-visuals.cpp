#include "network/disagreement-visuals.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <glm/glm.hpp>

namespace MimitaNet {

bool gDisagreementDebug = false;

static const char* reasonName(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return "occluded shot";
        case DISAGREEMENT_INVALID_DAMAGE: return "invalid damage";
        case DISAGREEMENT_POSITION_CORRECTION: return "position correction";
        case DISAGREEMENT_INVALID_MOVEMENT: return "invalid movement";
        case DISAGREEMENT_INVALID_STATE: return "invalid state";
        case DISAGREEMENT_REWIND_MISS:   return "rewind miss";
        case DISAGREEMENT_TARGET_NOT_FOUND: return "target not found";
        case DISAGREEMENT_TARGET_DEAD:   return "target dead";
        case DISAGREEMENT_SELF_TARGET:   return "self target";
        default: return "unknown";
    }
}

static const char* reasonLabel(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return "OCCLUDED SHOT";
        case DISAGREEMENT_INVALID_DAMAGE: return "INVALID DAMAGE";
        case DISAGREEMENT_POSITION_CORRECTION: return "POSITION CORRECTION";
        case DISAGREEMENT_INVALID_MOVEMENT: return "INVALID MOVEMENT";
        case DISAGREEMENT_INVALID_STATE: return "INVALID STATE";
        case DISAGREEMENT_REWIND_MISS:   return "REWIND MISS";
        case DISAGREEMENT_TARGET_NOT_FOUND: return "TARGET NOT FOUND";
        case DISAGREEMENT_TARGET_DEAD:   return "TARGET DEAD";
        case DISAGREEMENT_SELF_TARGET:   return "SELF TARGET";
        default: return "SERVER CORRECTION";
    }
}

static glm::vec3 reasonColor(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return glm::vec3(0.0f, 0.9f, 0.9f);
        case DISAGREEMENT_INVALID_DAMAGE: return glm::vec3(0.0f, 0.7f, 1.0f);
        case DISAGREEMENT_POSITION_CORRECTION: return glm::vec3(0.0f, 1.0f, 0.8f);
        case DISAGREEMENT_INVALID_MOVEMENT: return glm::vec3(0.2f, 0.8f, 1.0f);
        case DISAGREEMENT_INVALID_STATE: return glm::vec3(0.3f, 0.9f, 0.9f);
        case DISAGREEMENT_REWIND_MISS:   return glm::vec3(0.0f, 0.8f, 1.0f);
        case DISAGREEMENT_TARGET_NOT_FOUND: return glm::vec3(0.2f, 0.6f, 0.9f);
        case DISAGREEMENT_TARGET_DEAD:   return glm::vec3(0.3f, 0.7f, 0.8f);
        case DISAGREEMENT_SELF_TARGET:   return glm::vec3(0.1f, 0.5f, 0.8f);
        default: return glm::vec3(0.0f, 0.8f, 0.8f);
    }
}

static float correctionMagnitude(const DisagreementEvent& event)
{
    return glm::length(event.correction);
}

static bool isHitRejection(DisagreementReason reason)
{
    return reason == DISAGREEMENT_OCCLUDED_SHOT ||
           reason == DISAGREEMENT_REWIND_MISS ||
           reason == DISAGREEMENT_TARGET_NOT_FOUND ||
           reason == DISAGREEMENT_TARGET_DEAD ||
           reason == DISAGREEMENT_SELF_TARGET ||
           reason == DISAGREEMENT_INVALID_DAMAGE;
}

static float severityRadius(const DisagreementEvent& event)
{
    float mag = correctionMagnitude(event);
    float radius = 0.8f;
    if (mag > 1.0f) radius = 3.0f;
    else if (mag > 0.25f) radius = 1.8f;
    // For hit-rejection reasons, force a large minimum radius
    if (isHitRejection(event.reason))
        radius = std::max(radius, 4.0f);
    return radius;
}

static float severityLifetime(const DisagreementEvent& event)
{
    float mag = correctionMagnitude(event);
    float lifetime = 0.6f;
    if (mag > 1.0f) lifetime = 1.5f;
    else if (mag > 0.25f) lifetime = 1.0f;
    // For hit-rejection reasons, force a longer minimum lifetime
    if (isHitRejection(event.reason))
        lifetime = std::max(lifetime, 1.5f);
    return lifetime;
}

void spawnDisagreementEffect(const DisagreementEvent& event)
{
    const glm::vec3 color = reasonColor(event.reason);
    float radius = severityRadius(event);
    float lifetime = severityLifetime(event);
    const glm::vec3 pos = event.position;

    // Volume scaling: small = 0.5, large = 0.9
    float vol = 0.5f + 0.4f * (radius / 3.0f);

    // Pitch variation: deterministic from position hash
    unsigned int h = (unsigned int)(pos.x * 73856093)
        ^ (unsigned int)(pos.y * 19349663)
        ^ (unsigned int)(pos.z * 83492791);
    float pitch = 1.0f + ((float)(h % 101) - 50.0f) / 1000.0f;

    // Play world-space sound, audible up to 150 units (matches extended visual range)
    playWorldSound("serverdisagree", pos, vol, pitch, 150.0f);

    // ── Expanding sphere pulse ─────────────────────────────────────
    {
        EffectPart e;
        e.position = pos;
        e.color = color;
        e.maxLifetime = lifetime;
        e.scale = 0.1f;
        e.endScale = radius;
        e.billboardText = false;
        e.alpha = 0.6f;
        e.replayType = "server_disagreement_pulse";
        EffectPartSystem::instance().spawn(e);
    }

    // ── Vertical cyan beam ─────────────────────────────────────────
    {
        EffectPart e;
        e.position = pos;
        e.endPosition = pos + glm::vec3(0, 0, radius * 1.3f);
        e.color = color;
        e.maxLifetime = lifetime * 0.6f;
        e.scale = 0.08f;
        e.endScale = 0.02f;
        e.alpha = 0.4f;
        e.beam = true;
        e.replayType = "server_disagreement_beam";
        EffectPartSystem::instance().spawn(e);
    }

    // ── Correction tracer (origin → correction target) ─────────────
    if (glm::length(event.correction) > 0.01f)
    {
        EffectPart e;
        e.position = pos;
        e.endPosition = pos + event.correction;
        e.color = glm::vec3(0.0f, 1.0f, 1.0f);
        e.maxLifetime = 0.4f;
        e.scale = 0.06f;
        e.endScale = 0.0f;
        e.thickness = 0.06f;
        e.endThickness = 0.0f;
        e.alpha = 0.8f;
        e.beam = true;
        e.replayType = "server_disagreement_tracer";
        EffectPartSystem::instance().spawn(e);
    }

    // ── Floating description text ──────────────────────────────────
    {
        EffectPart e;
        e.position = pos + glm::vec3(0, 0, 0.5f);
        e.color = color;
        e.maxLifetime = lifetime + 0.3f;
        // Show the actual description from the server, or fall back to reason label
        if (!event.description.empty())
            e.label = "SERVER REJECTED HIT: " + event.description;
        else
            e.label = reasonLabel(event.reason);
        e.billboardText = true;
        e.scale = 0.3f;
        e.replayType = "server_disagreement_text";
        EffectPartSystem::instance().spawn(e);
    }

    // ── Particle burst (8 small cyan spheres) ──────────────────────
    {
        for (int i = 0; i < 8; ++i)
        {
            unsigned int si = h + (unsigned int)i * 769u;
            float angle = (float)(si % 6283) / 1000.0f;
            float elevation = ((float)((si * 311u) % 1000) / 1000.0f - 0.5f) * 3.14f;
            float speed = 1.0f + ((float)((si * 503u) % 500) / 500.0f) * 2.0f;
            glm::vec3 dir(
                std::cos(angle) * std::cos(elevation),
                std::sin(angle) * std::cos(elevation),
                std::sin(elevation)
            );
            EffectPart e;
            e.position = pos;
            e.velocity = dir * speed;
            e.color = color;
            e.maxLifetime = 0.5f + ((float)((si * 211u) % 200) / 200.0f) * 0.3f;
            e.scale = 0.04f;
            e.endScale = 0.08f;
            e.billboardText = false;
            e.alpha = 0.7f;
            e.gravity = 2.0f;
            e.affectedByGravity = true;
            e.replayType = "server_disagreement_particle";
            EffectPartSystem::instance().spawn(e);
        }
    }

    // Always log at warn level so it's visible in console
    Debug::warn(Debug::Category::Networking,
        "[SERVER DISAGREEMENT] eventId=%u shotSerial=%u shooter=%u target=%u "
        "reason=%s radius=%.1f lifetime=%.1f sound=serverdisagree "
        "pos=(%.1f,%.1f,%.1f) correction=(%.2f,%.2f,%.2f) mag=%.2f desc=\"%s\"\n",
        event.eventId, event.relatedSerial, event.sourcePlayerId, event.targetPlayerId,
        reasonName(event.reason),
        radius, lifetime,
        pos.x, pos.y, pos.z,
        event.correction.x, event.correction.y, event.correction.z,
        correctionMagnitude(event),
        event.description.c_str());
}

void logDisagreement(const DisagreementEvent& event)
{
    if (gDisagreementDebug)
    {
        Debug::warn(Debug::Category::Networking,
            "[NET DISAGREEMENT] %s at (%.1f,%.1f,%.1f) correction=(%.1f,%.1f,%.1f) %s\n",
            reasonName(event.reason),
            event.position.x, event.position.y, event.position.z,
            event.correction.x, event.correction.y, event.correction.z,
            event.description.c_str());
    }
}

void spawnLocalDisagreementIndicator(const DisagreementEvent& event)
{
    const glm::vec3 color(0.0f, 0.6f, 1.0f);

    // Correction arrow from predicted to corrected
    EffectPart tracer;
    tracer.position = event.position;
    tracer.endPosition = event.position + event.correction;
    tracer.color = color;
    tracer.maxLifetime = 1.2f;
    tracer.scale = 0.04f;
    tracer.endScale = 0.0f;
    tracer.thickness = 0.04f;
    tracer.endThickness = 0.0f;
    tracer.alpha = 0.9f;
    tracer.beam = true;
    tracer.replayType = "local_disagreement_arrow";
    EffectPartSystem::instance().spawn(tracer);

    // Local correction text (only visible to corrected player)
    EffectPart text;
    text.position = event.position + glm::vec3(0, 0, 0.3f);
    text.color = color;
    text.maxLifetime = 1.2f;
    text.label = "CORRECTED";
    text.billboardText = true;
    text.scale = 0.25f;
    text.replayType = "local_disagreement_text";
    EffectPartSystem::instance().spawn(text);

    Debug::warn(Debug::Category::Networking,
        "[LOCAL CORRECTION] corrected=(%.1f,%.1f,%.1f) offset=(%.1f,%.1f,%.1f)\n",
        event.position.x + event.correction.x,
        event.position.y + event.correction.y,
        event.position.z + event.correction.z,
        event.correction.x, event.correction.y, event.correction.z);
}

} // namespace MimitaNet
