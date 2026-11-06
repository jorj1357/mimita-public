// 2026-11-06
/* purpose
* Own the shared six-part actor body state used by players and NPCs.
* Provides authoritative part health, skin/meat/bone layer derivation, and
* deterministic part-name/index mapping for damage, replication, and rendering.
* Does NOT own movement, hit tracing, packet transport, or visual meshes.
*/
#pragma once

#include <array>
#include <cstdint>
#include <string>

enum class ActorBodyPart : uint8_t
{
    Head = 0,
    Torso = 1,
    LeftArm = 2,
    RightArm = 3,
    LeftLeg = 4,
    RightLeg = 5,
    Count = 6,
    Unknown = 0xFF,
};

constexpr int kActorBodyPartCount = 6;

// Render layers derived from part health.
enum class ActorBodyLayer : uint8_t
{
    Skin = 0,
    Meat = 1,
    Bone = 2,
    Destroyed = 3,
};

struct ActorBodyConfig
{
    int partMaxHealth = 100;
    // A part below two thirds is meat; below one third is bone.
    float meatThreshold = 0.66f;
    float boneThreshold = 0.33f;
    // Procedural inner-shell scale relative to the original part mesh.
    float meatShellScale = 0.94f;
    float boneShellScale = 0.88f;
    // Extra blood presentation when the last hit destroys the part.
    float destroyedBloodScale = 2.5f;

    static ActorBodyConfig& instance();
};

struct ActorBodyState
{
    std::array<uint8_t, kActorBodyPartCount> partHealth{};

    ActorBodyState() { reset(); }

    void reset();
    // Applies damage to one part. Returns true when the part reaches zero.
    bool applyDamage(ActorBodyPart part, int damage);
    bool destroyed(ActorBodyPart part) const;
    bool lethalPartDestroyed() const;
    uint8_t health(ActorBodyPart part) const;
    ActorBodyLayer layer(ActorBodyPart part) const;
};

const char* actorBodyPartName(ActorBodyPart part);
ActorBodyPart actorBodyPartFromName(const std::string& name);
// Ragdoll limb order: torso, head, leftArm, rightArm, leftLeg, rightLeg.
ActorBodyPart actorBodyPartFromRagdollIndex(int index);
// Legacy WeaponExecution::HitBodyPart: 0 torso, 1 head, 2 leg.
ActorBodyPart actorBodyPartFromHitBodyPart(uint8_t hitBodyPart);
// Claimed-hit wire part: 1 head, 2 torso, 3 leg.
ActorBodyPart actorBodyPartFromWireClaim(uint8_t wirePart);
