#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

class NpcSystem;

namespace MimitaNet {
struct ServerPlayer;
struct ServerNpc;
struct ServerProjectile;
struct HeadlessWorld;
}

// Owns the authoritative actor track captured beside a client scenario
// recording on the local dev-loop server. The client merges this sidecar into
// the final schema-2 scenario when recording stops.
class ScenarioServerRecorder {
public:
    static ScenarioServerRecorder& instance();

    bool start(const std::string& scenarioPath, std::string* error);
    bool startCanonical(const std::string& scenarioId,
                        const std::string& scenarioPath,
                        std::string* error);
    void stop(const char* reason);
    void captureTick(
        const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
        const std::unordered_map<uint32_t, MimitaNet::ServerNpc>& npcs,
        const NpcSystem& npcSystem,
        const std::unordered_map<uint32_t, MimitaNet::ServerProjectile>& projectiles,
        const MimitaNet::HeadlessWorld& world,
        uint32_t serverTick);
    void captureCheckpoint(
        uint32_t bookmarkNumber,
        uint32_t clientTick,
        const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
        const std::unordered_map<uint32_t, MimitaNet::ServerNpc>& npcs,
        const NpcSystem& npcSystem,
        const std::unordered_map<uint32_t, MimitaNet::ServerProjectile>& projectiles,
        uint32_t serverTick);

    bool active() const { return mActive; }
    bool canonical() const { return mCanonical; }
    bool captureInvalid() const { return mCaptureInvalid; }
    const std::string& captureFailure() const { return mCaptureFailure; }
    const std::string& scenarioId() const { return mScenarioId; }
    const std::string& scenarioPath() const { return mScenarioPath; }
    const std::string& sidecarPath() const { return mSidecarPath; }

private:
    struct ActorSnapshot {
        uint32_t spawnGeneration = 0;
        uint16_t transformEpoch = 0;
        int health = 0;
    };

    ScenarioServerRecorder() = default;

    void ensureHeader(uint32_t serverTick);
    void writeHeader(uint32_t serverTick);
    void writeFooter(const char* reason);
    bool writeScenarioEvent(const std::string& event, const std::string& reason,
                            uint32_t tick, const nlohmann::json& fields);
    void markCaptureFailure(const std::string& reason, uint32_t tick);
    void writeIdentity(const std::string& trackId,
                       const std::string& kind,
                       uint32_t entityId,
                       const std::string& name,
                       const std::string& avatar,
                       uint32_t spawnGeneration,
                       uint16_t transformEpoch);
    void updateLifecycle(
        const std::unordered_map<std::string, ActorSnapshot>& current,
        uint32_t serverTick);

    bool mActive = false;
    bool mCanonical = false;
    bool mProbePending = true;
    bool mProbeDone = false;
    bool mCaptureInvalid = false;
    std::string mCaptureFailure;
    std::string mScenarioId;
    std::ofstream mOut;
    std::string mScenarioPath;
    std::string mSidecarPath;
    bool mHeaderWritten = false;
    uint32_t mStartTick = 0;
    uint32_t mFrames = 0;
    uint64_t mLastCaptureWriteUs = 0;
    uint64_t mLastCaptureTotalUs = 0;
    std::unordered_map<std::string, ActorSnapshot> mLastActors;
    std::unordered_map<std::string, std::pair<glm::vec3, glm::vec3>> mLastPose;
    std::unordered_map<std::string, bool> mLastGrounded;
    std::unordered_map<std::string, uint64_t> mLastAnomalyTick;
    std::unordered_map<std::string, bool> mKnownIdentities;
};
