#include "replay-commands.h"
#include "terminal-state.h"

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "replay/replay-export.h"
#include "replay/replay-factory.h"
#include "replay/replay-editor.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "devtools/terminal.h"
#include "config/player-settings.h"
#include "render/lighting-config.h"

void registerReplayCommands()
{
    registerReplayPlaybackCommands();
    registerReplayExportCommands();
    registerReplayEditorCommands();

    Terminal::instance().registerCommand({
        "replay.record", "Start replay recording", "replay.record",
        [](const std::vector<std::string>&) {
            if (REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("[REPLAY] Already recording");
                return;
            }
            if (ACTIVE_MAP_PATH.empty()) {
                Terminal::instance().addLog("[REPLAY] No active map is loaded");
                return;
            }
            REPLAY_RECORDER.beginRecording(0.0f, "mimita");

            const std::string mapPath = ACTIVE_MAP_PATH;
            const std::string playerPath = "assets/entity/player/default/mimita-char-no-animations-v4.glb";
            const std::string revolverPath = "assets/objects/weapons/mimita-revolver-v1.glb";
            REPLAY_RECORDER.registerAsset("map:active", "map_glb", mapPath, {}, "basic", "world");
            REPLAY_RECORDER.registerAsset("model:player", "actor_glb", playerPath, {}, "basic", "player");
            REPLAY_RECORDER.registerAsset("model:revolver", "weapon_glb", revolverPath, {}, "basic", "weapon");
            const std::string shotgunPath = "assets/objects/weapons/mimita-shotgun-v1.glb";
            REPLAY_RECORDER.registerAsset("model:shotgun", "weapon_glb", shotgunPath, {}, "basic", "weapon");
            REPLAY_RECORDER.registerAsset("texture:outfit", "texture", GetPlayerSettings().outfitPath, {}, {}, "outfit");
            ReplayWorldMetadata replayWorld;
            replayWorld.mapAssetId = "map:active";
            replayWorld.mapPath = mapPath;
            for (const Mesh::Batch& batch : THE_WORLD.mesh.batches) {
                const std::string materialName = batch.materialName.empty() ? "default" : batch.materialName;
                bool alreadyRegistered = false;
                for (const ReplayMaterialReference& material : replayWorld.materials) {
                    if (material.materialName == materialName) {
                        alreadyRegistered = true;
                        break;
                    }
                }
                if (!alreadyRegistered)
                    replayWorld.materials.push_back({materialName, "", "basic"});
            }
            REPLAY_RECORDER.setWorldMetadata(replayWorld);

            ReplayLightingState replayLighting;
            auto& lc = LightingConfig::instance();
            replayLighting.directionalLight = lc.lightDir();
            replayLighting.ambientStrength = lc.ambientStrength();
            replayLighting.diffuseStrength = lc.diffuseStrength();
            replayLighting.edgeDarkness = lc.edgeDarkness();
            replayLighting.edgeWidth = lc.edgeWidth();
            replayLighting.aoDarkness = lc.aoDarkness();
            replayLighting.aoContrast = lc.aoContrast();
            replayLighting.textureContrast = lc.textureContrast();
            replayLighting.textureBrightness = lc.textureBrightness();
            REPLAY_RECORDER.setLighting(replayLighting);

            Terminal::instance().addLog("[REPLAY] Recording started");
        }
    });

    Terminal::instance().registerCommand({
        "replay.load", "Load replay file", "replay.load <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: replay.load <path>");
                return;
            }
            std::string path = args[0];
            bool ok = REPLAY_PLAYER.loadFromJSON(path);
            Terminal::instance().addLog(ok ? "[REPLAY] Loaded " + path : "[ERROR] Failed to load " + path);
        }
    });

    Terminal::instance().registerCommand({
        "rplload", "Load replay by index (1=newest) or filename", "rplload <index|filename>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[ERROR] Usage: rplload <index|filename>");
                return;
            }
            std::string path;
            char* end = nullptr;
            long idx = std::strtol(args[0].c_str(), &end, 10);
            if (end != args[0].c_str() && idx > 0 && *end == '\0') {
                std::vector<std::string> clips = listReplayClips();
                size_t index = (size_t)(idx - 1);
                if (index >= clips.size()) {
                    char buf[128];
                    snprintf(buf, sizeof(buf),
                        "[ERROR] Replay #%ld does not exist. Available replay count: %zu",
                        idx, clips.size());
                    Terminal::instance().addLog(buf);
                    return;
                }
                path = clips[index];
                char buf[256];
                snprintf(buf, sizeof(buf), "Loading replay #%ld\nReplay: %s",
                    idx, std::filesystem::path(path).filename().string().c_str());
                Terminal::instance().addLog(buf);
            } else {
                path = args[0];
            }
            bool ok = REPLAY_PLAYER.loadFromJSON(path);
            Terminal::instance().addLog(ok ? "[REPLAY] Loaded successfully." : "[ERROR] Failed to load " + path);
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rpllatest", "Load the newest replay (shortcut for rplload 1)", "rpllatest",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("rplload 1");
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "rpllist", "List replays with indices (1=newest)", "rpllist",
        [](const std::vector<std::string>&) {
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[REPLAY] no replays found");
                return;
            }
            Terminal::instance().addLog("[REPLAY] available replays:");
            for (size_t i = 0; i < clips.size(); ++i) {
                char buf[256];
                snprintf(buf, sizeof(buf), "  %zu  %s",
                    i + 1, std::filesystem::path(clips[i]).filename().string().c_str());
                Terminal::instance().addLog(buf);
            }
        }
    }, "2026-07-02", CommandCategory::Replay);

    Terminal::instance().registerCommand({
        "replay.info", "Show replay info", "replay.info",
        [](const std::vector<std::string>&) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Recording: %d  Playback: %d  Ticks: %u",
                     (int)REPLAY_RECORDER.isRecording(), (int)REPLAY_PLAYER.isPlaying(),
                     REPLAY_PLAYER.totalTicks());
            Terminal::instance().addLog(buf);
        }
    });

    Terminal::instance().registerCommand({
        "replay_list", "List saved replays newest first (optionally with index)", "replay.list",
        [](const std::vector<std::string>&) {
            REPLAY_CLIPS_CACHE = listReplayClips();
            if (REPLAY_CLIPS_CACHE.empty()) {
                Terminal::instance().addLog("[REPLAY] no saved replays");
                return;
            }
            for (size_t i = 0; i < REPLAY_CLIPS_CACHE.size(); ++i) {
                char buf[512];
                snprintf(buf, sizeof(buf), "[REPLAY] %zu. %s", i + 1,
                         REPLAY_CLIPS_CACHE[i].c_str());
                Terminal::instance().addLog(buf);
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_browser", "Toggle replay browser overlay", "replay_browser",
        [](const std::vector<std::string>&) {
            REPLAY_BROWSER.toggle();
            if (REPLAY_BROWSER.isOpen())
                REPLAY_BROWSER.refresh();
        }
    });

    Terminal::instance().registerCommand({
        "replay_save_last_kill", "Save five seconds before and three seconds after the last kill",
        "replay_save_last_kill",
        [](const std::vector<std::string>&) {
            std::string path;
            if (!REPLAY_FACTORY.saveLastKill(&path)) {
                Terminal::instance().addLog(
                    "[ERROR] no captured kill is available to save");
                return;
            }
            Terminal::instance().addLog("[REPLAY] saved clip " + path);
        }
    });

    Terminal::instance().registerCommand({
        "replay_save_instant", "Save the last ~60 seconds as an instant replay file",
        "replay_save_instant",
        [](const std::vector<std::string>&) {
            if (!REPLAY_RECORDER.isRecording()) {
                Terminal::instance().addLog("[ERROR] No replay recording active");
                return;
            }
            std::string path = generateInstantReplayPath();
            if (!REPLAY_RECORDER.exportToJSON(path)) {
                Terminal::instance().addLog("[ERROR] Failed to save instant replay");
                return;
            }
            size_t frameCount = REPLAY_RECORDER.frames().size();
            size_t sceneCount = REPLAY_RECORDER.sceneFrames().size();
            float duration = (float)frameCount / 60.0f;
            char buf[128];
            Terminal::instance().addLog("[REPLAY] Saved instant replay");
            snprintf(buf, sizeof(buf), "[REPLAY] Frames saved: %zu", frameCount);
            Terminal::instance().addLog(buf);
            snprintf(buf, sizeof(buf), "[REPLAY] Scene frames: %zu", sceneCount);
            Terminal::instance().addLog(buf);
            snprintf(buf, sizeof(buf), "[REPLAY] Duration: %.1f sec", duration);
            Terminal::instance().addLog(buf);
            Terminal::instance().addLog("[REPLAY] File: " + path);
            printf("[REPLAY] Saved instant replay: %s  frames=%zu  duration=%.1fs\n",
                   path.c_str(), frameCount, duration);
        },
        std::string(), CommandCategory::Uncategorized, {"rpls"}
    });

    Terminal::instance().registerCommand({
        "replay_watch_instant", "Load and watch the most recent instant replay",
        "replay_watch_instant",
        [](const std::vector<std::string>&) {
            Terminal::instance().addLog("[REPLAY] Loading latest replay...");
            std::vector<std::string> clips = listReplayClips();
            if (clips.empty()) {
                Terminal::instance().addLog("[ERROR] No replays found");
                return;
            }
            {
                std::string path = clips.front();
                if (!REPLAY_PLAYER.loadFromJSON(path)) {
                    Terminal::instance().addLog("[ERROR] failed to load replay: " + path);
                    return;
                }
                REPLAY_PLAYER.preloadAssets();
                REPLAY_PLAYER.beginPlayback();
                REPLAY_PLAYER.pause();

                uint32_t tickCount = REPLAY_PLAYER.totalTicks();
                float duration = (float)tickCount / 60.0f;
                char buf[128];
                snprintf(buf, sizeof(buf), "[REPLAY] Loaded replay  Frames: %u  Duration: %.1f sec",
                         tickCount, duration);
                Terminal::instance().addLog(buf);
                printf("[REPLAY] Loaded replay: %s  ticks=%u\n",
                       path.c_str(), tickCount);
            }
        }
    });

    Terminal::instance().registerCommand({
        "replay_exit", "Safe exit from replay, clear all replay resources", "replay_exit",
        [](const std::vector<std::string>&) {
            printf("[REPLAY] replay_exit called\n");
            if (REPLAY_PLAYER.isPlaying()) {
                REPLAY_PLAYER.stopPlayback();
                printf("[REPLAY] playback stopped\n");
            }
            if (!REPLAY_ACTOR_MODELS.empty()) {
                REPLAY_ACTOR_MODELS.clear();
                printf("[REPLAY] actor models cleared\n");
            }
            if (!REPLAY_WEAPON_MODELS.empty()) {
                REPLAY_WEAPON_MODELS.clear();
                printf("[REPLAY] weapon models cleared\n");
            }
            if (!REPLAY_CHAT_STATES.empty()) {
                REPLAY_CHAT_STATES.clear();
                printf("[REPLAY] chat states cleared\n");
            }
            Terminal::instance().addLog("[REPLAY] all replay resources cleaned");
        }
    });

    Terminal::instance().registerCommand({
        "replay_state", "Print current replay state info", "replay_state",
        [](const std::vector<std::string>&) {
            char buf[512];
            const ReplayPlayer& p = REPLAY_PLAYER;
            snprintf(buf, sizeof(buf),
                "ReplayState: %s\nGameState: %s\nLoaded: %s\nFrame: %u/%u\n"
                "Playing: %s Paused: %s Duration: %.1fs\nActors: %zu Weapons: %zu",
                p.isPlaying() ? "WatchingReplay" : GAME_STATE == GAME_MENU ? "Menu" : "None",
                GAME_STATE == GAME_PLAYING ? "PLAYING" : GAME_STATE == GAME_MENU ? "MENU" : "PAUSED",
                p.totalTicks() > 0 ? "yes" : "no",
                p.currentTick(), p.totalTicks(),
                p.isPlaying() ? "yes" : "no",
                p.isPaused() ? "yes" : "no",
                p.totalTicks() > 0 ? (float)p.totalTicks() / 60.0f : 0.0f,
                (unsigned long)REPLAY_ACTOR_MODELS.size(),
                (unsigned long)REPLAY_WEAPON_MODELS.size());
            Terminal::instance().addLog(buf);
            printf("[REPLAY STATE] %s\n", buf);
        }
    });

    Terminal::instance().registerCommand({
        "replay_debug", "Print all replay resource debug info", "replay_debug",
        [](const std::vector<std::string>&) {
            const ReplayPlayer& p = REPLAY_PLAYER;
            printf("[REPLAY DEBUG] isPlaying=%d isPaused=%d currentTick=%u totalTicks=%u timescale=%.2f\n",
                   (int)p.isPlaying(), (int)p.isPaused(),
                   p.currentTick(), p.totalTicks(), p.timescale());
            printf("[REPLAY DEBUG] actorModels=%zu weaponModels=%zu chatStates=%zu\n",
                   REPLAY_ACTOR_MODELS.size(), REPLAY_WEAPON_MODELS.size(),
                   REPLAY_CHAT_STATES.size());
            printf("[REPLAY DEBUG] recording=%d\n",
                   (int)REPLAY_RECORDER.isRecording());
            printf("[REPLAY DEBUG] replayExportActive=%d\n",
                   (int)isReplayExportActive());
            Terminal::instance().addLog("[REPLAY] debug info printed to console");
        }
    });

    Terminal::instance().registerCommand({
        "healthbar_audit", "Audit healthbar state: count alive/dead/orphaned entries", "healthbar_audit",
        [](const std::vector<std::string>&) {
            uint32_t total = 0, alive = 0, dead = 0;
            for (const auto& kv : REPLAY_ACTOR_MODELS) {
                total++;
                if (kv.second && kv.second->dead) dead++;
                else alive++;
            }
            printf("[HEALTHBAR AUDIT]\n");
            printf("  replayActorModels=%u\n", total);
            printf("  alive=%u\n", alive);
            printf("  dead=%u\n", dead);
            if (const ReplaySceneFrame* frame = REPLAY_PLAYER.currentSceneFrame()) {
                printf("  sceneFrameActors=%zu\n", frame->actors.size());
                size_t frameAlive = 0, frameDead = 0;
                for (const auto& a : frame->actors) {
                    if (a.dead) frameDead++;
                    else frameAlive++;
                }
                printf("  frameAlive=%zu frameDead=%zu\n", frameAlive, frameDead);
            } else {
                printf("  sceneFrameActors=0 (no current frame)\n");
            }
            printf("  replayExportActive=%d\n", (int)isReplayExportActive());
            Terminal::instance().addLog("[HEALTHBAR] audit printed to console");
        }
    });

    Terminal::instance().registerCommand({
        "replay_hitmarker_reload", "Reload config/audio/replay-hitmarkers.json",
        "replay_hitmarker_reload",
        [](const std::vector<std::string>&) {
            pollReplayHitmarkerConfig();
            Terminal::instance().addLog("[REPLAY HITMARKER] config reloaded");
        }
    });

    Terminal::instance().registerCommand({
        "replay_audio_debug", "Print replay export audio config", "replay_audio_debug",
        [](const std::vector<std::string>&) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "audioVolumeMultiplier=%.2f\n"
                     "configLoaded=1\n"
                     "configPath=config/replay/replay-export.json",
                     getReplayExportAudioVolume());
            Terminal::instance().addLog(buf);
        }
    });

    // Short command aliases
    Terminal::instance().registerCommand({
        "rplload", "Load replay", "rplload <path>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("Usage: rplload <path>"); return; }
            Terminal::instance().execute("replay.load " + args[0]);
        }
    });
    Terminal::instance().registerCommand({
        "rplplay", "Play loaded replay", "rplplay",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("replay.play");
        }
    });
    Terminal::instance().registerCommand({
        "rplpause", "Pause replay playback", "rplpause",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("replay_pause");
        }
    });
    Terminal::instance().registerCommand({
        "rplstop", "Stop replay playback", "rplstop",
        [](const std::vector<std::string>&) {
            Terminal::instance().execute("replay.stop");
        }
    });
    Terminal::instance().registerCommand({
        "rplf", "Skip forward N seconds", "rplf <seconds>",
        [](const std::vector<std::string>& args) {
            int sec = args.empty() ? 1 : std::atoi(args[0].c_str());
            if (sec < 1) sec = 1;
            char buf[64];
            snprintf(buf, sizeof(buf), "replay_forward_1s");
            for (int i = 0; i < sec; i++)
                Terminal::instance().execute(buf);
        }
    });
    Terminal::instance().registerCommand({
        "rplb", "Skip backward N seconds", "rplb <seconds>",
        [](const std::vector<std::string>& args) {
            int sec = args.empty() ? 1 : std::atoi(args[0].c_str());
            if (sec < 1) sec = 1;
            char buf[64];
            snprintf(buf, sizeof(buf), "replay_rewind_1s");
            for (int i = 0; i < sec; i++)
                Terminal::instance().execute(buf);
        }
    });
    Terminal::instance().registerCommand({
        "rpltick", "Jump to tick", "rpltick <tick>",
        [](const std::vector<std::string>& args) {
            if (args.empty()) { Terminal::instance().addLog("Usage: rpltick <tick>"); return; }
            Terminal::instance().execute("replay_seek_tick " + args[0]);
        }
    });
}
