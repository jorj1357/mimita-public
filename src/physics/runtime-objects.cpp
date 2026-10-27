// 2026-10-27

#include "physics/runtime-objects.h"

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "physics/constraints/constraint-system.h"
#include "physics/physical-entity.h"
#include "pobjects/persistent-physics.h"
#include "ragdoll/ragdoll-mode.h"
#include "vehicles/vehicle-system.h"

RuntimeObjectCounts clearRuntimeObjects(const char* reason)
{
    RuntimeObjectCounts counts;
    counts.vehicles = VehicleSystem::instance().vehicles().size();
    counts.constraints = ConstraintSystem::instance().count();
    counts.persistentObjects =
        PersistentPhysicsSystem::instance().objects().size();
    counts.corpses = RagdollModeSystem::instance().corpseCount();
    counts.physicalEntities =
        PhysicalEntitySystem::instance().entities().size();

    // Vehicles first: they remove their chassis through the physical-entity
    // owner and release their local seat/audio links.
    VehicleSystem::instance().clear();
    ConstraintSystem::instance().clear();
    PersistentPhysicsSystem::instance().clear();
    RagdollModeSystem::instance().clearCorpses();
    PhysicalEntitySystem::instance().clear();

    nlohmann::json fields;
    fields["reason"] = reason ? reason : "unspecified";
    fields["vehicles"] = counts.vehicles;
    fields["constraints"] = counts.constraints;
    fields["persistent_objects"] = counts.persistentObjects;
    fields["corpses"] = counts.corpses;
    fields["physical_entities"] = counts.physicalEntities;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "runtime.objects.cleared", "runtime-objects",
        "runtime-only world objects removed", 0, fields,
        __FILE__, __LINE__, __func__);

    return counts;
}
