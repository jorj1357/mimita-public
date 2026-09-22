// 09 22 2026
/* purpose
* Implements the rewound hitscan hitbox bridge self-test.
* See hitscan-target-selftest.h for scope.
*/
#include "network/hitscan-target-selftest.h"

#include <cmath>
#include <cstdio>

#include "combat/weapon-execution.h"
#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/hot-hitscan-target.h"
#include "network/server-hitscan-targets.h"

namespace {

bool nearly(float a, float b)
{
    return std::fabs(a - b) <= 1e-5f;
}

} // namespace

bool runHitscanTargetSelfTest(std::string& report)
{
    report.clear();
    bool ok = true;

    const EntityId entity =
        Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, 7777);

    WeaponExecution::PlayerTarget target;
    target.playerId = 7777;
    target.spawnGeneration = 3;
    target.position = {1.0f, 2.0f, 3.0f};

    WeaponExecution::PlayerTarget::BodyPartBox head;
    head.center = {1.0f, 2.0f, 4.5f};
    head.half = {0.2f, 0.25f, 0.3f};
    head.bodyPart = WeaponExecution::HitBodyPart::Head;
    target.bodyParts.push_back(head);

    WeaponExecution::PlayerTarget::BodyPartBox leg;
    leg.center = {1.0f, 2.0f, 0.5f};
    leg.half = {0.2f, 0.2f, 0.4f};
    leg.bodyPart = WeaponExecution::HitBodyPart::Leg;
    target.bodyParts.push_back(leg);

    MimitaNet::publishHitscanTargets({(std::uint64_t)entity}, {target});

    MimitaRuntime::DynamicComponentStore& store =
        MimitaRuntime::DynamicComponentStore::instance();
    HotHitscanTargetV1 read{};
    const bool readOk = store.read(entity, hotHitscanTargetComponentId(), &read,
                                   sizeof(read));
    if (!readOk)
    {
        report += "[FAIL] published component not readable\n";
        ok = false;
    }
    else
    {
        if (read.version != HOT_HITSCAN_TARGET_VERSION)
        {
            report += "[FAIL] wrong component version\n";
            ok = false;
        }
        if (read.spawnGeneration != 3)
        {
            report += "[FAIL] spawnGeneration not carried\n";
            ok = false;
        }
        if (read.partCount != 2)
        {
            char line[96];
            std::snprintf(line, sizeof(line),
                          "[FAIL] partCount=%u expected 2\n", read.partCount);
            report += line;
            ok = false;
        }
        else
        {
            if (read.parts[0].bodyPart != 1u ||
                !nearly(read.parts[0].center[2], 4.5f) ||
                !nearly(read.parts[0].half[0], 0.2f))
            {
                report += "[FAIL] head box not preserved\n";
                ok = false;
            }
            if (read.parts[1].bodyPart != 2u ||
                !nearly(read.parts[1].center[2], 0.5f))
            {
                report += "[FAIL] leg box not preserved\n";
                ok = false;
            }
        }
    }

    MimitaNet::clearHitscanTargets({(std::uint64_t)entity});
    if (store.has(entity, hotHitscanTargetComponentId()))
    {
        report += "[FAIL] component not removed after clear\n";
        ok = false;
    }

    if (ok)
        report += "[OK] rewound hitboxes published, read back, and cleared\n";
    return ok;
}
