#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

class NpcSystem;

namespace MimitaNet {
struct ServerPlayer;
struct ServerNpc;
}

// Owns the authoritative actor track captured beside a client scenario
// recording on the local dev-loop server. The client merges this sidecar into
// the final schema-2 scenario when recording stops.
class ScenarioServerRecorder {
public:
    static ScenarioServerRecorder& instance();

    bool start(const std::string& scenarioPath, std::string* error);
    void stop(const char* reason);
    void captureTick(
        const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
        const std::unordered_map<uint32_t, MimitaNet::ServerNpc>& npcs,
        const NpcSystem& npcSystem,
        uint32_t serverTick);

    bool active() const { return mActive; }
    const std::string& scenarioPath() const { return mScenarioPath; }
    const std::string& sidecarPath() const { return mSidecarPath; }

private:
    struct ActorSnapshot {
        uint32_t spawnGeneration = 0;
        uint16_t transformEpoch = 0;
        int health = 0;
    };

    ScenarioServerRecorder() = default;

    void writeHeader(uint32_t serverTick);
    void writeFooter(const char* reason);
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
    std::ofstream mOut;
    std::string mScenarioPath;
    std::string mSidecarPath;
    uint32_t mStartTick = 0;
    uint32_t mFrames = 0;
    std::unordered_map<std::string, ActorSnapshot> mLastActors;
    std::unordered_map<std::string, bool> mKnownIdentities;
};
