#include "network/disagreement-visuals.h"
#include "effects/effect-part.h"
#include "audio/audio.h"
#include "debug/debug-log.h"

#include <cstdio>
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
        default: return "unknown";
    }
}

static glm::vec3 reasonColor(DisagreementReason reason)
{
    switch (reason)
    {
        case DISAGREEMENT_OCCLUDED_SHOT:  return glm::vec3(0.0f, 0.5f, 0.5f);
        case DISAGREEMENT_INVALID_DAMAGE: return glm::vec3(0.5f, 0.0f, 0.5f);
        case DISAGREEMENT_POSITION_CORRECTION: return glm::vec3(0.0f, 0.6f, 0.4f);
        case DISAGREEMENT_INVALID_MOVEMENT: return glm::vec3(0.4f, 0.4f, 0.0f);
        case DISAGREEMENT_INVALID_STATE: return glm::vec3(0.5f, 0.2f, 0.0f);
        default: return glm::vec3(0.0f, 0.5f, 0.5f);
    }
}

void spawnDisagreementEffect(const DisagreementEvent& event)
{
    const glm::vec3 color = reasonColor(event.reason);
    const std::string label = std::string("server disagreement: ") + reasonName(event.reason);

    EffectPartSystem::instance().spawnCustom(
        event.position, color, event.lifetime, label.c_str());

    EffectPartSystem::instance().spawnTracer(
        event.position,
        event.position + event.correction,
        "server_disagreement");

    playWorldSound("disagreement", event.position, 0.6f, 1.0f, 40.0f);

    if (gDisagreementDebug)
    {
        printf("[DISAGREEMENT] reason=%s pos=(%.2f,%.2f,%.2f) "
               "correction=(%.2f,%.2f,%.2f) desc=\"%s\"\n",
               reasonName(event.reason),
               event.position.x, event.position.y, event.position.z,
               event.correction.x, event.correction.y, event.correction.z,
               event.description.c_str());
    }
}

void logDisagreement(const DisagreementEvent& event)
{
    if (gDisagreementDebug)
    {
        Debug::warn(Debug::Category::NpcCombat,
            "[NET DISAGREEMENT] %s at (%.1f,%.1f,%.1f) correction=(%.1f,%.1f,%.1f) %s\n",
            reasonName(event.reason),
            event.position.x, event.position.y, event.position.z,
            event.correction.x, event.correction.y, event.correction.z,
            event.description.c_str());
    }
}

} // namespace MimitaNet
