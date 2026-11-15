// 2026-11-14
// MiMITA combat packet wire structs.
#pragma once

#include "network/schema/packet-common.h"
#include "network/schema/packet-header.h"

namespace MimitaNet {

#pragma pack(push, 1)

struct ShotRequestPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
};

struct ShotEventPacket
{
    PacketHeader header;
    // Reliable event dedup fields (must be the first two uint32s after the
    // header — the reliable-queue overwrites them per player and the client
    // ACKs them so every shot's bullet visual is delivered even under loss).
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    int32_t targetHealth = 0;
    float power = 0.0f;
    uint16_t effectFlags = 0;
    uint16_t targetTransformEpoch = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t impactType = SHOT_IMPACT_NONE;
    uint8_t killed = 0;
    uint8_t damageConfirmed = 0;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float dirX = 0.0f;
    float dirY = 0.0f;
    float dirZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    // Full-beam endpoint (origin + dir * weapon max range) so the tracer can
    // continue past the first hit when beam_continue_after_hit is enabled.
    float beamEndX = 0.0f;
    float beamEndY = 0.0f;
    float beamEndZ = 0.0f;
};

constexpr int MAX_PROJECTILE_DAMAGE_RESULTS = 8;

struct ProjectileDamageResultPacket
{
    uint32_t victimPlayerId = 0;
    int32_t damage = 0;
    int32_t healthAfter = 0;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    uint8_t killed = 0;
    uint32_t targetSpawnGeneration = 0;
};

struct ProjectileSpawnEventPacket
{
    PacketHeader header;
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved[3] = {};
    uint16_t weaponDefNetworkId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float rotW = 1.0f;
    float angVelX = 0.0f;
    float angVelY = 0.0f;
    float angVelZ = 0.0f;
    uint32_t spawnTick = 0;
    float lifetime = 0.0f;
    float radius = 0.0f;
};

struct ProjectileStateEventPacket
{
    PacketHeader header;
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved[3] = {};
    uint16_t weaponDefNetworkId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velX = 0.0f;
    float velY = 0.0f;
    float velZ = 0.0f;
    float rotX = 0.0f;
    float rotY = 0.0f;
    float rotZ = 0.0f;
    float rotW = 1.0f;
    float angVelX = 0.0f;
    float angVelY = 0.0f;
    float angVelZ = 0.0f;
    float age = 0.0f;
};

struct ProjectileExplodeEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t projectileId = 0;
    uint32_t ownerPlayerId = 0;
    uint32_t fireSerial = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t victimCount = 0;
    uint8_t reserved[2] = {};
    uint16_t weaponDefNetworkId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float radius = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 1.0f;
    ProjectileDamageResultPacket victims[MAX_PROJECTILE_DAMAGE_RESULTS];
};

struct ProjectileDespawnEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t projectileId = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reason = 0;
    uint8_t reserved[2] = {};
    uint16_t weaponDefNetworkId = 0;
};

struct ReliableEventAckPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
};

struct DamageConfirmedEventPacket
{
    PacketHeader header;
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t attackerPlayerId = 0;
    uint32_t targetPlayerId = 0;
    uint32_t causeSerial = 0;
    uint32_t projectileId = 0;
    uint32_t attackerSpawnGeneration = 0;
    uint32_t targetSpawnGeneration = 0;
    int32_t damage = 0;
    int32_t healthBefore = 0;
    int32_t healthAfter = 0;
    uint8_t source = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t killed = 0;
    uint8_t attackerEntityType = ENTITY_PLAYER;
    uint8_t bodyPart = 0xFF;          // ActorBodyPart (0..5), 0xFF = unknown
    uint8_t partHealthAfter = 0;      // authoritative part health after the hit
    uint8_t partDestroyed = 0;        // 1 when this hit destroyed the part
    uint16_t weaponDefNetworkId = 0;  // Dynamic weapon ID — use weaponIdForDefNetworkId() to resolve
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    float contactForce = -1.0f; // shared physical-impact feedback
};

// attackType 3 (weapon clash) presentation flags replicated by the server.
constexpr uint8_t MELEE_CLASH_FX_SPARK = 1 << 0;
constexpr uint8_t MELEE_CLASH_FX_RICOCHET = 1 << 1;

