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
        "hitfx_debug", "Toggle hit effect debug panel",
        "hitfx_debug [0|1]",
        [](const std::vector<std::string>& args) {
            static bool debugEnabled = false;
            debugEnabled = args.empty() ? !debugEnabled : args[0] != "0";
            if (debugEnabled) {
                const auto& cfg = HitEffects::config();
                char buf[1024];
                std::snprintf(buf, sizeof(buf),
                    "[HITFX] bursts=%d spheres=%zu particles(enabled=%d count=%d) "
                    "elongated(enabled=%d) disc(enabled=%d) lifetime=%d useBlood=%d",
                    HitEffects::debugBurstCount(),
                    cfg.sphereTimeline.size(),
                    (int)cfg.particles.enabled, cfg.particles.count,
                    (int)cfg.elongatedSphere.enabled,
                    (int)cfg.impactDisc.enabled,
                    cfg.core.lifetimeTicks,
                    (int)cfg.core.useBlood);
                Terminal::instance().addLog(buf);
                Debug::log(Debug::Category::NpcCombat, "%s", buf);
            }
            Terminal::instance().addLog(std::string("[FX] hitfx_debug=") + (debugEnabled ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "hitfx_test", "Spawn test hit effects 3m in front of camera",
        "hitfx_test [damage]",
        [](const std::vector<std::string>& args) {
            int damage = 25;
            if (!args.empty()) damage = std::atoi(args[0].c_str());
            if (damage < 1) damage = 25;
            glm::vec3 hitPoint;
            glm::vec3 hitDir(0.0f, 0.0f, 1.0f);
            glm::vec3 hitNormal(0.0f, 0.0f, 1.0f);
            if (gpPlayer && gpCamera) {
                hitPoint = gpPlayer->pos + gpCamera->front * 3.0f;
                hitDir = gpCamera->front;
                hitNormal = -gpCamera->front;
            } else {
                hitPoint = glm::vec3(0.0f, 0.0f, 5.0f);
            }
            HitEffects::spawnHitEffects(hitPoint, hitDir, hitNormal, damage, "test", "test");
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

    Terminal::instance().registerCommand({
        "hitfx_info", "Print current parsed hitfx config values",
        "hitfx_info",
        [](const std::vector<std::string>&) {
            const auto& cfg = HitEffects::config();
            char buf[2048];
            int pos = 0;
            pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                "[HITFX] enabled=%d hotReload=%d lifetime=%d useBlood=%d",
                (int)cfg.enabled, (int)cfg.hotReload, cfg.core.lifetimeTicks, (int)cfg.core.useBlood);
            pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                " | spheres=%zu", cfg.sphereTimeline.size());
            for (size_t i = 0; i < cfg.sphereTimeline.size() && i < 3; ++i) {
                const auto& s = cfg.sphereTimeline[i];
                pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                    " [%s: ticks=%d-%d radius=%.2f-%.2f alpha=%.2f-%.2f]",
                    s.name.c_str(), s.startTick, s.endTick,
                    s.startRadius, s.endRadius, s.alphaStart, s.alphaEnd);
            }
            pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                " | particles(enabled=%d count=%d cone=%.0f speed=%.1f)",
                (int)cfg.particles.enabled, cfg.particles.count,
                cfg.particles.coneAngleDegrees, cfg.particles.speed);
            pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                " | elongated(enabled=%d ticks=%d-%d length=%.2f-%.2f)",
                (int)cfg.elongatedSphere.enabled,
                cfg.elongatedSphere.startTick, cfg.elongatedSphere.endTick,
                cfg.elongatedSphere.lengthStart, cfg.elongatedSphere.lengthEnd);
            pos += std::snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                " | disc(enabled=%d ticks=%d-%d radius=%.2f-%.2f)",
                (int)cfg.impactDisc.enabled,
                cfg.impactDisc.startTick, cfg.impactDisc.endTick,
                cfg.impactDisc.radiusStart, cfg.impactDisc.radiusEnd);
            Terminal::instance().addLog(buf);
        }
    });
}
