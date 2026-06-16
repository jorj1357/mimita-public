#include "effects/hitfx-commands.h"
#include "effects/hit-effects.h"
#include "effects/effect-part.h"
#include "devtools/terminal.h"
#include "debug/debug-log.h"
#include "config/player-settings.h"
#include "entities/player.h"
#include "camera.h"

extern Player* gpPlayer;
extern Camera* gpCamera;

void registerHitFxCommands()
{
    Terminal::instance().registerCommand({
        "bloodfx", "Toggle blood effects (0=off, 1=on). Default off.",
        "bloodfx [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gBloodFXEnabled
                    ? "[BLOODFX] enabled"
                    : "[BLOODFX] disabled");
                return;
            }
            gBloodFXEnabled = args[0] != "0";
            GetPlayerSettings().bloodFX = gBloodFXEnabled;
            SavePlayerSettings();
            Debug::log(Debug::Category::NpcCombat, "[BLOODFX] %s",
                gBloodFXEnabled ? "Enabled" : "Disabled");
            Terminal::instance().addLog(gBloodFXEnabled
                ? "[BLOODFX] enabled"
                : "[BLOODFX] disabled");
        }
    });
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
                    "elongated(enabled=%d) disc(enabled=%d) lifetime=%d useBlood=%d "
                    "legacySphere=%s",
                    HitEffects::debugBurstCount(),
                    cfg.sphereTimeline.size(),
                    (int)cfg.particles.enabled, cfg.particles.count,
                    (int)cfg.elongatedSphere.enabled,
                    (int)cfg.impactDisc.enabled,
                    cfg.core.lifetimeTicks,
                    (int)cfg.core.useBlood,
                    cfg.legacyContactSphere.enabled ? "ON" : "OFF");
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
                "[HITFX] enabled=%d hotReload=%d lifetime=%d useBlood=%d dmgNum=%d entImpact=%d worldImpact=%d bulletImpact=%d",
                (int)cfg.enabled, (int)cfg.hotReload, cfg.core.lifetimeTicks, (int)cfg.core.useBlood,
                (int)cfg.core.damageNumbers, (int)cfg.core.entityImpact, (int)cfg.core.worldImpact, (int)cfg.core.bulletImpact);
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
            const auto& l = cfg.legacyContactSphere;
            std::snprintf(buf, sizeof(buf),
                "Legacy Contact Sphere\n"
                "  enabled: %s\n"
                "  color: [%.2f, %.2f, %.2f]\n"
                "  alpha: %.2f\n"
                "  radius: %.2f -> %.2f\n"
                "  lifetime: %.2f",
                l.enabled ? "true" : "false",
                l.color.x, l.color.y, l.color.z,
                l.alpha,
                l.startRadius, l.endRadius,
                l.lifetimeSeconds);
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "hitfx_trace", "Trace every effect spawn to source file", "hitfx_trace [0|1]",
        [](const std::vector<std::string>& args) {
            bool on = args.empty() ? !gHitFxTraceEnabled : (args[0] != "0");
            gHitFxTraceEnabled = on;
            Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] %s\n", on ? "enabled" : "disabled");
            Terminal::instance().addLog(std::string("[HITFX] trace=") + (on ? "1" : "0"));
        }
    });

    Terminal::instance().registerCommand({
        "dashfx", "Toggle ground dash burst effect (0=off, 1=on). Default on.",
        "dashfx [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog(gDashFXEnabled
                    ? "[DASHFX] enabled"
                    : "[DASHFX] disabled");
                return;
            }
            gDashFXEnabled = args[0] != "0";
            Debug::log(Debug::Category::NpcCombat, "[DASHFX] %s",
                gDashFXEnabled ? "Enabled" : "Disabled");
            Terminal::instance().addLog(gDashFXEnabled
                ? "[DASHFX] enabled"
                : "[DASHFX] disabled");
        }
    });

    Terminal::instance().registerCommand({
        "dashfx_test", "Spawn dash burst at player feet",
        "dashfx_test",
        [](const std::vector<std::string>&) {
            if (!gpPlayer) {
                Terminal::instance().addLog("[DASHFX] no player");
                return;
            }
            glm::vec3 dir(1.0f, 0.0f, 0.0f);
            if (glm::length(gpPlayer->vel) > 0.001f)
                dir = glm::normalize(glm::vec3(gpPlayer->vel.x, gpPlayer->vel.y, 0.0f));
            HitEffects::spawnMovementDashBurst(gpPlayer->pos, dir, glm::length(gpPlayer->vel));
            Terminal::instance().addLog("[DASHFX] test burst spawned");
        }
    });

    // Death ellipsoid commands
    Terminal::instance().registerCommand({
        "deathfx_ellipsoid", "Enable/disable death ellipsoid effect (0=off, 1=on)",
        "deathfx_ellipsoid [0|1]",
        [](const std::vector<std::string>& args) {
            auto& deCfg = HitEffects::mutableConfig().deathEllipsoid;
            if (args.empty()) {
                Terminal::instance().addLog(deCfg.enabled
                    ? "[DEATHFX] ellipsoid enabled"
                    : "[DEATHFX] ellipsoid disabled");
                return;
            }
            deCfg.enabled = args[0] != "0";
            Terminal::instance().addLog(deCfg.enabled
                ? "[DEATHFX] ellipsoid enabled"
                : "[DEATHFX] ellipsoid disabled");
        }
    });

    Terminal::instance().registerCommand({
        "deathfx_ellipsoid_length", "Set death ellipsoid length",
        "deathfx_ellipsoid_length <value>",
        [](const std::vector<std::string>& args) {
            auto& deCfg = HitEffects::mutableConfig().deathEllipsoid;
            if (args.empty()) {
                Terminal::instance().addLog("[DEATHFX] length=" + std::to_string(deCfg.length));
                return;
            }
            deCfg.length = std::max(1.0f, std::stof(args[0]));
            Terminal::instance().addLog("[DEATHFX] length set to " + std::to_string(deCfg.length));
        }
    });

    Terminal::instance().registerCommand({
        "deathfx_ellipsoid_radius", "Set death ellipsoid radius",
        "deathfx_ellipsoid_radius <value>",
        [](const std::vector<std::string>& args) {
            auto& deCfg = HitEffects::mutableConfig().deathEllipsoid;
            if (args.empty()) {
                Terminal::instance().addLog("[DEATHFX] radius=" + std::to_string(deCfg.radius));
                return;
            }
            deCfg.radius = std::max(0.1f, std::stof(args[0]));
            Terminal::instance().addLog("[DEATHFX] radius set to " + std::to_string(deCfg.radius));
        }
    });

    Terminal::instance().registerCommand({
        "deathfx_ellipsoid_lifetime", "Set death ellipsoid lifetime in seconds",
        "deathfx_ellipsoid_lifetime <value>",
        [](const std::vector<std::string>& args) {
            auto& deCfg = HitEffects::mutableConfig().deathEllipsoid;
            if (args.empty()) {
                Terminal::instance().addLog("[DEATHFX] lifetime=" + std::to_string(deCfg.lifetime));
                return;
            }
            deCfg.lifetime = std::max(0.1f, std::stof(args[0]));
            Terminal::instance().addLog("[DEATHFX] lifetime set to " + std::to_string(deCfg.lifetime));
        }
    });

    // Test command: spawn a death ellipsoid at the player's position
    Terminal::instance().registerCommand({
        "deathfx_ellipsoid_test", "Spawn test death ellipsoid at player",
        "deathfx_ellipsoid_test",
        [](const std::vector<std::string>&) {
            if (!gpPlayer) {
                Terminal::instance().addLog("[DEATHFX] no player");
                return;
            }
            const auto& deCfg = HitEffects::config().deathEllipsoid;
            if (!deCfg.enabled) {
                Terminal::instance().addLog("[DEATHFX] ellipsoid disabled");
                return;
            }
            Camera* cam = gpCamera;
            glm::vec3 dir = cam ? cam->front : glm::vec3(1.0f, 0.0f, 0.0f);
            EffectPartSystem::instance().spawnDeathEllipsoid(
                gpPlayer->pos, dir,
                deCfg.length, deCfg.radius, deCfg.lifetime);
            Terminal::instance().addLog("[DEATHFX] test ellipsoid spawned");
        }
    });
}
