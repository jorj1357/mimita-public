// Versioned player_core resident-state migration.
//
// v1: generated player_core record with packed flags.
// v2: adds migration_epoch.
// v3: removes flags, keeps migration_epoch, adds migration_revision.
//
// The host owns the storage. A rejected migration returns false from the
// StateMigrateFn and the host keeps the old block committed.

#include "game/player-core-migration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "debug/structured-log.h"
#include "game/state-bridge-module.h"
#include "host/abi/state-capability.h"
#include "host/generated/resident-state-v1.h"

namespace mimita::game {
namespace {

using mimita::host::StateObjectId;
using mimita::host::StateMigrateFn;

constexpr std::uint64_t kPoisonEpoch = 0xDEADBEEFULL;

struct PlayerCoreV1 {
    std::uint32_t structSize = sizeof(PlayerCoreV1);
    std::uint16_t version = 1;
    std::uint16_t reserved = 0;
    std::uint32_t handle = 0;
    std::uint64_t fieldMask = 0;
    std::uint32_t id = 0;
    std::uint8_t kind = 0;
    std::int32_t team = -1;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    std::int32_t health = 100;
    std::int32_t max_health = 100;
    std::uint32_t flags = 0;
    char name[64] = {};
};

struct PlayerCoreV2 {
    std::uint32_t structSize = sizeof(PlayerCoreV2);
    std::uint16_t version = 2;
    std::uint16_t reserved = 0;
    std::uint32_t handle = 0;
    std::uint64_t fieldMask = 0;
    std::uint32_t id = 0;
    std::uint8_t kind = 0;
    std::int32_t team = -1;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    std::int32_t health = 100;
    std::int32_t max_health = 100;
    std::uint32_t flags = 0;
    char name[64] = {};
    std::uint64_t migration_epoch = 1;
};

static_assert(sizeof(PlayerCoreV1) != sizeof(PlayerCoreV2),
              "v1/v2 sizes must differ for host migration detection");
static_assert(sizeof(PlayerCoreV2) !=
                  sizeof(mimita::host::state::player_core::Record),
              "v2/v3 sizes must differ for host migration detection");

void logMigration(const char* eventName, const char* reason,
                  std::uint32_t oldSize, std::uint32_t newSize,
                  std::uint64_t epoch) {
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important,
        eventName, "player-core-migration", reason,
        static_cast<std::uint32_t>(epoch),
        nlohmann::json{{"domain", "player_core"},
                       {"old_size", oldSize},
                       {"new_size", newSize},
                       {"migration_epoch", epoch},
                       {"path", "dll"}},
        __FILE__, __LINE__, __func__);
}

void copyCommon(const PlayerCoreV1& oldValue,
                mimita::host::state::player_core::Record& newValue) {
    newValue.handle = oldValue.handle;
    newValue.fieldMask = oldValue.fieldMask;
    newValue.id = oldValue.id;
    newValue.kind = oldValue.kind;
    newValue.team = oldValue.team;
    std::memcpy(newValue.position, oldValue.position,
                sizeof(newValue.position));
    std::memcpy(newValue.velocity, oldValue.velocity,
                sizeof(newValue.velocity));
    newValue.health = oldValue.health;
    newValue.max_health = oldValue.max_health;
    std::snprintf(newValue.name, sizeof(newValue.name), "%s", oldValue.name);
    newValue.migration_epoch = 1;
    newValue.migration_revision = 3;
}

void copyCommon(const PlayerCoreV2& oldValue,
                mimita::host::state::player_core::Record& newValue) {
    newValue.handle = oldValue.handle;
    newValue.fieldMask = oldValue.fieldMask;
    newValue.id = oldValue.id;
    newValue.kind = oldValue.kind;
    newValue.team = oldValue.team;
    std::memcpy(newValue.position, oldValue.position,
                sizeof(newValue.position));
    std::memcpy(newValue.velocity, oldValue.velocity,
                sizeof(newValue.velocity));
    newValue.health = oldValue.health;
    newValue.max_health = oldValue.max_health;
    std::snprintf(newValue.name, sizeof(newValue.name), "%s", oldValue.name);
    newValue.migration_epoch = oldValue.migration_epoch;
    newValue.migration_revision = 3;
}

bool migrateToV2(void* oldBlock, std::uint32_t oldSize, void* newBlock,
                 std::uint32_t newSize) {
    if (!oldBlock || !newBlock || oldSize < sizeof(PlayerCoreV1) ||
        newSize < sizeof(PlayerCoreV2)) {
        logMigration("state.migration.reject",
                     "invalid v1 to v2 migration buffers", oldSize, newSize, 0);
        return false;
    }
    const auto& old = *static_cast<const PlayerCoreV1*>(oldBlock);
    auto& next = *static_cast<PlayerCoreV2*>(newBlock);
    next = PlayerCoreV2{};
    next.handle = old.handle;
    next.fieldMask = old.fieldMask;
    next.id = old.id;
    next.kind = old.kind;
    next.team = old.team;
    std::memcpy(next.position, old.position, sizeof(next.position));
    std::memcpy(next.velocity, old.velocity, sizeof(next.velocity));
    next.health = old.health;
    next.max_health = old.max_health;
    std::snprintf(next.name, sizeof(next.name), "%s", old.name);
    next.migration_epoch = 1;
    logMigration("state.migration.commit",
                 "player_core v1 migrated to v2", oldSize, newSize,
                 next.migration_epoch);
    return true;
}

bool migrateToV3(void* oldBlock, std::uint32_t oldSize, void* newBlock,
                 std::uint32_t newSize) {
    if (!oldBlock || !newBlock ||
        newSize < sizeof(mimita::host::state::player_core::Record)) {
        logMigration("state.migration.reject", "invalid migration buffers",
                     oldSize, newSize, 0);
        return false;
    }
    auto& next =
        *static_cast<mimita::host::state::player_core::Record*>(newBlock);
    next = mimita::host::state::player_core::Record{};
    if (oldSize == sizeof(PlayerCoreV1)) {
        const auto& old = *static_cast<const PlayerCoreV1*>(oldBlock);
        copyCommon(old, next);
        logMigration("state.migration.commit",
                     "player_core v1 migrated to v3", oldSize, newSize,
                     next.migration_epoch);
        return true;
    }
    if (oldSize == sizeof(PlayerCoreV2)) {
        const auto& old = *static_cast<const PlayerCoreV2*>(oldBlock);
        if (old.migration_epoch == kPoisonEpoch) {
            logMigration("state.migration.reject",
                         "poisoned v2 migration rejected atomically",
                         oldSize, newSize, old.migration_epoch);
            return false;
        }
        copyCommon(old, next);
        logMigration("state.migration.commit",
                     "player_core v2 migrated to v3", oldSize, newSize,
                     next.migration_epoch);
        return true;
    }
    logMigration("state.migration.reject",
                 "unsupported player_core source version", oldSize, newSize, 0);
    return false;
}

bool seedV1(void*, std::uint32_t, void* newBlock, std::uint32_t newSize) {
    if (!newBlock || newSize < sizeof(PlayerCoreV1)) return false;
    auto& value = *static_cast<PlayerCoreV1*>(newBlock);
    value = PlayerCoreV1{};
    value.id = 42;
    value.kind = 1;
    value.health = 77;
    value.max_health = 77;
    std::snprintf(value.name, sizeof(value.name), "%s", "player-core-v1");
    logMigration("state.migration.prepare",
                 "seed player_core v1", 0, newSize, 0);
    return true;
}

bool seedV2(void*, std::uint32_t, void* newBlock, std::uint32_t newSize) {
    if (!newBlock || newSize < sizeof(PlayerCoreV2)) return false;
    auto& value = *static_cast<PlayerCoreV2*>(newBlock);
    value = PlayerCoreV2{};
    value.id = 42;
    value.kind = 1;
    value.health = 77;
    value.max_health = 77;
    value.migration_epoch = 1;
    std::snprintf(value.name, sizeof(value.name), "%s", "player-core-v2");
    logMigration("state.migration.prepare",
                 "seed player_core v2", 0, newSize, value.migration_epoch);
    return true;
}

bool seedV2Rejected(void* oldBlock, std::uint32_t oldSize, void* newBlock,
                    std::uint32_t newSize) {
    if (!seedV2(oldBlock, oldSize, newBlock, newSize)) return false;
    static_cast<PlayerCoreV2*>(newBlock)->migration_epoch = kPoisonEpoch;
    logMigration("state.migration.prepare",
                 "seed poisoned player_core v2", oldSize, newSize,
                 kPoisonEpoch);
    return true;
}

void* ensureObject(std::size_t size, std::size_t alignment,
                   StateMigrateFn migrate, bool& created) {
    created = false;
    return ensureResidentObjectMigrating(
        StateObjectId::PlayerCore, size, alignment, migrate, &created);
}

}  // namespace

