// 2026-11-05
/* purpose
* Own the host-authoritative world-edit session: typed operations, per-property
* revisions, capabilities, accepted-op ordering, undo, replay, and snapshots.
* This file DOES NOT own sockets, packet layouts, project files, or map runtime
* geometry. The host applies accepted operations to MapConfigRegistry.
*/
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MimitaWorldProject {

enum class WorldEditCapability : uint32_t
{
    None = 0,
    Geometry = 1u << 0,
    Chunks = 1u << 1,
    Npcs = 1u << 2,
    Weapons = 1u << 3,
    Interactions = 1u << 4,
    Undo = 1u << 5,
    Publish = 1u << 6,
    All = 0x7f,
};

inline WorldEditCapability operator|(WorldEditCapability a, WorldEditCapability b)
{
    return static_cast<WorldEditCapability>(static_cast<uint32_t>(a) |
                                            static_cast<uint32_t>(b));
}
inline WorldEditCapability operator&(WorldEditCapability a, WorldEditCapability b)
{
    return static_cast<WorldEditCapability>(static_cast<uint32_t>(a) &
                                            static_cast<uint32_t>(b));
}

enum class WorldEditOpKind : uint8_t
{
    Set = 0,
    Create = 1,
    Delete = 2,
    Undo = 3,
};

enum class WorldEditTargetKind : uint8_t
{
    Entity = 0,
    Block = 1,
    Vehicle = 2,
    BombSite = 3,
    Payload = 4,
    NpcPreset = 5,
    GenerationZone = 6,
    Interaction = 7,
    World = 8,
};

enum class WorldEditValueType : uint8_t
{
    None = 0,
    Bool = 1,
    Int = 2,
    Float = 3,
    Vec3 = 4,
    String = 5,
};

struct WorldEditVec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool operator==(const WorldEditVec3& other) const
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct WorldEditValue
{
    WorldEditValueType type = WorldEditValueType::None;
    bool boolValue = false;
    int64_t intValue = 0;
    double floatValue = 0.0;
    WorldEditVec3 vec3Value;
    std::string stringValue;

    static WorldEditValue boolean(bool value);
    static WorldEditValue integer(int64_t value);
    static WorldEditValue number(double value);
    static WorldEditValue vector(double x, double y, double z);
    static WorldEditValue text(std::string value);

    bool operator==(const WorldEditValue& other) const;
};

struct WorldEditOperation
{
    uint64_t opId = 0;
    uint32_t baseRevision = 0;
    WorldEditCapability capability = WorldEditCapability::None;
    WorldEditOpKind kind = WorldEditOpKind::Set;
    WorldEditTargetKind target = WorldEditTargetKind::Entity;
    std::string objectId;
    uint16_t propertyId = 0;
    WorldEditValue value;
    uint32_t undoSequence = 0;
    // Optional host-supplied prior state. The applier reads the live map value
    // before the host accepts the operation so the first edit is undoable.
    bool hasPrevious = false;
    WorldEditValue previousValue;
};

struct WorldEditAccepted
{
    uint32_t sequence = 0;
    uint32_t revision = 0;
    uint64_t opId = 0;
    std::string authorAccountId;
    WorldEditCapability capability = WorldEditCapability::None;
    WorldEditOpKind kind = WorldEditOpKind::Set;
    WorldEditTargetKind target = WorldEditTargetKind::Entity;
    std::string objectId;
    uint16_t propertyId = 0;
    WorldEditValue value;
    bool previousExists = false;
    WorldEditValue previousValue;
    uint32_t undoSequence = 0;
};

struct WorldEditResult
{
    bool accepted = false;
    bool conflict = false;
    std::string reason;
    uint32_t sequence = 0;
    uint32_t revision = 0;
    uint32_t currentPropertyRevision = 0;
};

struct WorldEditSnapshot
{
    uint32_t lastSequence = 0;
    uint32_t revision = 0;
    std::string stateHash;
    std::vector<WorldEditAccepted> properties;
};

class WorldEditSession
{
public:
    void begin(const std::string& projectId,
               const std::string& projectVersion,
               const std::string& ownerAccountId);
    void reset();

    bool active() const { return mActive; }
    const std::string& projectId() const { return mProjectId; }
    const std::string& projectVersion() const { return mProjectVersion; }
    const std::string& ownerAccountId() const { return mOwnerAccountId; }

    void setCapabilities(const std::string& accountId, WorldEditCapability mask);
    void grantCapabilities(const std::string& accountId, WorldEditCapability mask);
    void revokeCapabilities(const std::string& accountId, WorldEditCapability mask);
    WorldEditCapability capabilities(const std::string& accountId) const;
    bool authorized(const std::string& accountId,
                    WorldEditCapability required) const;

    WorldEditResult submit(const std::string& authorAccountId,
                           const WorldEditOperation& operation);
    WorldEditResult submitUndo(const std::string& authorAccountId,
                               uint32_t targetSequence);

    bool acknowledge(const std::string& accountId, uint32_t sequence);
    uint32_t acknowledged(const std::string& accountId) const;
    bool replay(const std::string& accountId, uint32_t afterSequence,
                std::vector<WorldEditAccepted>& out) const;
    bool snapshot(WorldEditSnapshot& out) const;

    void setTargetExistsCallback(
        std::function<bool(WorldEditTargetKind, const std::string&)> callback);

    // Tests use a small retained-log budget to prove snapshot fallback.
    void setLogLimitForTesting(size_t limit) { mLogLimit = limit ? limit : 1; }
    size_t logSize() const { return mLog.size(); }
    uint32_t lastSequence() const { return mSequence; }
    uint32_t revision() const { return mRevision; }
    uint32_t propertyRevisionFor(WorldEditTargetKind target,
                                 const std::string& objectId,
                                 uint16_t propertyId) const;
    bool acceptedBySequence(uint32_t sequence, WorldEditAccepted& out) const;

private:
    std::string propertyKey(WorldEditTargetKind target,
                            const std::string& objectId,
                            uint16_t propertyId) const;
    bool hasCapabilityForTarget(const std::string& accountId,
                                WorldEditCapability declared,
                                WorldEditTargetKind target) const;
    uint32_t propertyRevision(const std::string& key) const;
    void applyAccepted(const WorldEditAccepted& accepted);
    void trimLog();
    WorldEditResult reject(const std::string& reason, bool conflict,
                           uint32_t propertyRevisionValue = 0) const;

    bool mActive = false;
    std::string mProjectId;
    std::string mProjectVersion;
    std::string mOwnerAccountId;
    uint32_t mSequence = 0;
    uint32_t mRevision = 0;
    std::unordered_map<std::string, WorldEditCapability> mCapabilities;
    std::unordered_map<std::string, uint32_t> mPropertyRevisions;
    std::unordered_map<std::string, WorldEditValue> mPropertyValues;
    std::unordered_map<std::string, uint32_t> mAcknowledgements;
    std::vector<WorldEditAccepted> mLog;
    size_t mLogLimit = 2048;
    std::function<bool(WorldEditTargetKind, const std::string&)>
        mTargetExists;
};

// Focused session self-test; no sockets or filesystem.
bool worldEditSessionSelfTest(std::string& report);

} // namespace MimitaWorldProject