struct MeleeHitEventPacket
{
    PacketHeader header;
    uint32_t attackSerial = 0;
    uint32_t attackerPlayerId = 0;
    uint32_t targetPlayerId = 0;
    int32_t damage = 0;
    int32_t targetHealth = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t attackType = 0;
    uint8_t killed = 0;
    uint8_t damageConfirmed = 0;
    uint8_t clashFlags = 0;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float knockX = 0.0f;
    float knockY = 0.0f;
    float knockZ = 0.0f;
    uint16_t targetTransformEpoch = 0;
    uint32_t targetSpawnGeneration = 0;
};

// ── VIP name style sync ──────────────────────────────────────────────
// Server broadcasts the full user-chosen style once per join/change so every
// client renders exact colors. Animation state is NEVER sent: each client
// renders the shift locally from its own tick.

struct DisagreementPacket
{
    PacketHeader header;
    uint8_t reason = DISAGREEMENT_NONE;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
    uint32_t eventId = 0;
    uint32_t relatedSerial = 0;
    uint32_t sourcePlayerId = 0;
    uint32_t targetPlayerId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float correctionX = 0.0f;
    float correctionY = 0.0f;
    float correctionZ = 0.0f;
    char description[64];
};

// ── Pellet blast packets ─────────────────────────────────────────────
struct PelletBlastRequestPacket
{
    PacketHeader header;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t lastServerTick = 0;
    uint32_t spreadSeed = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t reserved0 = 0;
    uint16_t reserved1 = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float baseDirX = 0.0f;
    float baseDirY = 0.0f;
    float baseDirZ = 0.0f;
};

constexpr int MAX_PELLET_BLAST_TARGETS = 8;

struct PelletBlastTargetResult
{
    uint32_t targetPlayerId = 0;
    int16_t totalDamage = 0;
    int16_t healthAfter = 0;
    int16_t knockX = 0, knockY = 0, knockZ = 0;
    uint8_t pelletsHit = 0;
    uint8_t killed = 0;
    uint32_t targetSpawnGeneration = 0;
};

struct NetworkPelletResult
{
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    uint32_t targetPlayerId = 0;
    uint8_t impactType = PELLET_IMPACT_NONE;
    uint8_t bodyPart = 0;
    uint8_t pelletIndex = 0;
    uint8_t reserved = 0;
};

struct PelletBlastEventPacket
{
    PacketHeader header;
    // Reliable event dedup fields (same contract as ShotEventPacket).
    uint32_t eventId = 0;
    uint32_t eventSessionId = 0;
    uint32_t shotSerial = 0;
    uint64_t clientTimeMs = 0;
    uint32_t shooterPlayerId = 0;
    uint32_t spreadSeed = 0;
    uint32_t lastServerTick = 0;
    float originX = 0.0f;
    float originY = 0.0f;
    float originZ = 0.0f;
    float baseDirX = 0.0f;
    float baseDirY = 0.0f;
    float baseDirZ = 0.0f;
    float beamEndX = 0.0f;
    float beamEndY = 0.0f;
    float beamEndZ = 0.0f;
    float maxRange = 0.0f;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t pelletCount = 0;
    uint8_t targetCount = 0;
    uint8_t reserved = 0;
    NetworkPelletResult pellets[MAX_NETWORK_PELLETS];
    PelletBlastTargetResult targets[MAX_PELLET_BLAST_TARGETS];
};

// ── Ragdoll / physical-body limb state replication ─────────────────────
// Six limbs in the fixed order owned by ragdoll-replication.h. Raw floats to
// keep packets.h independent of glm. 20-byte header + 12 + 6*28 = 200 bytes.
struct RagdollLimbWire
{
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
    // Damage hitbox: world AABB center and half extents (owner-computed, same
    // formula as the client's hit detection).
    float hx = 0.0f, hy = 0.0f, hz = 0.0f;
    float hhx = 0.0f, hhy = 0.0f, hhz = 0.0f;
};

