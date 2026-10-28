#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "devtools/scenario-tracks.h"

class NpcSystem;

namespace MimitaNet {
struct ServerPlayer;
struct ServerNpc;
}

// Server-side half of schema-2 scenario playback. It validates the scenario,
// creates temporary Npc-backed target proxies from the recorded authoritative
// track, and advances those proxies one fixed tick ahead of the normal melee /
// physical-contact pass. Live connected humans are never proxied.
class ScenarioServerPlayback {
public:
    static ScenarioServerPlayback& instance();

    bool prepare(const std::string& path,
                 const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
                 NpcSystem& npcSystem,
                 std::string* error,
                 std::string* status);
    bool begin(uint32_t serverTick,
               const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
               std::string* error,
               std::string* status);
    void tick(uint32_t serverTick,
              const std::unordered_map<uint32_t, MimitaNet::ServerPlayer>& players,
              NpcSystem& npcSystem);
    void stop(const char* reason, NpcSystem& npcSystem);

    bool active() const { return mActive; }
    bool prepared() const { return mPrepared; }
    const std::string& path() const { return mPath; }
    const std::string& localPlayerTrack() const { return mLocalPlayerTrack; }

private:
    struct Proxy {
        std::string trackId;
        std::string name;
        std::string avatar;
        uint32_t proxyId = 0;
        uint32_t lastGeneration = 0;
        uint16_t lastEpoch = 0;
        bool hasLife = false;
        bool present = false;
    };

    ScenarioServerPlayback() = default;

    bool load(const std::string& path, std::string* error);
    bool applyActor(Proxy& proxy,
                    const ScenarioTracks::ActorState& state,
                    NpcSystem& npcSystem,
                    uint32_t serverTick,
                    bool initial);
    void removeProxy(Proxy& proxy, NpcSystem& npcSystem, uint32_t serverTick);
    const ScenarioTracks::ActorState* stateFor(const std::string& trackId,
                                               uint32_t trackTick) const;
    uint32_t mapTrackTick(uint32_t serverTick) const;

    bool mPrepared = false;
    bool mActive = false;
    std::string mPath;
    std::string mLocalPlayerTrack;
    uint32_t mLocalPlayerId = 0;
    uint32_t mFirstTrackTick = 0;
    uint32_t mLastTrackTick = 0;
    uint32_t mPlaybackStartServerTick = 0;
    uint32_t mNextProxyId = 0x40000000u;
    std::string mMap;
    std::string mMode;
    int mWeaponSet = 0;
    uint32_t mStartSpawnGeneration = 0;
    uint16_t mStartTransformEpoch = 0;
    std::vector<ScenarioTracks::MeleeRevision> mRevisions;
    std::vector<ScenarioTracks::ActorIdentity> mIdentities;
    std::map<uint32_t, std::vector<ScenarioTracks::ActorState>> mServerTicks;
    std::map<std::string, Proxy> mProxies;
};
