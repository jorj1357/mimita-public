#include "player-nameplates.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "entities/player.h"
#include "gui/ui-system.h"
#include "physics/config.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"

namespace {

constexpr float MAX_HEALTHBAR_DISTANCE = 50.0f;
constexpr float FADE_DISTANCE = 60.0f;

} // namespace

glm::vec3 playerHealthbarAnchor(
    const Player& player,
    bool* usedHeadTransform)
{
    if (usedHeadTransform)
        *usedHeadTransform = false;

    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        if (part.name != "head")
            continue;

        const glm::vec3 localTop{
            (part.collider.localMin.x + part.collider.localMax.x) * 0.5f,
            (part.collider.localMin.y + part.collider.localMax.y) * 0.5f,
            part.collider.localMax.z
        };
        const glm::vec3 headTop =
            glm::vec3(part.worldTransform * glm::vec4(localTop, 1.0f));
        if (usedHeadTransform)
            *usedHeadTransform = true;
        return headTop + glm::vec3(0.0f, 0.0f, 0.3f);
    }

    return player.pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.72f);
}

static int gTotalHealthbarsRendered = 0;
static int gTotalLiveWorldHealthbars = 0;
static int gTotalInvalidHealthbars = 0;

void resetHealthbarCounters()
{
    gTotalHealthbarsRendered = 0;
    gTotalLiveWorldHealthbars = 0;
    gTotalInvalidHealthbars = 0;
}

int getHealthbarTotal() { return gTotalHealthbarsRendered; }
int getHealthbarLiveWorld() { return gTotalLiveWorldHealthbars; }
int getHealthbarInvalid() { return gTotalInvalidHealthbars; }

HealthbarRenderResult drawPlayerHealthbar(
    const Player& player,
    const Camera& camera,
    const char* debugPrefix,
    const char* sourceTag)
{
    HealthbarRenderResult result;
    result.anchor = playerHealthbarAnchor(
        player, &result.usedHeadTransform);

    bool isLiveWorld = (std::strcmp(sourceTag, "live_world") == 0);

    if (player.dead || player.currentHp <= 0)
    {
        Debug::log(Debug::Category::General, "[HEALTHBAR] skipped render owner=%s dead=%d hp=%d", player.username.c_str(), (int)player.dead, player.currentHp);
        RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=1 pos=(%.2f %.2f %.2f) rendered=NO reason=dead actor\n",
                  (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
                  player.currentHp, player.maxHp,
                  result.anchor.x, result.anchor.y, result.anchor.z);
        result.cullReason = HealthbarCullReason::Dead;
        if (isLiveWorld) gTotalLiveWorldHealthbars++;
        gTotalInvalidHealthbars++;
        return result;
    }

    result.distance = glm::length(camera.pos - result.anchor);
    if (result.distance > MAX_HEALTHBAR_DISTANCE)
    {
        RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=0 pos=(%.2f %.2f %.2f) rendered=NO reason=too far\n",
                  (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
                  player.currentHp, player.maxHp,
                  result.anchor.x, result.anchor.y, result.anchor.z);
        result.cullReason = HealthbarCullReason::TooFar;
        return result;
    }

    if (!DebugVis::projectToScreen(
            camera, result.anchor, result.screen.x, result.screen.y))
    {
        RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=0 pos=(%.2f %.2f %.2f) rendered=NO reason=offscreen\n",
                  (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
                  player.currentHp, player.maxHp,
                  result.anchor.x, result.anchor.y, result.anchor.z);
        result.cullReason = HealthbarCullReason::Offscreen;
        return result;
    }

    const float fade = std::clamp(
        1.0f - result.distance / FADE_DISTANCE, 0.25f, 1.0f);
    const float ratio = player.maxHp > 0
        ? std::clamp(
            (float)player.currentHp / (float)player.maxHp, 0.0f, 1.0f)
        : 0.0f;
    const float x = result.screen.x;
    const float y = result.screen.y;
    const std::string prefix = debugPrefix ? debugPrefix : "entity-hp";
    const std::string backgroundName = prefix + "-bg";
    const std::string currentName = prefix + "-current";

    uiDrawRect(
        {x - 65.0f, y - 8.0f, 130.0f, 11.0f},
        {0.55f, 0.03f, 0.03f, 0.9f * fade},
        backgroundName.c_str());
    uiDrawRect(
        {x - 65.0f, y - 8.0f, 130.0f * ratio, 11.0f},
        {0.05f, 0.8f, 0.15f, 0.9f * fade},
        currentName.c_str());

    const float nameWidth = uiMeasureText(player.username.c_str(), 0.28f);
    uiDrawText(
        player.username.c_str(), x - nameWidth * 0.5f, y - 31.0f,
        0.28f, {1.0f, 1.0f, 1.0f, fade});

    char healthText[48];
    snprintf(
        healthText, sizeof(healthText), "%d/%d",
        player.currentHp, player.maxHp);
    const float healthWidth = uiMeasureText(healthText, 0.25f);
    uiDrawText(
        healthText, x - healthWidth * 0.5f, y + 8.0f,
        0.25f, {1.0f, 1.0f, 1.0f, fade});

    result.rendered = true;
    gTotalHealthbarsRendered++;
    if (isLiveWorld) gTotalLiveWorldHealthbars++;
    RPLXDEBUG("[HEALTHBAR] frame=? tick=? entity=%d name=%s source=%s hp=%d/%d dead=0 pos=(%.2f %.2f %.2f) rendered=YES reason=visible\n",
              (int)(uintptr_t)&player, player.username.c_str(), sourceTag,
              player.currentHp, player.maxHp,
              result.anchor.x, result.anchor.y, result.anchor.z);
    return result;
}

const char* healthbarCullReasonName(HealthbarCullReason reason)
{
    switch (reason)
    {
    case HealthbarCullReason::None: return "none";
    case HealthbarCullReason::Dead: return "dead";
    case HealthbarCullReason::TooFar: return "too-far";
    case HealthbarCullReason::Offscreen: return "offscreen";
    }
    return "unknown";
}
