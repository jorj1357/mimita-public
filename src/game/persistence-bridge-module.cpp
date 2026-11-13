// 2026-11-13
// Game-DLL-side CAP_PERSISTENCE bridge.
//
// The resident PersistenceQueue owns the backend worker, session state, and
// notices. The game forwards bounded records and receives copied notices.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/persistence-capability.h"
#include "persistence/persistence-queue.h"

namespace mimita::game {

void initializeHostPersistence(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::PersistenceCapabilityV1* gPersistence = nullptr;
void* gHostData = nullptr;

bool invoke(mimita::host::PersistenceOp op, const void* input,
            std::uint32_t inputSize, void* output, std::uint32_t outputSize) {
    if (!gPersistence || !gPersistence->invoke) return false;
    return gPersistence->invoke(gHostData, op, input, inputSize, output,
                                outputSize) != 0;
}

void copyText(char* destination, std::size_t capacity, const std::string& value) {
    if (!destination || capacity == 0) return;
    const std::size_t count = value.size() < capacity - 1 ? value.size() : capacity - 1;
    std::memcpy(destination, value.data(), count);
    destination[count] = 0;
}
}  // namespace

void initializeHostPersistence(const mimita::host::HostServicesV1* services) {
    gPersistence = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::PersistenceCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Persistence),
            mimita::host::kPersistenceCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kPersistenceCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::PersistenceCapabilityV1)) {
        gPersistence = capability;
    }
}

}  // namespace mimita::game

PersistenceQueue& PersistenceQueue::instance() {
    static PersistenceQueue queue;
    return queue;
}

PersistenceQueue::~PersistenceQueue() = default;

void PersistenceQueue::observePlayer(std::uint32_t playerId, std::uint64_t userId,
                                     const std::string& name,
                                     const std::string& ticket, bool active) {
    mimita::host::PersistenceObserveArgs args{};
    args.playerId = playerId;
    args.userId = userId;
    args.active = active ? 1 : 0;
    mimita::game::copyText(args.name, sizeof(args.name), name);
    mimita::game::copyText(args.ticket, sizeof(args.ticket), ticket);
    mimita::game::invoke(mimita::host::PersistenceOp::ObservePlayer, &args,
                         sizeof(args), nullptr, 0);
}

void PersistenceQueue::tick() {
    mimita::game::invoke(mimita::host::PersistenceOp::Tick, nullptr, 0, nullptr, 0);
}

void PersistenceQueue::enqueueKill(const PersistenceKillEvent& event) {
    mimita::game::invoke(mimita::host::PersistenceOp::EnqueueKill, &event,
                         sizeof(event), nullptr, 0);
}

void PersistenceQueue::enqueueMatchResult(const PersistenceMatchEvent& event) {
    mimita::game::invoke(mimita::host::PersistenceOp::EnqueueMatchResult, &event,
                         sizeof(event), nullptr, 0);
}

std::vector<ProgressionNotice> PersistenceQueue::takeNotices() {
    std::vector<ProgressionNotice> notices;
    mimita::game::invoke(mimita::host::PersistenceOp::TakeNotices, nullptr, 0,
                         &notices, sizeof(notices));
    return notices;
}

std::string PersistenceQueue::sessionId() const {
    char output[256]{};
    mimita::game::invoke(mimita::host::PersistenceOp::SessionId, nullptr, 0,
                         output, sizeof(output));
    return std::string(output);
}

std::size_t PersistenceQueue::queueDepth() const {
    std::uint64_t depth = 0;
    mimita::game::invoke(mimita::host::PersistenceOp::QueueDepth, nullptr, 0,
                         &depth, sizeof(depth));
    return static_cast<std::size_t>(depth);
}

std::uint64_t PersistenceQueue::totalSent() const {
    std::uint64_t value = 0;
    mimita::game::invoke(mimita::host::PersistenceOp::TotalSent, nullptr, 0,
                         &value, sizeof(value));
    return value;
}

std::uint64_t PersistenceQueue::totalRetries() const {
    std::uint64_t value = 0;
    mimita::game::invoke(mimita::host::PersistenceOp::TotalRetries, nullptr, 0,
                         &value, sizeof(value));
    return value;
}

std::uint64_t PersistenceQueue::totalDuplicates() const {
    std::uint64_t value = 0;
    mimita::game::invoke(mimita::host::PersistenceOp::TotalDuplicates, nullptr, 0,
                         &value, sizeof(value));
    return value;
}