struct RagdollStatePacket
{
    PacketHeader header;
    uint32_t ownerPlayerId = 0;
    uint32_t sourceTick = 0;
    // Owning client's authoritative root while ragdolled (server accepts it).
    float rootX = 0.0f, rootY = 0.0f, rootZ = 0.0f, rootYaw = 0.0f;
    uint8_t active = 0;
    uint8_t mode = 0;
    uint8_t count = 0;
    uint8_t reserved = 0;
    RagdollLimbWire limbs[6];
};

static_assert(sizeof(RagdollStatePacket) <= MAX_GAME_DATAGRAM_BYTES,
              "RagdollStatePacket exceeds the safe datagram size");

// ── Godball state (position + velocity for remote visual replication) ──
struct GodballStatePacket
{
    PacketHeader header;
    uint32_t ownerPlayerId = 0;
    float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    uint8_t active = 0;
};

// ── Godball hit claim (owner → server, reliable) ──────────────────────
struct GodballHitClaimPacket
{
    PacketHeader header;
    uint32_t attackerId = 0;
    uint32_t targetId = 0;
    uint32_t contactSerial = 0;
    uint32_t simulationTick = 0;
    float damage = 0.0f;
    float ballSpeed = 0.0f;
    float hitX = 0.0f, hitY = 0.0f, hitZ = 0.0f;
    float normalX = 0.0f, normalY = 0.0f, normalZ = 0.0f;
    uint32_t spawnGeneration = 0;
};

struct SpyKnifeContact
{
    uint32_t targetId = 0;
    uint32_t contactTick = 0;
    uint32_t contactId = 0;
    uint32_t targetGeneration = 0;
    uint8_t targetIsNpc = 0;
    uint8_t isBackstab = 0;
    uint16_t reserved = 0;
    // Client-side action phase elapsed when this contact was detected. The
    // server uses the same phase for damage/rev resolution even when its own
    // action start tick is a few ticks later than the client's.
    uint16_t actionElapsedTicks = 0;
    // Index into the client's move-set action list. 0xffff = use the server's
    // current/retained action id.
    uint16_t actionIndex = 0xffff;
    float impactSpeed = 0.0f;
    float impactForce = 0.0f;
    float impactDirectness = 0.0f;
    float hitX = 0.0f, hitY = 0.0f, hitZ = 0.0f;
    float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;
    float boxCenterX = 0.0f, boxCenterY = 0.0f, boxCenterZ = 0.0f;
    float boxHalfX = 0.5f, boxHalfY = 0.5f, boxHalfZ = 0.5f;
    float boxAxis0X = 1.0f, boxAxis0Y = 0.0f, boxAxis0Z = 0.0f;
    float boxAxis1X = 0.0f, boxAxis1Y = 1.0f, boxAxis1Z = 0.0f;
    float boxAxis2X = 0.0f, boxAxis2Y = 0.0f, boxAxis2Z = 1.0f;
};

struct SpyKnifeContactBatchPacket
{
    PacketHeader header;
    uint32_t attackerId = 0;
    uint32_t attackerSpawnGeneration = 0;
    uint32_t batchId = 0;
    uint8_t contactCount = 0;
    uint8_t reserved[3] = {};
    SpyKnifeContact contacts[SPYKNIFE_CONTACT_BATCH_MAX]{};
};

// ── Projectile fire result ────────────────────────────────────────────
enum ProjectileFireResultReason : uint8_t
{
    PROJECTILE_FIRE_ACCEPTED = 0,
    PROJECTILE_FIRE_ALREADY_ACCEPTED = 1,
    PROJECTILE_FIRE_DEAD = 2,
    PROJECTILE_FIRE_COOLDOWN = 3,
    PROJECTILE_FIRE_WEAPON_MISMATCH = 4,
    PROJECTILE_FIRE_ORIGIN_INVALID = 5,
    PROJECTILE_FIRE_DIRECTION_INVALID = 6,
    PROJECTILE_FIRE_CONFIG_MISSING = 7
};

struct ProjectileFireResultPacket
{
    PacketHeader header;
    uint32_t fireSerial = 0;
    uint32_t projectileId = 0;
    uint8_t weapon = NETWORK_WEAPON_NONE;
    uint8_t accepted = 0;
    uint8_t reason = 0;
    uint8_t reserved = 0;
    float cooldownRemaining = 0.0f;
};


