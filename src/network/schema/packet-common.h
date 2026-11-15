// 2026-11-14
// Shared MiMITA packet limits, flags, and enums.
#pragma once

#include <cstdint>

namespace MimitaNet {

// ── Player state flags for remote visual replication ──────────────
enum NetworkPlayerStateFlags : uint16_t
{
    NET_STATE_WALKING      = 1 << 0,
    NET_STATE_JUMPING      = 1 << 1,
    NET_STATE_DASHING      = 1 << 2,
    NET_STATE_DOWN_DASHING = 1 << 3,
    NET_STATE_FREEZING     = 1 << 4,
    NET_STATE_ON_GROUND    = 1 << 5,
    NET_STATE_ATTACKING    = 1 << 6,
    NET_STATE_GODBALL_ACTIVE = 1 << 7,
    // Melee: held shield and grab intent. stateFlags is uint16_t so these do
    // not change any packet size.
    NET_STATE_SHIELDING    = 1 << 8,
    NET_STATE_GRABBING     = 1 << 9,
    NET_STATE_CROUCHING    = 1 << 10,
    NET_STATE_REVIVING     = 1 << 11,
    // The server changed this actor's authoritative transform through the
    // actor-pair collision pass on this tick. Clients apply the authoritative
    // transform instead of treating the push as ordinary prediction drift.
    // Input-only held interaction intent (F). It is not a visual flag and is
    // not copied into broadcast snapshot flags.
    NET_STATE_INTERACTING  = 1 << 14,

