#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

class Player;
class Camera;
class NpcSystem;
class WeaponSystem;
struct InputFrame;
namespace MimitaNet { struct MultiplayerContext; }

// Lightweight per-tick input recorder for scenario playback. Schema 2 adds
// stable actor identities plus client-rendered actor/pose state. The server
// authoritative sidecar is merged into the same file when recording stops.
class ScenarioRecorder {
public:
    static ScenarioRecorder& instance();

    bool start(const std::string& name,
               const std::string& map,
               const std::string& mode,
               int weaponSet,
               const Player& player,
               const Camera& camera,
               std::string* error = nullptr);
    void stop(const char* reason);

    bool active() const { return mActive; }
    const std::string& path() const { return mPath; }
    uint32_t frames() const { return mFrame; }
    bool networked() const { return mNetworked; }
    bool merged() const { return mMerged; }
    const std::string& mergeError() const { return mMergeError; }

    void captureTick(const InputFrame& frame,
                     const Player& player,
                     const Camera& camera,
                     const NpcSystem& npcSystem,
                     const MimitaNet::MultiplayerContext& mpContext,
                     WeaponSystem& weapons);
    void poll();

private:
    struct ActorSnapshot {
        uint32_t spawnGeneration = 0;
        uint16_t transformEpoch = 0;
        int health = 0;
    };

    ScenarioRecorder() = default;

    void writeHeader(const Player& player, const Camera& camera);
    void writeFooter(const char* reason);
    void flushIfNeeded();
    void writeIdentity(const std::string& trackId,
                       const std::string& kind,
                       uint32_t entityId,
                       const std::string& name,
                       const std::string& avatar,
                       uint32_t spawnGeneration,
                       uint16_t transformEpoch);
    void updateLifecycle(const std::unordered_map<std::string, ActorSnapshot>& current,
                         uint32_t serverTick,
                         uint64_t clientTick);

    bool mActive = false;
    bool mStartPending = false;
    bool mNetworked = false;
    bool mMerged = false;
    std::ofstream mOut;
    std::string mPath;
    std::string mServerPath;
    std::string mMergeError;
    std::string mMap;
    std::string mMode;
    int mWeaponSet = 0;
    uint32_t mFrame = 0;
    uint32_t mLocalPlayerId = 0;
    uint32_t mServerTrackStartTick = 0;
    uint64_t mStartMs = 0;
    std::unordered_map<std::string, ActorSnapshot> mLastActors;
    std::unordered_map<std::string, bool> mKnownIdentities;
};
