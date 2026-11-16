#pragma once

namespace mimita::game {

// Create or migrate the resident player_core domain to the current schema.
// Returns false only when the host rejected the migration; the old state is
// then still active.
bool ensurePlayerCoreState();

// Dev-loop proof helpers. `version` is 1 or 2; `reject` seeds the v2 poison
// value so the v2 -> v3 migration must be rejected atomically.
bool seedPlayerCoreState(int version, bool reject);

// Force a migration attempt in the current generation (same code path as a
// generation bootstrap).
bool migratePlayerCoreState();

// Stage the explicit v1 -> v2 add-field step. The next generation bootstrap
// then proves v2 -> v3 remove-field.
bool migratePlayerCoreStateToV2();

}  // namespace mimita::game
