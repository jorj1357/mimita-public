// 2026-11-17
// Purpose: resident transport/ICE/session mechanism used across a game
// generation swap.
//
// The resident host owns sockets, libjuice agents, callback threads, raw
// queues, and maintenance payload bytes. Game code refers to them by opaque
// handles only and never keeps a host pointer after unload.
//
// This file DOES NOT make join/reconnect/ICE/session decisions. Those belong to
// the replaceable game generation.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "network/game-transport.h"

namespace mimita::host {

using TransportHandle = std::uint64_t;
using SessionHandle = std::uint64_t;

inline constexpr TransportHandle kInvalidTransport = 0;
inline constexpr SessionHandle kInvalidSession = 0;

// ── Resident transport registry ──────────────────────────────────────
TransportHandle registerTransport(std::unique_ptr<IGameTransport> transport);
bool pollTransport(TransportHandle handle, std::vector<ReceivedPacket>& out);
bool sendTransport(TransportHandle handle, const void* data, std::size_t size);
void closeTransport(TransportHandle handle);
void closeAllTransports();

// ── Resident session registry ────────────────────────────────────────
SessionHandle openSession(std::uint32_t role, std::uint64_t connectionKey);
SessionHandle attachSession(std::uint64_t connectionKey);
void closeSession(SessionHandle handle);
bool setMaintenancePayload(SessionHandle handle, const void* bytes,
                           std::uint32_t size, std::uint32_t intervalMs);
void clearMaintenancePayload(SessionHandle handle);

// ── Swap bookkeeping ─────────────────────────────────────────────────
void setGenerationAttached(bool attached);
void noteSwapSuspend();
void noteSwapResume();
struct SwapSnapshot {
    std::uint32_t generationAttached = 0;
    std::uint32_t suspended = 0;
    std::uint32_t sessionCount = 0;
    std::uint32_t queuedEvents = 0;
    std::uint64_t suspends = 0;
    std::uint64_t resumes = 0;
    std::uint64_t activeGeneration = 0;
};
SwapSnapshot querySwapSnapshot();

// ── Resident ICE agent registry ──────────────────────────────────────
bool registerIceAgent(const std::string& stableKey, void* handle);
void* attachIceAgent(const std::string& stableKey);
void unregisterIceAgent(const std::string& stableKey);

}  // namespace mimita::host
