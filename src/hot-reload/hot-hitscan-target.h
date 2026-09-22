// 09 22 2026
/* purpose
* Shared plain-data bridge for authoritative rewound actor hitboxes.
* The cold server publishes one `HitscanTargetBoxes` dynamic component per actor
* in a hitscan trace (reconstructed from the SAME rewound body parts the cold
* trace validates against); the hot hitscan behavior reads it through the
* generic dynamic-component capability. No ABI change and no second geometry
* owner: the component carries exactly what the cold authority used.
* Runtime-only and non-replicated: it exists only for the duration of one shot.
* Does NOT own damage, ammo, collision, or networking.
*/
#pragma once

#include "hot-reload/game-api.h"

#include <cstdint>

static constexpr std::uint32_t HOT_HITSCAN_TARGET_VERSION = 1;
static constexpr int HOT_HITSCAN_MAX_PARTS = 8;

// bodyPart numeric matches WeaponExecution::HitBodyPart (0 torso, 1 head, 2 leg).
struct HotHitscanPartV1 {
    float center[3];
    float half[3];
    std::uint32_t bodyPart;
    std::uint32_t reserved;
};

struct HotHitscanTargetV1 {
    std::uint32_t version;
    std::uint32_t partCount;
    std::uint32_t spawnGeneration;
    std::uint32_t reserved;
    HotHitscanPartV1 parts[HOT_HITSCAN_MAX_PARTS];
};

inline std::uint64_t hotHitscanTargetComponentId() {
    return gameHash("HitscanTargetBoxes");
}

inline std::uint64_t hotHitscanTargetSchemaHash() {
    return gameHash("HitscanTargetBoxes.v1");
}