bool ensurePlayerCoreState() {
    bool created = false;
    void* result = ensureObject(
        sizeof(mimita::host::state::player_core::Record),
        alignof(mimita::host::state::player_core::Record), migrateToV3,
        created);
    if (!result) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "state.migration.reject", "player-core-migration",
            "player_core migration failed; previous resident state retained", 0,
            nlohmann::json{{"domain", "player_core"},
                           {"created", created},
                           {"source", "generation-bootstrap"}},
            __FILE__, __LINE__, __func__);
        return false;
    }
    if (created) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "state.player-core.created", "player-core-migration",
            "created current player_core state", 0,
            nlohmann::json{{"domain", "player_core"},
                           {"version", 3},
                           {"path", "dll"}},
            __FILE__, __LINE__, __func__);
    }
    return true;
}

bool seedPlayerCoreState(int version, bool reject) {
    bool created = false;
    if (version == 1) {
        void* result = ensureObject(sizeof(PlayerCoreV1), alignof(PlayerCoreV1),
                                    seedV1, created);
        return result != nullptr;
    }
    if (version == 2) {
        void* result = ensureObject(sizeof(PlayerCoreV2), alignof(PlayerCoreV2),
                                    reject ? seedV2Rejected : seedV2, created);
        return result != nullptr;
    }
    return false;
}

bool migratePlayerCoreStateToV2() {
    bool created = false;
    void* result = ensureObject(sizeof(PlayerCoreV2), alignof(PlayerCoreV2),
                                migrateToV2, created);
    if (!result) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Errors,
            "state.migration.reject", "player-core-migration",
            "player_core v1 to v2 migration failed", 0,
            nlohmann::json{{"domain", "player_core"},
                           {"target_version", 2},
                           {"created", created}},
            __FILE__, __LINE__, __func__);
        return false;
    }
    return true;
}

bool migratePlayerCoreState() {
    return ensurePlayerCoreState();
}

}  // namespace mimita::game
