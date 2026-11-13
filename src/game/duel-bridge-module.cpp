// 2026-11-13
// Game-DLL-side CAP_GAMEMODE duel bridge.
//
// The resident DuelQueue owns queue polling, server launch/join, and HUD state.
// The game generation forwards calls and receives bounded packet/text data.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "duel/duel-queue.h"
#include "host/abi/game-host-abi.h"
#include "host/abi/gamemode-capability.h"
#include "network/packets.h"

namespace mimita::game {

void initializeHostDuel(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::GamemodeCapabilityV1* gDuel = nullptr;
void* gHostData = nullptr;

bool invoke(mimita::host::GamemodeOp op, const void* input,
            std::uint32_t inputSize, void* output, std::uint32_t outputSize) {
    if (!gDuel || !gDuel->invoke) {
        return false;
    }
    return gDuel->invoke(gHostData, op, input, inputSize, output, outputSize) != 0;
}

void copyText(char* destination, std::size_t capacity, const std::string& value) {
    if (!destination || capacity == 0) return;
    const std::size_t count = value.size() < capacity - 1 ? value.size() : capacity - 1;
    std::memcpy(destination, value.data(), count);
    destination[count] = 0;
}
}  // namespace

void initializeHostDuel(const mimita::host::HostServicesV1* services) {
    gDuel = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::GamemodeCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Gamemode),
            mimita::host::kGamemodeCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kGamemodeCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::GamemodeCapabilityV1)) {
        gDuel = capability;
    }
}

}  // namespace mimita::game

DuelQueue& DuelQueue::instance() {
    static DuelQueue queue;
    return queue;
}

void DuelQueue::startQueue(const std::string& profileId, const std::string& name,
                           const std::string& preferOpponentId,
                           const std::vector<std::string>& maps) {
    mimita::host::GamemodeTextArgs args{};
    mimita::game::copyText(args.s0, sizeof(args.s0), profileId);
    mimita::game::copyText(args.s1, sizeof(args.s1), name);
    mimita::game::copyText(args.s2, sizeof(args.s2), preferOpponentId);
    const std::size_t count = maps.size() < 32 ? maps.size() : 32;
    for (std::size_t i = 0; i < count; ++i) {
        mimita::game::copyText(args.maps[i], sizeof(args.maps[i]), maps[i]);
    }
    args.mapCount = static_cast<std::uint32_t>(count);
    mimita::game::invoke(mimita::host::GamemodeOp::DuelStartQueue, &args,
                         sizeof(args), nullptr, 0);
}

std::string DuelQueue::defaultProfileId() {
    char output[256]{};
    mimita::game::invoke(mimita::host::GamemodeOp::DuelDefaultProfileId, nullptr, 0,
                         output, sizeof(output));
    return std::string(output);
}

void DuelQueue::stopQueue() {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelStopQueue, nullptr, 0,
                         nullptr, 0);
}

void DuelQueue::returnToQueue() {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelReturnToQueue, nullptr, 0,
                         nullptr, 0);
}

void DuelQueue::update(float dt) {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelUpdate, &dt, sizeof(dt),
                         nullptr, 0);
}

void DuelQueue::onDuelState(const MimitaNet::DuelStatePacket& pkt) {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelOnState, &pkt, sizeof(pkt),
                         nullptr, 0);
}

void DuelQueue::onDuelEnemySpawn(const MimitaNet::DuelEnemySpawnPacket& pkt) {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelOnEnemySpawn, &pkt,
                         sizeof(pkt), nullptr, 0);
}

bool DuelQueue::countdownActive() const {
    std::int32_t active = 0;
    mimita::game::invoke(mimita::host::GamemodeOp::DuelCountdownActive, nullptr, 0,
                         &active, sizeof(active));
    return active != 0;
}

float DuelQueue::countdownLeft() const {
    float seconds = 0.0f;
    mimita::game::invoke(mimita::host::GamemodeOp::DuelCountdownLeft, nullptr, 0,
                         &seconds, sizeof(seconds));
    return seconds;
}

void DuelQueue::requestRematchNow() {
    mimita::game::invoke(mimita::host::GamemodeOp::DuelRequestRematchNow, nullptr, 0,
                         nullptr, 0);
}

void DuelQueue::requestRematchWith(const std::string& opponentId) {
    mimita::host::GamemodeTextArgs args{};
    mimita::game::copyText(args.s0, sizeof(args.s0), opponentId);
    mimita::game::invoke(mimita::host::GamemodeOp::DuelRequestRematchWith, &args,
                         sizeof(args), nullptr, 0);
}

void DuelQueue::onMapChange(const std::string& mapId, std::uint32_t duelId,
                            std::uint32_t mapVersion) {
    mimita::host::GamemodeTextArgs args{};
    mimita::game::copyText(args.s0, sizeof(args.s0), mapId);
    args.u0 = duelId;
    args.u1 = mapVersion;
    mimita::game::invoke(mimita::host::GamemodeOp::DuelOnMapChange, &args,
                         sizeof(args), nullptr, 0);
}