// ── Generic attack: one request type for all weapons ─────────────────
// All weapons (hitscan, projectile, melee) share this packet.
// weaponDefNetworkId is a stable uint16 assigned during protocol init.
// basedOnInputSequence ties the attack to a specific equip/input frame.

constexpr int MAX_CLAIMED_HIT_TARGETS = 8;

// Client-local hitscan aggregate for one target. The server validates target
// identity, life, range, occlusion, hit volume, and caps damage before using
// the claimed total. This is the client-authority contract: the shooter owns
// what they saw; the server rejects only implausible outcomes.
struct ClaimedHitPacket
{
    uint32_t targetId = 0;
    uint32_t targetSpawnGeneration = 0;
    uint16_t damage = 0;
    uint8_t pelletHits = 0;
    uint8_t bodyPart = 0; // 0=none 1=head 2=torso 3=leg
    uint8_t pad[2] = {};
    float hitX = 0.0f;
    float hitY = 0.0f;
    float hitZ = 0.0f;
};

struct AttackRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t clientSimulationTick = 0;
    uint16_t basedOnInputSequence = 0;
    int16_t equippedSlot = 0;
    uint16_t weaponDefNetworkId = 0;
    float aimOriginX = 0, aimOriginY = 0, aimOriginZ = 0;
    float aimDirX = 0, aimDirY = 0, aimDirZ = 0;
    float muzzlePosX = 0, muzzlePosY = 0, muzzlePosZ = 0;
    uint32_t deterministicSeed = 0;
    uint8_t attackVariant = 0;
    // ── Client hit claim ("shoot what I saw") ──────────────────────────
    // The client traces against the exact body it rendered (body-part
    // hitboxes). The server re-traces authoritatively with lag compensation,
    // but if its rewind re-trace misses, it accepts this claimed hit point
    // when it is inside the target's rewound collision volume (within
    // rewind_hit_tolerance) and not wall-occluded — so what the shooter saw
    // is what takes damage. The target is looked up by id and validated
    // against its current spawn generation; no generation is sent.
    uint32_t claimedTargetId = 0;
    float claimedHitX = 0, claimedHitY = 0, claimedHitZ = 0;
    uint8_t claimedBodyPart = 0; // 0=none 1=head 2=torso 3=leg
    uint8_t reservedAttack[2] = {};
    uint32_t movesetHash = 0;
    // Validated client-first per-target hitscan claims. Single-ray weapons use
    // one entry; multi-pellet weapons use one entry per target.
    uint8_t claimedHitCount = 0;
    uint8_t claimPad[3] = {};
    ClaimedHitPacket claimedHits[MAX_CLAIMED_HIT_TARGETS];
};

struct AttackResultPacket
{
    PacketHeader header;
    uint64_t nextAllowedFireTick = 0;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint32_t projectileId = 0;
    int32_t magazineAmmo = 0;
    int32_t reserveAmmo = 0;
    uint32_t stateRevision = 0;
    uint32_t serverTick = 0;
    uint16_t weaponDefNetworkId = 0;
    uint8_t accepted = 0;
    uint8_t reason = 0;
    uint8_t hitVerdict = 0; // HitVerdict
};

// ── Reload request/result ────────────────────────────────────────────
struct ReloadRequestPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint16_t weaponDefNetworkId = 0;
    // Client-authoritative ammo at the moment the reload starts. The server
    // adopts these numbers (when >= 0) so it never rejects on a drifted copy.
    int32_t magazineAmmo = -1;
    int32_t reserveAmmo = -1;
};

struct ReloadResultPacket
{
    PacketHeader header;
    uint32_t requestId = 0;
    uint32_t spawnGeneration = 0;
    uint16_t weaponDefNetworkId = 0;
    uint8_t accepted = 0;
    uint8_t reason = 0;
    int32_t magazineAmmo = 0;
    int32_t reserveAmmo = 0;
    uint64_t reloadCompleteTick = 0;
    uint64_t nextAllowedFireTick = 0;
    uint8_t reloading = 0;
    uint32_t stateRevision = 0;
};

// ── Respawn request/result ───────────────────────────────────────────

#pragma pack(pop)

}  // namespace MimitaNet
