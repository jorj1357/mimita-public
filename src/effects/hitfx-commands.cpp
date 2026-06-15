#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "effects/effect-part.h"
#include "devtools/terminal.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "camera.h"

extern Player* gpPlayer;
extern Camera* gpCamera;

void registerHitFxCommands()
{
    Terminal::instance().registerCommand({
        "bloodfx", "Toggle blood/gore effects (0=off, 1=on). Contact sphere, damage numbers, and hit burst unaffected.",
        "bloodfx [0|1]",
        [](const std::vector<std::string>& args) {
            gBloodFXEnabled = args.empty() ? !gBloodFXEnabled : args[0] != "0";
            Terminal::instance().addLog(std::string("[FX] bloodfx=") + (gBloodFXEnabled ? "1" : "0"));
            Debug::log(Debug::Category::NpcCombat, "[FX] bloodfx=%s", gBloodFXEnabled ? "ON" : "OFF");
        }
    });

    Terminal::instance().registerCommand({
        "hitfx_debug", "Toggle hit effect debug panel showing active config values",
        "hitfx_debug [0|1]",
        [](const std::vector<std::string>& args) {
            static bool debugEnabled = false;
            debugEnabled = args.empty() ? !debugEnabled : args[0] != "0";
            if (debugEnabled) {
                char buf[512];
                std::snprintf(buf, sizeof(buf),
                    "[HITFX] bursts=%d | WhiteStar(spikes=%d ticks=%d) BlueBurst(ticks=%d) "
                    "Cone(particles=%d angle=%.0f) Disc(radius=%.1f) Oval(length=%.1f)",
                    HitEffects::debugBurstCount(),
                    HitEffects::config().whiteStar.spikeCount, HitEffects::config().whiteStar.ticks,
                    HitEffects::config().blueBurst.ticks,
                    HitEffects::config().cone.particleCount, HitEffects::config().cone.coneAngleDegrees,
                    HitEffects::config().disc.endRadius, HitEffects::config().oval.endLength);
                Terminal::instance().addLog(buf);
                Debug::log(Debug::Category::NpcCombat, "%s", buf);
            }
            Terminal::instance().addLog(std::string("[FX] hitfx_debug=") + (debugEnabled ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "hitfx_test", "Spawn test hit effects at camera aim point",
        "hitfx_test [damage]",
        [](const std::vector<std::string>& args) {
            int damage = 25;
            if (!args.empty()) damage = std::atoi(args[0].c_str());
            if (damage < 1) damage = 25;
            glm::vec3 hitPoint;
            glm::vec3 hitNormal(0.0f, 0.0f, 1.0f);
            if (gpPlayer && gpCamera) {
                hitPoint = gpPlayer->pos + gpCamera->front * 3.0f;
                hitNormal = -gpCamera->front;
            } else {
                hitPoint = glm::vec3(0.0f, 0.0f, 5.0f);
            }
            HitEffects::spawnHitEffects(hitPoint, hitNormal, damage, "test", "test");
            Terminal::instance().addLog(std::string("[FX] hitfx_test spawned damage=") +
                std::to_string(damage) + " at (" +
                std::to_string((int)hitPoint.x) + " " +
                std::to_string((int)hitPoint.y) + " " +
                std::to_string((int)hitPoint.z) + ")");
        }
    });

    Terminal::instance().registerCommand({
        "hitfx_reload", "Force reload hitfx.json config",
        "hitfx_reload",
        [](const std::vector<std::string>&) {
            HitEffects::loadConfig("config/hitfx.json");
            Terminal::instance().addLog("[HITFX] config reloaded");
        }
    });
}
