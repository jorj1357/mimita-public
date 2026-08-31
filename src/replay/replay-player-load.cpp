#include "replay.h"
#include "replay-io.h"

#include <cstdio>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#include "entities/player.h"
#include "debug/debug-log.h"
#include "perf/perf-spike.h"

using json = nlohmann::json;

bool ReplayPlayer::loadClip(ReplayClip clip)
{
    mClip = std::move(clip);
    mHeader = mClip.header;
    mFrames = mClip.frames;
    mCurrentTick = 0;
    mPlaybackTick = 0.0f;
    mLastEventTick = -1;
    mOutfitPath.clear();
    rebuildInterpolatedFrameAtTick();
    return !mClip.sceneFrames.empty() || !mFrames.empty();
}

bool ReplayPlayer::loadFromJSON(const std::string& path) {
    printf("[REPLAY] loading clip from %s\n", path.c_str());
    ReplayClip clip;
    if (clip.load(path)) {
        loadClip(std::move(clip));
        printf("[REPLAY] clip loaded path=%s sceneFrames=%zu frames=%zu header.tickCount=%u\n",
               path.c_str(), mClip.sceneFrames.size(), mFrames.size(), mHeader.tickCount);
        return true;
    }

    printf("[REPLAY] clip.load() returned false, trying JSON parse for %s\n", path.c_str());
    std::ifstream file(path);
    if (!file.is_open()) {
        printf("[REPLAY] Could not open %s\n", path.c_str());
        return false;
    }

    try {
        json j;
        file >> j;

        auto& h = j["header"];
        mHeader.version = h.value("version", 1);
        mHeader.tickCount = h.value("tickCount", 0);
        mHeader.tickRate = h.value("tickRate", 60);
        mHeader.randomSeed = h.value("randomSeed", 0.0f);
        std::string mapName = h.value("mapName", "");
        std::strncpy(mHeader.mapName, mapName.c_str(), sizeof(mHeader.mapName) - 1);
        mHeader.timestamp = h.value("timestamp", 0ULL);
        std::string playerName = h.value("playerName", "");
        std::strncpy(mHeader.playerName, playerName.c_str(), sizeof(mHeader.playerName) - 1);

        mAssets.clear();
        if (j.contains("assets")) {
            for (const auto& jsonAsset : j["assets"]) {
                ReplayAsset asset;
                asset.id = jsonAsset.value("id", "");
                asset.type = jsonAsset.value("type", "");
                asset.path = jsonAsset.value("path", "");
                asset.shaderName = jsonAsset.value("shader", "");
                asset.source = jsonAsset.value("source", "");
                mAssets.push_back(asset);
            }
        }

        mOutfitPath.clear();
        for (const ReplayAsset& asset : mAssets) {
            if (asset.id.find("outfit") != std::string::npos) {
                mOutfitPath = asset.path;
                printf("[REPLAY] Restoring outfit: %s\n", mOutfitPath.c_str());
                break;
            }
        }

        mFrames.clear();
        for (const auto& f : j["frames"]) {
            ReplayFrame rf;
            rf.tick = f.value("tick", 0);
            rf.inputs.moveX = f.value("moveX", 0.0f);
            rf.inputs.moveY = f.value("moveY", 0.0f);
            rf.inputs.jump = f.value("jump", false);
            rf.inputs.jumpPressed = f.value("jumpPressed", false);
            rf.inputs.dashPressed = f.value("dashPressed", false);
            rf.inputs.groundReturnPressed = f.value("groundReturnPressed", false);
            rf.inputs.freezeHeld = f.value("freezeHeld", false);
            rf.inputs.movementPressed = f.value("movementPressed", false);
            rf.inputs.reloadPressed = f.value("reloadPressed", false);
            rf.inputs.lookYaw = f.value("lookYaw", 0.0f);
            rf.inputs.lookPitch = f.value("lookPitch", 0.0f);
            mFrames.push_back(rf);
        }

        mClip.sceneFrames.clear();
        if (j.contains("sceneFrames")) {
            for (const auto& sf : j["sceneFrames"]) {
                ReplaySceneFrame frame;
                frame.tick = sf.value("tick", 0);
                frame.time = sf.value("time", 0.0f);
                frame.camera.position = {
                    sf["camera"].value("position", std::vector<float>{0,0,0})[0],
                    sf["camera"].value("position", std::vector<float>{0,0,0})[1],
                    sf["camera"].value("position", std::vector<float>{0,0,0})[2]
                };
                frame.camera.rotation = {
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[0],
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[1],
                    sf["camera"].value("rotation", std::vector<float>{0,0,0})[2]
                };
                frame.camera.fov = sf["camera"].value("fov", 70.0f);
                if (sf.contains("actors")) {
                    for (const auto& a : sf["actors"]) {
                        ReplayActorState actor;
                        actor.id = a.value("id", "");
                        actor.name = a.value("name", "");
                        actor.type = a.value("type", "");
                        actor.modelPath = a.value("modelPath", "");
                        actor.position = {
                            a.value("position", std::vector<float>{0,0,0})[0],
                            a.value("position", std::vector<float>{0,0,0})[1],
                            a.value("position", std::vector<float>{0,0,0})[2]
                        };
                        actor.rotation = {
                            a.value("rotation", std::vector<float>{0,0,0})[0],
                            a.value("rotation", std::vector<float>{0,0,0})[1],
                            a.value("rotation", std::vector<float>{0,0,0})[2]
                        };
                        actor.velocity = {
                            a.value("velocity", std::vector<float>{0,0,0})[0],
                            a.value("velocity", std::vector<float>{0,0,0})[1],
                            a.value("velocity", std::vector<float>{0,0,0})[2]
                        };
                        actor.health = a.value("health", 100);
                        actor.maxHealth = a.value("maxHealth", 100);
                        actor.currentAmmo = a.value("currentAmmo", 0);
                        actor.reserveAmmo = a.value("reserveAmmo", 0);
                        actor.dead = a.value("dead", false);
                        actor.outfitPath = a.value("outfitPath", "");
                        actor.characterName = a.value("characterName", "");
                        actor.avatarName = a.value("avatarName", "");
                        actor.weaponName = a.value("weaponName", "");
                        actor.weaponModelPath = a.value("weaponModelPath", "");
                        actor.shooting = a.value("shooting", false);
                        actor.reloading = a.value("reloading", false);
                        actor.grounded = a.value("grounded", true);
                        actor.sizeScale = a.value("sizeScale", 1.0f);
                        if (a.contains("bodyParts")) {
                            actor.bodyPartCount = 0;
                            for (auto& bp : a["bodyParts"].items()) {
                                if (actor.bodyPartCount >= ReplayActorState::MAX_BODY_PARTS) break;
                                ReplayBodyPartState& part = actor.bodyParts[actor.bodyPartCount];
                                part.partId = partIdFromName(bp.key().c_str());
                                if (bp.value().contains("position")) {
                                    auto& p = bp.value()["position"];
                                    part.position = {p[0].get<float>(), p[1].get<float>(), p[2].get<float>()};
                                }
                                if (bp.value().contains("rotation") && bp.value()["rotation"].is_array()) {
                                    auto& r = bp.value()["rotation"];
                                    if (r.size() >= 4)
                                        part.rotation = glm::quat(r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>());
                                }
                                if (bp.value().contains("scale")) {
                                    auto& s = bp.value()["scale"];
                                    part.scale = {s[0].get<float>(), s[1].get<float>(), s[2].get<float>()};
                                }
                                actor.bodyParts[actor.bodyPartCount] = part;
                                actor.bodyPartCount++;
                            }
                        }
                        frame.actors.push_back(actor);
                    }
                }
                if (sf.contains("effects")) {
                    for (const auto& e : sf["effects"]) {
                        ReplayEffectEvent ev = parseEffect(e);
                        Debug::log(Debug::Category::Replay,
                            "[REPLAY EFFECT] loaded type=%s tick=%d pos=(%.2f %.2f %.2f) scale=(%.2f %.2f %.2f) alpha=%.2f\n",
                            ev.type.c_str(), ev.spawnTick,
                            ev.position.x, ev.position.y, ev.position.z,
                            ev.scale.x, ev.scale.y, ev.scale.z,
                            ev.alpha);
                        frame.effects.push_back(std::move(ev));
                    }
                }
                mClip.sceneFrames.push_back(frame);
            }
            printf("[REPLAY] Loaded %zu scene frames\n", mClip.sceneFrames.size());
        }

        mClip.soundEvents.clear();
        if (j.contains("soundEvents")) {
            for (const auto& se : j["soundEvents"]) {
                ReplaySoundEvent sound;
                sound.tick = se.value("tick", 0);
                sound.soundPath = se.value("soundPath", "");
                sound.world = se.value("world", false);
                std::vector<float> pos = se.value("position", std::vector<float>{0,0,0});
                sound.position = {pos[0], pos[1], pos[2]};
                sound.volume = se.value("volume", 1.0f);
                sound.pitch = se.value("pitch", 1.0f);
                sound.maxDistance = se.value("maxDistance", 30.0f);
                if (se.contains("listenerPosition")) {
                    std::vector<float> lp = se["listenerPosition"].get<std::vector<float>>();
                    sound.listenerPosition = {lp[0], lp[1], lp[2]};
                    std::vector<float> lf = se.value("listenerForward", std::vector<float>{0.0f, 1.0f, 0.0f});
                    sound.listenerForward = {lf[0], lf[1], lf[2]};
                    sound.listenerValid = true;
                }
                mClip.soundEvents.push_back(sound);
            }
            Debug::warn(Debug::Category::Replay, "[REPLAY] Loaded %zu sound events from JSON (manual path)\n", mClip.soundEvents.size());
        }

        printf("[REPLAY] Loaded %zu frames from %s\n", mFrames.size(), path.c_str());
        rebuildInterpolatedFrameAtTick();
        return true;

    } catch (const std::exception& e) {
        printf("[REPLAY] Error loading %s: %s\n", path.c_str(), e.what());
        return false;
    }
}

bool ReplayPlayer::loadFromBinary(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("[REPLAY] Could not open %s\n", path.c_str());
        return false;
    }

    file.read((char*)&mHeader, sizeof(mHeader));
    if (file.gcount() != sizeof(mHeader)) {
        printf("[REPLAY] Invalid header in %s\n", path.c_str());
        return false;
    }

    mFrames.resize(mHeader.tickCount);
    for (uint32_t i = 0; i < mHeader.tickCount; ++i) {
        file.read((char*)&mFrames[i], sizeof(ReplayFrame));
    }

    printf("[REPLAY] Loaded %zu frames (binary) from %s\n", mFrames.size(), path.c_str());
    return true;
}
