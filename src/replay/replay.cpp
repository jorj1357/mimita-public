#include "replay.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================
// ReplayRecorder
// ============================================================

void ReplayRecorder::beginRecording(float randomSeed, const char* mapName) {
    mFrames.clear();
    mTick = 0;
    mRecording = true;

    mHeader = {};
    std::memcpy(mHeader.magic, "MIRPLAY", 8);
    mHeader.version = 1;
    mHeader.tickRate = 60;
    mHeader.randomSeed = randomSeed;
    mHeader.timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();
    if (mapName) {
        std::strncpy(mHeader.mapName, mapName, sizeof(mHeader.mapName) - 1);
    }
    std::strncpy(mHeader.playerName, "player", sizeof(mHeader.playerName) - 1);

    printf("[REPLAY] Recording started  map=%s seed=%.1f\n",
           mHeader.mapName, mHeader.randomSeed);
}

void ReplayRecorder::recordFrame(const InputFrame& frame) {
    if (!mRecording) return;

    ReplayFrame rf;
    rf.tick = mTick++;
    rf.inputs = frame;
    mFrames.push_back(rf);
}

void ReplayRecorder::stopRecording() {
    if (!mRecording) return;
    mRecording = false;
    mHeader.tickCount = (uint32_t)mFrames.size();
    printf("[REPLAY] Recording stopped  ticks=%u\n", mHeader.tickCount);
}

bool ReplayRecorder::exportToJSON(const std::string& path) const {
    json j;

    // Header
    j["header"]["version"] = mHeader.version;
    j["header"]["tickCount"] = mHeader.tickCount;
    j["header"]["tickRate"] = mHeader.tickRate;
    j["header"]["randomSeed"] = mHeader.randomSeed;
    j["header"]["mapName"] = std::string(mHeader.mapName);
    j["header"]["timestamp"] = mHeader.timestamp;
    j["header"]["playerName"] = std::string(mHeader.playerName);

    // Frames
    json framesJson = json::array();
    for (const auto& rf : mFrames) {
        json f;
        f["tick"] = rf.tick;
        f["moveX"] = rf.inputs.moveX;
        f["moveY"] = rf.inputs.moveY;
        f["jump"] = rf.inputs.jump;
        f["jumpPressed"] = rf.inputs.jumpPressed;
        f["dashPressed"] = rf.inputs.dashPressed;
        f["groundReturnPressed"] = rf.inputs.groundReturnPressed;
        f["freezeHeld"] = rf.inputs.freezeHeld;
        // ADD THESE 6 7 2026 
        f["movementPressed"] = rf.inputs.movementPressed;
        f["reloadPressed"] = rf.inputs.reloadPressed;
        f["lookYaw"] = rf.inputs.lookYaw;
        f["lookPitch"] = rf.inputs.lookPitch;
        framesJson.push_back(f);
    }
    j["frames"] = framesJson;

    std::ofstream file(path);
    if (!file.is_open()) {
        printf("[REPLAY] Failed to write %s\n", path.c_str());
        return false;
    }
    file << j.dump(2);
    printf("[REPLAY] Exported %u frames to %s\n", mHeader.tickCount, path.c_str());
    return true;
}

bool ReplayRecorder::exportToBinary(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        printf("[REPLAY] Failed to write %s\n", path.c_str());
        return false;
    }

    ReplayHeader header = mHeader;
    header.tickCount = (uint32_t)mFrames.size();
    file.write((const char*)&header, sizeof(header));

    for (const auto& rf : mFrames) {
        file.write((const char*)&rf, sizeof(rf));
    }

    printf("[REPLAY] Exported %u frames (binary) to %s\n", header.tickCount, path.c_str());
    return true;
}

// ============================================================
// ReplayPlayer
// ============================================================

bool ReplayPlayer::loadFromJSON(const std::string& path) {
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
            // ADD THESE
            rf.inputs.movementPressed = f.value("movementPressed", false);
            rf.inputs.reloadPressed = f.value("reloadPressed", false);
            rf.inputs.lookYaw = f.value("lookYaw", 0.0f);
            rf.inputs.lookPitch = f.value("lookPitch", 0.0f);
            mFrames.push_back(rf);
        }

        printf("[REPLAY] Loaded %zu frames from %s\n", mFrames.size(), path.c_str());
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

void ReplayPlayer::beginPlayback() {
    mPlaying = true;
    mCurrentTick = 0;
    printf("[REPLAY] Playback started  ticks=%u\n", mHeader.tickCount);
}

void ReplayPlayer::stopPlayback() {
    mPlaying = false;
    printf("[REPLAY] Playback stopped at tick %u\n", mCurrentTick);
}

bool ReplayPlayer::getFrameAt(uint32_t tick, InputFrame& out) const {
    if (tick >= mFrames.size()) return false;
    out = mFrames[tick].inputs;
    return true;
}

const InputFrame* ReplayPlayer::advanceTick() {
    if (!mPlaying || mCurrentTick >= mFrames.size()) {
        mPlaying = false;
        return nullptr;
    }
    const InputFrame* frame = &mFrames[mCurrentTick].inputs;
    mCurrentTick++;
    return frame;
}

void ReplayPlayer::seekToTick(uint32_t tick) {
    mCurrentTick = std::min(tick, (uint32_t)mFrames.size());
}