    NET_STATE_ACTOR_COLLISION = 1 << 12,
    // The server corrected a movement report. The owning client must apply
    // that authoritative transform regardless of the ordinary drift threshold.
    NET_STATE_POSITION_CORRECTED = 1 << 13
};
constexpr int MAX_RECONNECT_TOKEN_BYTES = 64;
constexpr int MAX_JOIN_TOKEN_BYTES = 64;
constexpr int MAX_VIP_JOIN_TICKET_BYTES = 64;
constexpr int MAX_RUNTIME_RUN_ID_BYTES = 64;
constexpr int MAX_RUNTIME_BUILD_ID_BYTES = 96;
constexpr int MAX_PLAYERS = 32;
constexpr int MAX_SNAPSHOT_ENTITIES = 90;
constexpr int MAX_NAME_BYTES = 32;
// Map ids are content-defined GLB basenames (for example,
// "mimita-aabb-only-interior-small-v4"). The old MAX_NAME_BYTES field silently
// truncated versions, so the client was told to load a map that does not exist
// and then reported ready on a stale world. Give map ids their own budget.
constexpr int MAX_MAP_ID_BYTES = 64;
// Avatar folder names are user/content-defined and can exceed the old 15-byte
// snapshot field (for example, "abusivegirlheadless"). Keep replication from
// silently truncating a valid avatar into a nonexistent folder name.
constexpr int MAX_AVATAR_NAME_BYTES = 32;

// Safe datagram size: 1200 bytes ensures no IP fragmentation on internet paths.
// Accounts for IP header (20), UDP header (8), ICE/STUN overhead (~32),
// and leaves ~1140 bytes for game payload.
constexpr int MAX_GAME_DATAGRAM_BYTES = 1200;
constexpr int MAX_RECEIVE_PACKET_BYTES = 32768; // includes legacy assembled snapshots

enum DamageConfirmedSource : uint8_t
{
    DAMAGE_CONFIRMED_HITSCAN = 1,
    DAMAGE_CONFIRMED_MELEE = 2,
    DAMAGE_CONFIRMED_ROCKET_EXPLOSION = 3,
    DAMAGE_CONFIRMED_GRENADE_EXPLOSION = 4,
    DAMAGE_CONFIRMED_PHYSICAL_CONTACT = 5,
    DAMAGE_CONFIRMED_ENVIRONMENT = 6
};

// Verdict the server returns for every accepted attack so the client can
// resolve its local hit prediction the moment the round trip completes
// (instead of waiting for a damage event that may never arrive on a miss).
enum HitVerdict : uint8_t
{
    HIT_VERDICT_NONE = 0,
    HIT_VERDICT_HIT_CLAIMED_TARGET = 1, // server confirmed the claimed target
    HIT_VERDICT_HIT_OTHER_TARGET = 2,   // server hit a different target
    HIT_VERDICT_MISS = 3                // server traced no hit at all
};

enum EntityType : uint8_t
{
    ENTITY_NONE = 0,
    ENTITY_PLAYER = 1,
    ENTITY_NPC = 2
};

enum NetworkWeaponType : uint8_t
{
    // DEPRECATED: Do not use for new code. Use weaponDefNetworkId (uint16_t)
    // from the dynamic registration system in network-weapons.h instead.
    // This enum is kept only for legacy packet struct wire compatibility.
    NETWORK_WEAPON_NONE = 0,
    NETWORK_WEAPON_REVOLVER = 1,
    NETWORK_WEAPON_GODBALL = 2,
    NETWORK_WEAPON_SHOTGUN = 3,
    NETWORK_WEAPON_SWORDSWORD = 4,
    NETWORK_WEAPON_ROCKET_LAUNCHER = 5,
    NETWORK_WEAPON_HAFS = 6,
    NETWORK_WEAPON_GRENADE_LAUNCHER = 7,
    NETWORK_WEAPON_AA12 = 8,
    NETWORK_WEAPON_SPYKNIFE = 9,
    NETWORK_WEAPON_FORCE_PUNCH = 10,
    NETWORK_WEAPON_PROJECTILE_RIFLE = 11,
    NETWORK_WEAPON_LARGE_MACHINE_GUN = 12,
    NETWORK_WEAPON_HEAVY_SHOTGUN = 13
};

enum NetworkWeaponStateFlags : uint8_t
{
    NET_WEAPON_STATE_FIRING = 1 << 0,
    NET_WEAPON_STATE_RELOADING = 1 << 1,
    NET_WEAPON_STATE_COOLDOWN = 1 << 2,
    NET_WEAPON_STATE_EMPTY = 1 << 3,
    NET_WEAPON_STATE_EQUIPPING = 1 << 4
};

enum ShotImpactType : uint8_t
{
    SHOT_IMPACT_NONE = 0,
    SHOT_IMPACT_WORLD = 1,
    SHOT_IMPACT_ENTITY = 2
};

enum DisagreementReason : uint8_t
{
    DISAGREEMENT_NONE = 0,
    DISAGREEMENT_OCCLUDED_SHOT = 1,
    DISAGREEMENT_INVALID_DAMAGE = 2,
    DISAGREEMENT_POSITION_CORRECTION = 3,
    DISAGREEMENT_INVALID_MOVEMENT = 4,
    DISAGREEMENT_INVALID_STATE = 5,
    DISAGREEMENT_REWIND_MISS = 6,
    DISAGREEMENT_TARGET_NOT_FOUND = 7,
    DISAGREEMENT_TARGET_DEAD = 8,
    DISAGREEMENT_SELF_TARGET = 9
};

// ── Pellet blast impact types ──────────────────────────────────────
enum PelletImpactType : uint8_t
{
    PELLET_IMPACT_NONE = 0,
    PELLET_IMPACT_WORLD = 1,
    PELLET_IMPACT_PLAYER = 2
};

enum HitBodyPart : uint8_t
{
    HIT_BODY_HEAD = 0,
    HIT_BODY_TORSO = 1,
    HIT_BODY_ARM = 2,
    HIT_BODY_LEG = 3
};

constexpr int MAX_NETWORK_PELLETS = 16;

static constexpr uint8_t SPYKNIFE_CONTACT_BATCH_MAX = 6;

enum ShotEffectFlags : uint16_t
{
    SHOT_EFFECT_MUZZLE = 1 << 0,
    SHOT_EFFECT_TRACER = 1 << 1,
    SHOT_EFFECT_SHOOT_SOUND = 1 << 2,
    SHOT_EFFECT_WORLD_IMPACT = 1 << 3,
    SHOT_EFFECT_DEBRIS = 1 << 4,
    SHOT_EFFECT_ENTITY_IMPACT = 1 << 5,
    SHOT_EFFECT_BLOOD = 1 << 6,
    SHOT_EFFECT_HIT_SOUND = 1 << 7,
    SHOT_EFFECT_WEAPON_TRIGGER = 1 << 8,
    // Confirmed Melee impact feedback may also be shown to its local attacker;
    // unlike gun impacts it has no locally predicted contact effect.
    SHOT_EFFECT_MELEE_CONFIRM = 1 << 9
};

}  // namespace MimitaNet
