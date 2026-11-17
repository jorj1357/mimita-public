// 2026-11-17
// Resident implementation of the transport/ICE/session mechanism.
#include "host/net-core-primitive.h"

#include <mutex>
#include <unordered_map>
#include <utility>

namespace mimita::host {
namespace {

struct TransportEntry {
    std::unique_ptr<IGameTransport> transport;
};

struct SessionEntry {
    std::uint32_t role = 0;
    std::uint64_t connectionKey = 0;
    std::vector<std::uint8_t> maintenance;
    std::uint32_t maintenanceIntervalMs = 0;
};

std::mutex gMutex;
std::unordered_map<TransportHandle, TransportEntry> gTransports;
std::unordered_map<SessionHandle, SessionEntry> gSessions;
std::unordered_map<std::string, void*> gIceAgents;
TransportHandle gNextTransport = 1;
SessionHandle gNextSession = 1;
bool gGenerationAttached = false;
bool gSuspended = false;
std::uint64_t gSuspends = 0;
std::uint64_t gResumes = 0;
std::uint64_t gActiveGeneration = 0;

}  // namespace

TransportHandle registerTransport(std::unique_ptr<IGameTransport> transport) {
    if (!transport) return kInvalidTransport;
    std::lock_guard<std::mutex> lock(gMutex);
    const TransportHandle handle = gNextTransport++;
    gTransports.emplace(handle, TransportEntry{std::move(transport)});
    return handle;
}

bool pollTransport(TransportHandle handle, std::vector<ReceivedPacket>& out) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gTransports.find(handle);
    if (it == gTransports.end() || !it->second.transport) return false;
    it->second.transport->poll(out);
    return true;
}

bool sendTransport(TransportHandle handle, const void* data, std::size_t size) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gTransports.find(handle);
    if (it == gTransports.end() || !it->second.transport) return false;
    return it->second.transport->send(data, size);
}

void closeTransport(TransportHandle handle) {
    std::unique_ptr<IGameTransport> transport;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gTransports.find(handle);
        if (it == gTransports.end()) return;
        transport = std::move(it->second.transport);
        gTransports.erase(it);
    }
    if (transport) transport->close();
}

void closeAllTransports() {
    std::unordered_map<TransportHandle, TransportEntry> transports;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        transports.swap(gTransports);
    }
    for (auto& entry : transports) {
        if (entry.second.transport) entry.second.transport->close();
    }
}

SessionHandle openSession(std::uint32_t role, std::uint64_t connectionKey) {
    std::lock_guard<std::mutex> lock(gMutex);
    for (const auto& entry : gSessions) {
        if (entry.second.connectionKey == connectionKey) return entry.first;
    }
    const SessionHandle handle = gNextSession++;
    SessionEntry session;
    session.role = role;
    session.connectionKey = connectionKey;
    gSessions.emplace(handle, std::move(session));
    return handle;
}

SessionHandle attachSession(std::uint64_t connectionKey) {
    std::lock_guard<std::mutex> lock(gMutex);
    for (const auto& entry : gSessions) {
        if (entry.second.connectionKey == connectionKey) return entry.first;
    }
    return kInvalidSession;
}

void closeSession(SessionHandle handle) {
    std::lock_guard<std::mutex> lock(gMutex);
    gSessions.erase(handle);
}

bool setMaintenancePayload(SessionHandle handle, const void* bytes,
                           std::uint32_t size, std::uint32_t intervalMs) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gSessions.find(handle);
    if (it == gSessions.end()) return false;
    it->second.maintenance.clear();
    if (bytes && size) {
        const auto* begin = static_cast<const std::uint8_t*>(bytes);
        it->second.maintenance.assign(begin, begin + size);
    }
    it->second.maintenanceIntervalMs = intervalMs;
    return true;
}

void clearMaintenancePayload(SessionHandle handle) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gSessions.find(handle);
    if (it == gSessions.end()) return;
    it->second.maintenance.clear();
    it->second.maintenanceIntervalMs = 0;
}

void setGenerationAttached(bool attached) {
    std::lock_guard<std::mutex> lock(gMutex);
    gGenerationAttached = attached;
    if (attached) ++gActiveGeneration;
}

void noteSwapSuspend() {
    std::lock_guard<std::mutex> lock(gMutex);
    gSuspended = true;
    ++gSuspends;
}

void noteSwapResume() {
    std::lock_guard<std::mutex> lock(gMutex);
    gSuspended = false;
    ++gResumes;
}

SwapSnapshot querySwapSnapshot() {
    std::lock_guard<std::mutex> lock(gMutex);
    SwapSnapshot snapshot;
    snapshot.generationAttached = gGenerationAttached ? 1u : 0u;
    snapshot.suspended = gSuspended ? 1u : 0u;
    snapshot.sessionCount = static_cast<std::uint32_t>(gSessions.size());
    snapshot.suspends = gSuspends;
    snapshot.resumes = gResumes;
    snapshot.activeGeneration = gActiveGeneration;
    return snapshot;
}

bool registerIceAgent(const std::string& stableKey, void* handle) {
    if (stableKey.empty() || !handle) return false;
    std::lock_guard<std::mutex> lock(gMutex);
    gIceAgents[stableKey] = handle;
    return true;
}

void* attachIceAgent(const std::string& stableKey) {
    std::lock_guard<std::mutex> lock(gMutex);
    auto it = gIceAgents.find(stableKey);
    return it == gIceAgents.end() ? nullptr : it->second;
}

void unregisterIceAgent(const std::string& stableKey) {
    std::lock_guard<std::mutex> lock(gMutex);
    gIceAgents.erase(stableKey);
}

}  // namespace mimita::host
