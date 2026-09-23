// 09 23 2026
/* purpose
* Central names for the live-networking JSONL events so the reload, packet,
* transport, and reconnect stages all record the same event vocabulary.
* Strings only: no state, no policy, no reload logic.
* Does NOT own the journal (see live-journal.h) or decide when an event fires.
*/
#pragma once

namespace LiveNetEvents {

// Reload lifecycle.
constexpr const char* kReloadRequested = "network.reload.requested";
constexpr const char* kBarrierEntered = "network.reload.barrier_entered";
constexpr const char* kBarrierReleased = "network.reload.barrier_released";
constexpr const char* kStateSerialized = "network.state_serialized";
constexpr const char* kStateMigrated = "network.state_migrated";
constexpr const char* kGenerationActivated = "network.generation_activated";
constexpr const char* kGenerationRollback = "network.generation_rollback";

// Packet schema negotiation and codec.
constexpr const char* kPacketSchemaAnnounced = "network.packet_schema_announced";
constexpr const char* kPacketSchemaNegotiated = "network.packet_schema_negotiated";
constexpr const char* kPacketEncode = "network.packet_encode";
constexpr const char* kPacketDecode = "network.packet_decode";
constexpr const char* kPacketRejected = "network.packet_rejected";

// Transport / connection continuity.
constexpr const char* kSocketPreserved = "network.socket_preserved";
constexpr const char* kConnectionPreserved = "network.connection_preserved";
constexpr const char* kReconnectStarted = "network.reconnect_started";
constexpr const char* kReconnectAttempt = "network.reconnect_attempt";
constexpr const char* kReconnectSucceeded = "network.reconnect_succeeded";
constexpr const char* kReconnectFailed = "network.reconnect_failed";
constexpr const char* kIceStateChanged = "network.ice_state_changed";

// Tick / gameplay ownership.
constexpr const char* kServerTick = "network.server_tick";
constexpr const char* kClientTick = "network.client_tick";
constexpr const char* kNpcDied = "network.npc_died";
constexpr const char* kProjectileCancelled = "network.projectile_cancelled";
constexpr const char* kDamageApplied = "network.damage_applied";
constexpr const char* kReconciliationApplied = "network.reconciliation_applied";

// Shutdown / crash boundary.
constexpr const char* kShutdownRequested = "network.shutdown_requested";
constexpr const char* kShutdownCompleted = "network.shutdown_completed";
constexpr const char* kCrashBoundaryMissing = "network.crash_boundary_missing";

} // namespace LiveNetEvents
