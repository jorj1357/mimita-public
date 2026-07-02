#include "replay-camera.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-log.h"

using json = nlohmann::json;

void ReplayCameraMgr::addKeyframe(int tick, const Camera& camera)
{
    CameraKeyframe kf;
    kf.tick = tick;
    kf.position = camera.pos;
    kf.yaw = camera.yaw;
    kf.pitch = camera.pitch;
    kf.fov = camera.fov;
    kf.mode = mMode;
    mKeyframes.push_back(kf);
    std::sort(mKeyframes.begin(), mKeyframes.end(),
        [](const CameraKeyframe& a, const CameraKeyframe& b) {
            return a.tick < b.tick;
        });
    Debug::log(Debug::Category::Replay,
        "[CAMERA TIMELINE] added keyframe at tick %d  pos=(%.2f %.2f %.2f) yaw=%.1f pitch=%.1f fov=%.1f mode=%s\n",
        tick, camera.pos.x, camera.pos.y, camera.pos.z,
        camera.yaw, camera.pitch, camera.fov, mMode.c_str());
}

bool ReplayCameraMgr::deleteKeyframe(int index)
{
    if (index < 0 || index >= (int)mKeyframes.size())
        return false;
    mKeyframes.erase(mKeyframes.begin() + index);
    Debug::log(Debug::Category::Replay,
        "[CAMERA TIMELINE] deleted keyframe at index %d\n", index);
    return true;
}

void ReplayCameraMgr::clearKeyframes()
{
    mKeyframes.clear();
    Debug::log(Debug::Category::Replay,
        "[CAMERA TIMELINE] all keyframes cleared\n");
}

bool ReplayCameraMgr::save(const std::string& path)
{
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path(), ec);

    json j;
    j["mode"] = mMode;
    json kfArr = json::array();
    for (const auto& kf : mKeyframes) {
        json kfJson;
        kfJson["tick"] = kf.tick;
        kfJson["pos"] = {kf.position.x, kf.position.y, kf.position.z};
        kfJson["yaw"] = kf.yaw;
        kfJson["pitch"] = kf.pitch;
        kfJson["fov"] = kf.fov;
        kfJson["mode"] = kf.mode;
        kfArr.push_back(kfJson);
    }
    j["keyframes"] = kfArr;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << j.dump(2) << '\n';
    file.close();

    Debug::log(Debug::Category::Replay,
        "[CAMERA TIMELINE] saved %zu keyframes to %s\n",
        mKeyframes.size(), path.c_str());
    return true;
}

bool ReplayCameraMgr::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;

        if (j.contains("mode"))
            mMode = j["mode"].get<std::string>();

        mKeyframes.clear();
        if (j.contains("keyframes")) {
            for (const auto& kfJson : j["keyframes"]) {
                CameraKeyframe kf;
                kf.tick = kfJson.value("tick", 0);
                if (kfJson.contains("pos") && kfJson["pos"].is_array() && kfJson["pos"].size() >= 3) {
                    kf.position.x = kfJson["pos"][0].get<float>();
                    kf.position.y = kfJson["pos"][1].get<float>();
                    kf.position.z = kfJson["pos"][2].get<float>();
                }
                kf.yaw = kfJson.value("yaw", 0.0f);
                kf.pitch = kfJson.value("pitch", 0.0f);
                kf.fov = kfJson.value("fov", 70.0f);
                kf.mode = kfJson.value("mode", std::string());
                mKeyframes.push_back(kf);
            }
        }

        Debug::log(Debug::Category::Replay,
            "[CAMERA TIMELINE] loaded %zu keyframes from %s (mode=%s)\n",
            mKeyframes.size(), path.c_str(), mMode.c_str());
        return true;
    } catch (...) {
        Debug::log(Debug::Category::Replay,
            "[CAMERA TIMELINE] failed to parse %s\n", path.c_str());
        return false;
    }
}

bool ReplayCameraMgr::lerpKeyframes(const CameraKeyframe& a, const CameraKeyframe& b,
                                     float t, Camera& camera)
{
    camera.pos = glm::mix(a.position, b.position, t);
    camera.yaw = glm::mix(a.yaw, b.yaw, t);
    camera.pitch = glm::mix(a.pitch, b.pitch, t);
    camera.fov = glm::mix(a.fov, b.fov, t);
    camera.updateVectors();
    return true;
}

void ReplayCameraMgr::applyModeChange(int tick)
{
    for (const auto& kf : mKeyframes) {
        if (kf.tick == tick && !kf.mode.empty()) {
            if (kf.mode != mMode) {
                Debug::log(Debug::Category::Replay,
                    "[CAMERA TIMELINE] mode switch at tick %d: %s -> %s\n",
                    tick, mMode.c_str(), kf.mode.c_str());
                mMode = kf.mode;
            }
        }
    }
}

void ReplayCameraMgr::update(int currentTick, Camera& camera, float dt)
{
    applyModeChange(currentTick);

    if (mMode != "keyframed")
        return;

    if (mKeyframes.size() < 2)
        return;

    int prevIdx = -1;
    int nextIdx = -1;

    for (int i = 0; i < (int)mKeyframes.size(); ++i) {
        if (mKeyframes[i].tick <= currentTick)
            prevIdx = i;
    }
    for (int i = (int)mKeyframes.size() - 1; i >= 0; --i) {
        if (mKeyframes[i].tick >= currentTick)
            nextIdx = i;
    }

    if (prevIdx == -1) {
        const auto& kf = mKeyframes.front();
        camera.pos = kf.position;
        camera.yaw = kf.yaw;
        camera.pitch = kf.pitch;
        camera.fov = kf.fov;
        camera.updateVectors();
        return;
    }

    if (nextIdx == -1 || prevIdx == nextIdx) {
        const auto& kf = mKeyframes[prevIdx];
        camera.pos = kf.position;
        camera.yaw = kf.yaw;
        camera.pitch = kf.pitch;
        camera.fov = kf.fov;
        camera.updateVectors();
        return;
    }

    const auto& a = mKeyframes[prevIdx];
    const auto& b = mKeyframes[nextIdx];
    float range = (float)(b.tick - a.tick);
    float t = range > 0.0f
        ? std::clamp((float)(currentTick - a.tick) / range, 0.0f, 1.0f)
        : 0.0f;

    lerpKeyframes(a, b, t, camera);
}
