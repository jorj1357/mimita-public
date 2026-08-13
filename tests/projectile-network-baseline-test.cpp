// 07 20 2026, 19 40
/* purpose
* Protects the Stage 0 rocket and grenade network authority baseline.
* Exercises generic AttackRequest routing, projectile idempotency, and spawn lifetime rejection.
* Verifies shared projectile simulation damage, death, respawn fire, prediction, and rejection helpers.
* Does NOT open sockets, render, play audio, or modify movement behavior.
* Does NOT replace full ICE or two-client end-to-end validation.
* Does NOT migrate non-projectile weapons to the generic attack pipeline.
*/

#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "combat/projectile-simulation.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-rocket-launcher.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace MimitaNet;

namespace MimitaNet {

const char* serverTimestamp()
{
    return "[projectile-network-baseline-test]";
}

bool sameAddress(const sockaddr_in& a, const sockaddr_in& b)
{
    return a.sin_family == b.sin_family &&
           a.sin_port == b.sin_port &&
           a.sin_addr.s_addr == b.sin_addr.s_addr;
}

uint64_t nowMs()
{
    static uint64_t current = 100000;
    current += 16;
    return current;
}

bool serverSendToPlayer(SOCKET, const ServerPlayer& player, const void* data, size_t size)
{
    if (!player.transport)
        return true;
    return player.transport->send(data, size);
}

uint32_t serverReliableEventSessionId()
{
    return 9001;
}

uint32_t nextReliableGameplayEventId()
{
    static uint32_t nextId = 1;
    return nextId++;
}

uint32_t reliableGameplayEventSessionForPlayer(ServerPlayer& player)
{
    if (player.reliableEventSessionId == 0)
        player.reliableEventSessionId = serverReliableEventSessionId();
    return player.reliableEventSessionId;
}

ReliableGameplayEventQueueResult queueReliableGameplayEventToAll(
    SOCKET,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const void* data,
    size_t size,
    uint32_t,
    uint32_t,
    uint64_t& totalPacketsOut)
{
    for (auto& entry : players)
    {
        if (entry.second.transport)
            entry.second.transport->send(data, size);
        ++totalPacketsOut;
    }
    return ReliableGameplayEventQueueResult::Queued;
}

void beginAuthoritativeTransform(ServerPlayer& player,
    const glm::vec3& position, const glm::vec3& velocity, float yaw,
    const char*)
{
    player.pos = position;
    player.vel = velocity;
    player.yaw = yaw;
    ++player.transformEpoch;
    player.awaitingAuthoritativeTransformAck = true;
    player.authoritativeTransformPosition = position;
    player.authoritativeTransformEpoch = player.transformEpoch;
}

} // namespace MimitaNet

namespace {

int gFailures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    }
}

template <typename A, typename B>
void checkEq(const A& actual, const B& expected, const std::string& message)
{
    if (!(actual == expected))
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    }
}

class CaptureTransport final : public IGameTransport
{
public:
    bool send(const void* data, size_t size) override
    {
        const auto* first = static_cast<const uint8_t*>(data);
        sent.emplace_back(first, first + size);
        return true;
    }

    void poll(std::vector<ReceivedPacket>&) override {}
    bool connected() const override { return true; }
    void close() override {}

    void clear()
    {
        sent.clear();
    }

    int count(uint8_t type) const
    {
        int out = 0;
        for (const auto& packet : sent)
        {
            if (packet.size() < sizeof(PacketHeader))
                continue;
            const auto* header = reinterpret_cast<const PacketHeader*>(packet.data());
            if (header->type == type)
                ++out;
        }
        return out;
    }

    template <typename Packet>
    std::vector<Packet> packets(uint8_t type) const
    {
        std::vector<Packet> out;
        for (const auto& bytes : sent)
        {
            if (bytes.size() < sizeof(Packet))
                continue;
            const auto* header = reinterpret_cast<const PacketHeader*>(bytes.data());
            if (header->type != type)
                continue;
            Packet packet{};
            std::memcpy(&packet, bytes.data(), sizeof(Packet));
            out.push_back(packet);
        }
        return out;
    }

private:
    std::vector<std::vector<uint8_t>> sent;
};

sockaddr_in loopbackAddress(uint16_t port)
{
    sockaddr_in out{};
    out.sin_family = AF_INET;
    out.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &out.sin_addr);
    return out;
}

struct Fixture
{
    Fixture()
    {
        players.reserve(8);
        projectiles.reserve(8);
    }

    HeadlessWorld world;
    std::unordered_map<uint32_t, ServerPlayer> players;
    std::unordered_map<uint32_t, ServerNpc> npcs;
    std::unordered_map<uint32_t, ServerProjectile> projectiles;
    std::unordered_map<uint32_t, CaptureTransport*> captures;
    uint32_t nextProjectileId = 100;
    uint64_t totalPacketsOut = 0;

    ServerPlayer& addPlayer(uint32_t id, uint16_t port)
    {
        ServerPlayer player;
        player.id = id;
        player.name = "P" + std::to_string(id);
        player.addr = loopbackAddress(port);
        player.spawnState = ServerPlayer::Active;
        player.spawnGeneration = id;
        player.reliableEventSessionId = serverReliableEventSessionId();

        auto transport = std::make_unique<CaptureTransport>();
        CaptureTransport* raw = transport.get();
        player.transport = std::move(transport);

        auto inserted = players.emplace(id, std::move(player));
        captures[id] = raw;
        return inserted.first->second;
    }

    CaptureTransport& capture(uint32_t id)
    {
        return *captures.at(id);
    }

    void clearCaptures()
    {
        for (auto& entry : captures)
            entry.second->clear();
    }
};

struct ProjectileWeaponRefs
{
    const WeaponDefinition* rocket = nullptr;
    const WeaponDefinition* grenade = nullptr;
    uint16_t rocketNetId = 0;
    uint16_t grenadeNetId = 0;
};

ProjectileWeaponRefs ensureProjectileWeapons()
{
    static bool registered = false;
    if (!registered)
    {
        WeaponRegistry::instance().registerWeapon(WeaponData::createRocketLauncherDefinition());
        WeaponRegistry::instance().registerWeapon(WeaponData::createGrenadeLauncherDefinition());
        registered = true;
    }

    ProjectileWeaponRefs refs;
    refs.rocket = WeaponRegistry::instance().get("rocket_launcher");
    refs.grenade = WeaponRegistry::instance().get("grenade_launcher");
    if (refs.rocket)
        refs.rocketNetId = registerWeaponDefNetworkId(refs.rocket->id);
    if (refs.grenade)
        refs.grenadeNetId = registerWeaponDefNetworkId(refs.grenade->id);
    return refs;
}

ServerPlayer::ServerWeaponRuntime makeRuntime(const WeaponDefinition& def, int ammo)
{
    ServerPlayer::ServerWeaponRuntime runtime;
    runtime.magazineAmmo = ammo >= 0 ? ammo : def.magazineSize;
    auto reserveIt = def.customParams.find("reserveAmmo");
    runtime.reserveAmmo = reserveIt != def.customParams.end()
        ? static_cast<int>(reserveIt->second)
        : 0;
    runtime.nextAllowedFireTick = 0;
    runtime.reloading = false;
    runtime.reloadCompleteTick = 0;
    runtime.stateRevision = 0;
    runtime.initialized = true;
    return runtime;
}

void equipProjectileWeapon(ServerPlayer& player, const WeaponDefinition& def,
                           uint32_t spawnGeneration, int ammo = -1)
{
    player.spawnState = ServerPlayer::Active;
    player.spawnGeneration = spawnGeneration;
    player.dead = false;
    player.health = 100;
    player.equippedSlot = def.slot;
    player.ownedWeaponIds.clear();
    player.ownedWeaponIds.push_back(def.id);
    player.weaponRuntimes.clear();
    player.weaponRuntimes[def.id] = makeRuntime(def, ammo);
    player.projectileFireCooldown = 0.0f;
    player.nextProjectileFireTick = 0;
}

AttackRequestPacket makeAttackRequest(const ServerPlayer& player,
                                      const WeaponDefinition& def,
                                      uint32_t requestId,
                                      uint32_t tick,
                                      glm::vec3 origin,
                                      glm::vec3 direction,
                                      uint32_t spawnGenerationOverride = 0)
{
    AttackRequestPacket packet{};
    packet.header.type = PACKET_ATTACK_REQUEST;
    packet.header.tick = tick;
    packet.header.playerId = player.id;
    packet.requestId = requestId;
    packet.spawnGeneration = spawnGenerationOverride != 0
        ? spawnGenerationOverride
        : player.spawnGeneration;
    packet.clientSimulationTick = tick;
    packet.equippedSlot = static_cast<int16_t>(def.slot);
    packet.weaponDefNetworkId = weaponDefNetworkIdFor(def.id);
    packet.aimOriginX = origin.x;
    packet.aimOriginY = origin.y;
    packet.aimOriginZ = origin.z;
    packet.aimDirX = direction.x;
    packet.aimDirY = direction.y;
    packet.aimDirZ = direction.z;
    packet.muzzlePosX = origin.x;
    packet.muzzlePosY = origin.y;
    packet.muzzlePosZ = origin.z;
    packet.deterministicSeed = requestId * 2654435761u;
    return packet;
}

void sendGenericAttack(Fixture& fixture,
                       const ServerPlayer& sender,
                       const AttackRequestPacket& request,
                       uint32_t tick)
{
    handleAttackRequest(
        INVALID_SOCKET,
        sender.addr,
        reinterpret_cast<const char*>(&request),
        static_cast<int>(sizeof(request)),
        fixture.players,
        fixture.npcs,
        fixture.projectiles,
        fixture.nextProjectileId,
        fixture.world,
        tick,
        fixture.totalPacketsOut);
}

int runtimeAmmo(const ServerPlayer& player, const WeaponDefinition& def)
{
    auto it = player.weaponRuntimes.find(def.id);
    return it != player.weaponRuntimes.end() ? it->second.magazineAmmo : -9999;
}

ServerPlayer::ServerWeaponRuntime& runtimeFor(ServerPlayer& player, const WeaponDefinition& def)
{
    return player.weaponRuntimes[def.id];
}

std::optional<ProjectileFireResultPacket> onlyProjectileResult(CaptureTransport& capture)
{
    auto packets = capture.packets<ProjectileFireResultPacket>(PACKET_PROJECTILE_FIRE_RESULT);
    if (packets.empty())
        return std::nullopt;
    return packets.back();
}

std::optional<AttackResultPacket> onlyAttackResult(CaptureTransport& capture)
{
    auto packets = capture.packets<AttackResultPacket>(PACKET_ATTACK_RESULT);
    if (packets.empty())
        return std::nullopt;
    return packets.back();
}

void expectAcceptedProjectileFire(Fixture& fixture,
                                  const WeaponDefinition& def,
                                  uint8_t weaponType,
                                  uint32_t requestId,
                                  uint32_t tick)
{
    auto& shooter = fixture.players.at(1);
    fixture.clearCaptures();
    const int ammoBefore = runtimeAmmo(shooter, def);
    const size_t projectileCountBefore = fixture.projectiles.size();
    AttackRequestPacket request = makeAttackRequest(
        shooter, def, requestId, tick, shooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));

    sendGenericAttack(fixture, shooter, request, tick);

    auto& capture = fixture.capture(shooter.id);
    auto result = onlyProjectileResult(capture);
    auto spawns = capture.packets<ProjectileSpawnEventPacket>(PACKET_PROJECTILE_SPAWN_EVENT);

    check(result.has_value(), def.id + " accepted generic fire sends ProjectileFireResultPacket");
    checkEq(capture.count(PACKET_ATTACK_RESULT), 0, def.id + " accepted generic fire does not send AttackResultPacket");
    checkEq(fixture.projectiles.size(), projectileCountBefore + 1, def.id + " accepted fire creates one projectile");
    checkEq(runtimeAmmo(shooter, def), ammoBefore - 1, def.id + " accepted fire consumes one magazine round");
    check(shooter.nextProjectileFireTick > tick, def.id + " accepted fire sets projectile cooldown tick");
    checkEq(runtimeFor(shooter, def).stateRevision, 0u, def.id + " projectile path does not invent runtime state revision");

    if (result)
    {
        checkEq(result->accepted, static_cast<uint8_t>(1), def.id + " fire result accepted");
        checkEq(result->reason, static_cast<uint8_t>(PROJECTILE_FIRE_ACCEPTED), def.id + " accepted reason");
        checkEq(result->fireSerial, requestId, def.id + " fireSerial mirrors requestId");
        checkEq(result->weapon, weaponType, def.id + " fire result weapon type");
        check(result->projectileId != 0, def.id + " accepted result has projectile id");

        auto it = fixture.projectiles.find(result->projectileId);
        check(it != fixture.projectiles.end(), def.id + " projectile id exists in authoritative map");
        if (it != fixture.projectiles.end())
        {
            checkEq(it->second.ownerPlayerId, shooter.id, def.id + " projectile owner");
            checkEq(it->second.fireSerial, requestId, def.id + " projectile fireSerial");
            checkEq(it->second.weaponType, weaponType, def.id + " projectile weapon type");
        }
    }

    check(!spawns.empty(), def.id + " accepted fire broadcasts spawn event");
    if (!spawns.empty())
    {
        checkEq(spawns.back().ownerPlayerId, shooter.id, def.id + " spawn owner");
        checkEq(spawns.back().fireSerial, requestId, def.id + " spawn fireSerial");
        checkEq(spawns.back().weapon, weaponType, def.id + " spawn weapon type");
    }
}

void expectPreBridgeAttackReject(Fixture& fixture,
                                 const WeaponDefinition& def,
                                 const AttackRequestPacket& request,
                                 uint32_t tick,
                                 uint8_t reason,
                                 const std::string& label)
{
    auto& shooter = fixture.players.at(1);
    fixture.clearCaptures();
    const int ammoBefore = runtimeAmmo(shooter, def);
    const size_t projectileCountBefore = fixture.projectiles.size();

    sendGenericAttack(fixture, shooter, request, tick);

    auto& capture = fixture.capture(shooter.id);
    auto result = onlyAttackResult(capture);
    check(result.has_value(), label + " sends AttackResultPacket");
    checkEq(capture.count(PACKET_PROJECTILE_FIRE_RESULT), 0, label + " does not reach projectile result path");
    checkEq(capture.count(PACKET_PROJECTILE_SPAWN_EVENT), 0, label + " does not spawn projectile");
    checkEq(fixture.projectiles.size(), projectileCountBefore, label + " keeps projectile map unchanged");
    checkEq(runtimeAmmo(shooter, def), ammoBefore, label + " keeps ammo unchanged");
    if (result)
    {
        checkEq(result->accepted, static_cast<uint8_t>(0), label + " result rejected");
        checkEq(result->reason, reason, label + " rejection reason");
        checkEq(result->requestId, request.requestId, label + " request id echoed");
    }
}

void expectProjectileHandlerReject(Fixture& fixture,
                                   const WeaponDefinition& def,
                                   const AttackRequestPacket& request,
                                   uint32_t tick,
                                   uint8_t reason,
                                   const std::string& label)
{
    auto& shooter = fixture.players.at(1);
    fixture.clearCaptures();
    const int ammoBefore = runtimeAmmo(shooter, def);
    const size_t projectileCountBefore = fixture.projectiles.size();

    sendGenericAttack(fixture, shooter, request, tick);

    auto& capture = fixture.capture(shooter.id);
    auto result = onlyProjectileResult(capture);
    check(result.has_value(), label + " sends ProjectileFireResultPacket");
    checkEq(capture.count(PACKET_ATTACK_RESULT), 0, label + " does not send AttackResultPacket");
    checkEq(capture.count(PACKET_PROJECTILE_SPAWN_EVENT), 0, label + " does not spawn projectile");
    checkEq(fixture.projectiles.size(), projectileCountBefore, label + " keeps projectile map unchanged");
    checkEq(runtimeAmmo(shooter, def), ammoBefore, label + " keeps ammo unchanged");
    if (result)
    {
        checkEq(result->accepted, static_cast<uint8_t>(0), label + " result rejected");
        checkEq(result->reason, reason, label + " rejection reason");
        checkEq(result->fireSerial, request.requestId, label + " fireSerial mirrors requestId");
    }
}

void testWeaponRouting()
{
    ProjectileWeaponRefs refs = ensureProjectileWeapons();
    check(refs.rocket != nullptr, "rocket definition registered");
    check(refs.grenade != nullptr, "grenade definition registered");
    if (!refs.rocket || !refs.grenade)
        return;

    check(refs.rocketNetId != 0, "rocket has weaponDefNetworkId");
    check(refs.grenadeNetId != 0, "grenade has weaponDefNetworkId");
    check(refs.rocketNetId != refs.grenadeNetId, "rocket and grenade network ids differ");
    checkEq(networkWeaponTypeForDefinition(*refs.rocket), static_cast<uint8_t>(NETWORK_WEAPON_ROCKET_LAUNCHER), "rocket definition maps to rocket network weapon");
    checkEq(networkWeaponTypeForDefinition(*refs.grenade), static_cast<uint8_t>(NETWORK_WEAPON_GRENADE_LAUNCHER), "grenade definition maps to grenade network weapon");
    checkEq(networkWeaponTypeForSlot(refs.rocket->slot), static_cast<uint8_t>(NETWORK_WEAPON_ROCKET_LAUNCHER), "rocket slot maps to rocket network weapon");
    checkEq(networkWeaponTypeForSlot(refs.grenade->slot), static_cast<uint8_t>(NETWORK_WEAPON_GRENADE_LAUNCHER), "grenade slot maps to grenade network weapon");
    check(networkWeaponTypeIsProjectile(NETWORK_WEAPON_ROCKET_LAUNCHER), "rocket is projectile weapon");
    check(networkWeaponTypeIsProjectile(NETWORK_WEAPON_GRENADE_LAUNCHER), "grenade is projectile weapon");
    check(!networkWeaponTypeIsHitscan(NETWORK_WEAPON_ROCKET_LAUNCHER), "rocket is not hitscan");
    check(!networkWeaponTypeIsHitscan(NETWORK_WEAPON_GRENADE_LAUNCHER), "grenade is not hitscan");
    check(!networkWeaponTypeIsMelee(NETWORK_WEAPON_ROCKET_LAUNCHER), "rocket is not melee");
    check(!networkWeaponTypeIsMelee(NETWORK_WEAPON_GRENADE_LAUNCHER), "grenade is not melee");
}

void testGenericAcceptAndRejects(const WeaponDefinition& def,
                                 uint8_t weaponType,
                                 uint32_t baseRequestId)
{
    Fixture fixture;
    auto& shooter = fixture.addPlayer(1, static_cast<uint16_t>(4100 + baseRequestId % 200));
    shooter.pos = glm::vec3(0.0f);
    equipProjectileWeapon(shooter, def, 11);

    expectAcceptedProjectileFire(fixture, def, weaponType, baseRequestId, 100);

    uint32_t tick = static_cast<uint32_t>(fixture.players.at(1).nextProjectileFireTick + 10);
    auto& currentShooter = fixture.players.at(1);

    currentShooter.dead = true;
    AttackRequestPacket deadRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 1, tick,
        currentShooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    expectPreBridgeAttackReject(fixture, def, deadRequest, tick, 2, def.id + " dead pre-bridge rejection");

    currentShooter.dead = false;
    AttackRequestPacket staleRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 2, tick + 1,
        currentShooter.pos, glm::vec3(1.0f, 0.0f, 0.0f),
        currentShooter.spawnGeneration - 1);
    expectPreBridgeAttackReject(fixture, def, staleRequest, tick + 1, 8, def.id + " stale spawnGeneration rejection");

    runtimeFor(currentShooter, def).magazineAmmo = 0;
    runtimeFor(currentShooter, def).nextAllowedFireTick = 0;
    currentShooter.nextProjectileFireTick = 0;
    AttackRequestPacket emptyRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 3, tick + 2,
        currentShooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    expectPreBridgeAttackReject(fixture, def, emptyRequest, tick + 2, 3, def.id + " empty magazine pre-bridge rejection");

    runtimeFor(currentShooter, def).magazineAmmo = def.magazineSize;
    runtimeFor(currentShooter, def).nextAllowedFireTick = 0;
    currentShooter.nextProjectileFireTick = tick + 200;
    AttackRequestPacket cooldownRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 4, tick + 3,
        currentShooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    expectProjectileHandlerReject(
        fixture, def, cooldownRequest, tick + 3, PROJECTILE_FIRE_COOLDOWN,
        def.id + " projectile cooldown rejection");

    currentShooter.nextProjectileFireTick = 0;
    AttackRequestPacket badDirectionRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 5, tick + 4,
        currentShooter.pos, glm::vec3(0.0f));
    expectProjectileHandlerReject(
        fixture, def, badDirectionRequest, tick + 4, PROJECTILE_FIRE_DIRECTION_INVALID,
        def.id + " projectile direction rejection");

    currentShooter.nextProjectileFireTick = 0;
    const float nanValue = std::numeric_limits<float>::quiet_NaN();
    AttackRequestPacket nanDirectionRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 6, tick + 5,
        currentShooter.pos, glm::vec3(nanValue, 0.0f, 0.0f));
    expectProjectileHandlerReject(
        fixture, def, nanDirectionRequest, tick + 5, PROJECTILE_FIRE_DIRECTION_INVALID,
        def.id + " projectile non-finite direction rejection");

    currentShooter.nextProjectileFireTick = 0;
    AttackRequestPacket nanOriginRequest = makeAttackRequest(
        currentShooter, def, baseRequestId + 7, tick + 6,
        glm::vec3(nanValue, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    expectProjectileHandlerReject(
        fixture, def, nanOriginRequest, tick + 6, PROJECTILE_FIRE_ORIGIN_INVALID,
        def.id + " projectile non-finite origin rejection");
}

void testDuplicateAndDifferentRequestIds(const WeaponDefinition& def,
                                         uint8_t weaponType,
                                         uint32_t baseRequestId)
{
    Fixture fixture;
    auto& shooter = fixture.addPlayer(1, static_cast<uint16_t>(4300 + baseRequestId % 200));
    shooter.pos = glm::vec3(0.0f);
    equipProjectileWeapon(shooter, def, 21);

    fixture.clearCaptures();
    AttackRequestPacket first = makeAttackRequest(
        shooter, def, baseRequestId, 200, shooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, shooter, first, 200);

    auto firstResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(firstResult.has_value(), def.id + " initial duplicate test fire accepted");
    if (!firstResult)
        return;

    const uint32_t firstProjectileId = firstResult->projectileId;
    const int ammoAfterFirst = runtimeAmmo(fixture.players.at(1), def);
    const uint64_t cooldownAfterFirst = fixture.players.at(1).nextProjectileFireTick;
    const uint32_t stateRevisionAfterFirst = runtimeFor(fixture.players.at(1), def).stateRevision;
    const size_t projectileCountAfterFirst = fixture.projectiles.size();

    fixture.clearCaptures();
    sendGenericAttack(fixture, fixture.players.at(1), first, 201);
    auto duplicateResult = onlyProjectileResult(fixture.capture(shooter.id));
    auto duplicateSpawns = fixture.capture(shooter.id).packets<ProjectileSpawnEventPacket>(PACKET_PROJECTILE_SPAWN_EVENT);

    check(duplicateResult.has_value(), def.id + " duplicate fire resends projectile result");
    checkEq(fixture.projectiles.size(), projectileCountAfterFirst, def.id + " duplicate does not create projectile");
    checkEq(runtimeAmmo(fixture.players.at(1), def), ammoAfterFirst, def.id + " duplicate does not consume ammo twice");
    checkEq(fixture.players.at(1).nextProjectileFireTick, cooldownAfterFirst, def.id + " duplicate preserves projectile cooldown");
    checkEq(runtimeFor(fixture.players.at(1), def).stateRevision, stateRevisionAfterFirst, def.id + " duplicate preserves state revision");
    if (duplicateResult)
    {
        checkEq(duplicateResult->accepted, static_cast<uint8_t>(1), def.id + " duplicate result accepted");
        checkEq(duplicateResult->projectileId, firstProjectileId, def.id + " duplicate reuses projectile id");
        checkEq(duplicateResult->fireSerial, first.requestId, def.id + " duplicate reuses fire serial");
        checkEq(duplicateResult->weapon, weaponType, def.id + " duplicate keeps weapon type");
    }
    check(!duplicateSpawns.empty(), def.id + " duplicate resends authoritative spawn to requester");
    if (!duplicateSpawns.empty())
        checkEq(duplicateSpawns.back().projectileId, firstProjectileId, def.id + " duplicate spawn uses same projectile id");

    const uint32_t secondTick = static_cast<uint32_t>(cooldownAfterFirst + 5);
    fixture.clearCaptures();
    AttackRequestPacket second = makeAttackRequest(
        fixture.players.at(1), def, baseRequestId + 1, secondTick,
        fixture.players.at(1).pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, fixture.players.at(1), second, secondTick);

    auto secondResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(secondResult.has_value(), def.id + " different request id sends result");
    checkEq(fixture.projectiles.size(), projectileCountAfterFirst + 1, def.id + " different request id creates another projectile");
    checkEq(runtimeAmmo(fixture.players.at(1), def), ammoAfterFirst - 1, def.id + " different request id consumes another round");
    if (secondResult)
    {
        checkEq(secondResult->accepted, static_cast<uint8_t>(1), def.id + " different request accepted");
        check(secondResult->projectileId != firstProjectileId, def.id + " different request gets distinct projectile id");
        checkEq(secondResult->fireSerial, second.requestId, def.id + " different request has new fire serial");
    }
}

void testOldLifeRejectionAndRespawnFire(const WeaponDefinition& def,
                                        uint8_t weaponType,
                                        uint32_t baseRequestId)
{
    Fixture fixture;
    auto& shooter = fixture.addPlayer(1, static_cast<uint16_t>(4500 + baseRequestId % 200));
    shooter.pos = glm::vec3(0.0f);
    equipProjectileWeapon(shooter, def, 31);

    fixture.clearCaptures();
    AttackRequestPacket oldLifeRequest = makeAttackRequest(
        shooter, def, baseRequestId, 300, shooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, shooter, oldLifeRequest, 300);
    auto firstResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(firstResult.has_value(), def.id + " old-life setup accepted fire");
    if (!firstResult)
        return;

    const size_t projectileCountAfterOldLife = fixture.projectiles.size();
    const uint32_t oldGeneration = fixture.players.at(1).spawnGeneration;

    fixture.players.at(1).dead = true;
    fixture.players.at(1).health = 0;
    fixture.players.at(1).respawnSeconds = 2.0f;
    resetPlayerForSpawn(fixture.players.at(1), false);
    fixture.players.at(1).spawnState = ServerPlayer::Active;
    fixture.players.at(1).equippedSlot = def.slot;

    check(fixture.players.at(1).spawnGeneration == oldGeneration + 1, def.id + " respawn increments generation");
    check(!fixture.players.at(1).dead, def.id + " respawn clears dead");
    checkEq(fixture.players.at(1).health, 100, def.id + " respawn restores health");

    const int newLifeAmmoBefore = runtimeAmmo(fixture.players.at(1), def);
    const int newLifeHealthBefore = fixture.players.at(1).health;
    const glm::vec3 newLifeVelocityBefore = fixture.players.at(1).vel;
    const uint64_t newLifeCooldownBefore = fixture.players.at(1).nextProjectileFireTick;
    const uint32_t newLifeStateRevisionBefore = runtimeFor(fixture.players.at(1), def).stateRevision;
    const uint32_t staleTick = static_cast<uint32_t>(fixture.players.at(1).nextProjectileFireTick + 120);
    fixture.clearCaptures();
    sendGenericAttack(fixture, fixture.players.at(1), oldLifeRequest, staleTick);
    auto staleResult = onlyAttackResult(fixture.capture(shooter.id));
    check(staleResult.has_value(), def.id + " old-life request after respawn sends AttackResultPacket");
    if (staleResult)
    {
        checkEq(staleResult->accepted, static_cast<uint8_t>(0), def.id + " old-life request rejected");
        checkEq(staleResult->reason, static_cast<uint8_t>(8), def.id + " old-life request rejected as stale generation");
    }
    checkEq(fixture.projectiles.size(), projectileCountAfterOldLife, def.id + " old-life duplicate cannot spawn into new life");
    checkEq(runtimeAmmo(fixture.players.at(1), def), newLifeAmmoBefore, def.id + " old-life duplicate cannot consume new-life ammo");
    checkEq(fixture.players.at(1).health, newLifeHealthBefore, def.id + " old-life duplicate cannot change new-life health");
    check(glm::length(fixture.players.at(1).vel - newLifeVelocityBefore) < 0.0001f, def.id + " old-life duplicate cannot apply new-life knockback");
    checkEq(fixture.players.at(1).nextProjectileFireTick, newLifeCooldownBefore, def.id + " old-life duplicate cannot change new-life cooldown");
    checkEq(runtimeFor(fixture.players.at(1), def).stateRevision, newLifeStateRevisionBefore, def.id + " old-life duplicate cannot change new-life state revision");

    fixture.clearCaptures();
    AttackRequestPacket newLifeRequest = makeAttackRequest(
        fixture.players.at(1), def, baseRequestId + 1, staleTick + 1,
        fixture.players.at(1).pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, fixture.players.at(1), newLifeRequest, staleTick + 1);

    auto newLifeResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(newLifeResult.has_value(), def.id + " new-life fire sends projectile result");
    checkEq(fixture.projectiles.size(), projectileCountAfterOldLife + 1, def.id + " new-life fire creates projectile");
    checkEq(runtimeAmmo(fixture.players.at(1), def), newLifeAmmoBefore - 1, def.id + " new-life fire consumes one round");
    if (newLifeResult)
    {
        checkEq(newLifeResult->accepted, static_cast<uint8_t>(1), def.id + " new-life fire accepted");
        checkEq(newLifeResult->weapon, weaponType, def.id + " new-life result weapon");
    }
}

void testRocketPredictionIdentityHelpers()
{
    RocketLauncherState state;
    RocketLauncherState::Rocket first;
    first.position = glm::vec3(1.0f, 0.0f, 0.0f);
    state.activeRockets.push_back(first);

    WeaponRocketLauncher::tagLatestLocalRocket(state, 77);
    checkEq(state.activeRockets.size(), static_cast<size_t>(1), "tagging keeps rocket count");
    checkEq(state.activeRockets[0].fireSerial, 77u, "latest local rocket gets fire serial");

    check(WeaponRocketLauncher::attachAuthoritativeRocket(state, 77, 1234), "authoritative rocket attaches by fire serial");
    checkEq(state.activeRockets[0].authoritativeProjectileId, 1234u, "authoritative id stored on predicted rocket");
    check(WeaponRocketLauncher::attachAuthoritativeRocket(state, 77, 1234), "duplicate authoritative attach remains idempotent");
    check(!WeaponRocketLauncher::attachAuthoritativeRocket(state, 88, 2222), "unmatched fire serial does not attach");

    RocketLauncherState::Rocket second;
    second.fireSerial = 88;
    state.activeRockets.push_back(second);

    check(!WeaponRocketLauncher::removeLocalRocketByFireSerial(state, 999), "unmatched rejection does not remove rocket");
    checkEq(state.activeRockets.size(), static_cast<size_t>(2), "unmatched rejection preserves rocket count");
    check(WeaponRocketLauncher::removeLocalRocketByFireSerial(state, 88), "matched rejection removes predicted rocket");
    checkEq(state.activeRockets.size(), static_cast<size_t>(1), "matched rejection removes exactly one rocket");
    checkEq(state.activeRockets[0].fireSerial, 77u, "matched rejection leaves unrelated rocket");
    check(WeaponRocketLauncher::removeAuthoritativeRocket(state, 1234), "authoritative despawn removes adopted rocket");
    check(state.activeRockets.empty(), "authoritative despawn leaves no rockets");
}

void testClientProjectileFireResultPendingState()
{
    MultiplayerContext ctx;
    ctx.localPlayerId = 9;

    MultiplayerContext::PendingFireRequest pendingFire;
    pendingFire.fireSerial = 501;
    pendingFire.weapon = NETWORK_WEAPON_ROCKET_LAUNCHER;
    pendingFire.origin = glm::vec3(1.0f, 0.0f, 0.0f);
    pendingFire.direction = glm::vec3(1.0f, 0.0f, 0.0f);
    ctx.pendingFireRequests[pendingFire.fireSerial] = pendingFire;

    MultiplayerContext::PendingAttackRequest pendingAttack;
    pendingAttack.requestId = pendingFire.fireSerial;
    pendingAttack.spawnGeneration = 12;
    pendingAttack.weaponDefNetworkId = 44;
    ctx.pendingAttackRequests[pendingAttack.requestId] = pendingAttack;

    ProjectileFireResultPacket rejected{};
    rejected.fireSerial = pendingFire.fireSerial;
    rejected.weapon = NETWORK_WEAPON_ROCKET_LAUNCHER;
    rejected.accepted = 0;
    rejected.reason = PROJECTILE_FIRE_COOLDOWN;
    rejected.cooldownRemaining = 0.25f;

    ProjectileFireResultApplyOutcome rejectedOutcome =
        mpApplyProjectileFireResultToPending(ctx, rejected);
    check(!rejectedOutcome.accepted, "rejected projectile result remains rejected");
    check(rejectedOutcome.clearedGenericPending, "rejected result clears generic pending attack");
    check(rejectedOutcome.clearedProjectilePending, "rejected result clears projectile pending request");
    check(rejectedOutcome.queuedRejection, "rejected result queues one fire rejection");
    checkEq(ctx.pendingAttackRequests.count(501), static_cast<size_t>(0), "generic pending attack erased on rejection");
    checkEq(ctx.pendingFireRequests.count(501), static_cast<size_t>(0), "projectile pending fire erased on rejection");
    checkEq(ctx.fireRejections.size(), static_cast<size_t>(1), "one rejection refund entry queued");
    checkEq(ctx.fireRejections[0].fireSerial, 501u, "queued rejection uses matching serial");
    checkEq(ctx.fireRejections[0].weapon, static_cast<uint8_t>(NETWORK_WEAPON_ROCKET_LAUNCHER), "queued rejection keeps weapon type");
    checkEq(ctx.fireRejections[0].reason, static_cast<uint8_t>(PROJECTILE_FIRE_COOLDOWN), "queued rejection keeps reason");
    check(ctx.processedRefundSerials.count(501) == 1, "processed refund serial recorded");

    ProjectileFireResultApplyOutcome duplicateRejectedOutcome =
        mpApplyProjectileFireResultToPending(ctx, rejected);
    check(!duplicateRejectedOutcome.queuedRejection, "duplicate rejection does not queue a second refund");
    check(duplicateRejectedOutcome.duplicateOrStaleRejection, "duplicate rejection is classified as stale or duplicate");
    checkEq(ctx.fireRejections.size(), static_cast<size_t>(1), "duplicate rejection leaves one refund entry");

    MultiplayerContext::PendingFireRequest unrelated;
    unrelated.fireSerial = 777;
    unrelated.weapon = NETWORK_WEAPON_GRENADE_LAUNCHER;
    ctx.pendingFireRequests[unrelated.fireSerial] = unrelated;
    ProjectileFireResultPacket unrelatedReject = rejected;
    unrelatedReject.fireSerial = 778;
    ProjectileFireResultApplyOutcome unrelatedOutcome =
        mpApplyProjectileFireResultToPending(ctx, unrelatedReject);
    check(!unrelatedOutcome.clearedProjectilePending, "unrelated rejection does not clear pending projectile");
    checkEq(ctx.pendingFireRequests.count(777), static_cast<size_t>(1), "unrelated pending projectile survives rejection");

    ctx.pendingFireRequests.clear();
    ctx.pendingAttackRequests.clear();
    ctx.fireRejections.clear();
    ctx.processedRefundSerials.clear();

    MultiplayerContext::PendingFireRequest acceptedFire;
    acceptedFire.fireSerial = 888;
    acceptedFire.weapon = NETWORK_WEAPON_GRENADE_LAUNCHER;
    ctx.pendingFireRequests[acceptedFire.fireSerial] = acceptedFire;
    MultiplayerContext::PendingAttackRequest acceptedAttack;
    acceptedAttack.requestId = acceptedFire.fireSerial;
    acceptedAttack.spawnGeneration = 13;
    ctx.pendingAttackRequests[acceptedAttack.requestId] = acceptedAttack;

    ProjectileFireResultPacket accepted{};
    accepted.fireSerial = acceptedFire.fireSerial;
    accepted.projectileId = 2222;
    accepted.weapon = NETWORK_WEAPON_GRENADE_LAUNCHER;
    accepted.accepted = 1;
    accepted.reason = PROJECTILE_FIRE_ACCEPTED;

    ProjectileFireResultApplyOutcome acceptedOutcome =
        mpApplyProjectileFireResultToPending(ctx, accepted);
    check(acceptedOutcome.accepted, "accepted projectile result remains accepted");
    check(acceptedOutcome.clearedGenericPending, "accepted result clears generic pending attack");
    check(acceptedOutcome.clearedProjectilePending, "accepted result clears projectile pending request");
    checkEq(ctx.pendingAttackRequests.count(888), static_cast<size_t>(0), "generic pending attack erased on accepted result");
    checkEq(ctx.pendingFireRequests.count(888), static_cast<size_t>(0), "projectile pending fire erased on accepted result");
    check(ctx.fireRejections.empty(), "accepted result never queues rejection refund");
    check(ctx.processedRefundSerials.empty(), "accepted result never marks refund serial");
}

void testRocketExplosionDamageDeathAndRespawnFire(const WeaponDefinition& rocket)
{
    Fixture fixture;
    fixture.addPlayer(1, 4701);
    fixture.addPlayer(2, 4702);
    fixture.addPlayer(3, 4703);

    auto& shooter = fixture.players.at(1);
    auto& armored = fixture.players.at(2);
    auto& fragile = fixture.players.at(3);

    shooter.pos = glm::vec3(0.0f);
    armored.pos = glm::vec3(5.0f, 0.0f, 0.0f);
    fragile.pos = glm::vec3(5.0f, 0.8f, 0.0f);
    armored.health = 300;
    fragile.health = 50;
    armored.spawnState = ServerPlayer::Active;
    fragile.spawnState = ServerPlayer::Active;
    armored.spawnGeneration = 41;
    fragile.spawnGeneration = 42;

    equipProjectileWeapon(shooter, rocket, 40);

    fixture.clearCaptures();
    AttackRequestPacket request = makeAttackRequest(
        shooter, rocket, 9000, 1000, shooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, shooter, request, 1000);
    auto fireResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(fireResult.has_value() && fireResult->accepted == 1, "rocket explosion setup fire accepted");
    checkEq(fixture.projectiles.size(), static_cast<size_t>(1), "rocket explosion setup creates one projectile");

    for (uint32_t i = 0; i < 120 && !fixture.projectiles.empty(); ++i)
    {
        tickServerProjectiles(
            INVALID_SOCKET,
            fixture.players,
            fixture.npcs,
            fixture.projectiles,
            fixture.world,
            1.0f / 60.0f,
            1001 + i,
            fixture.totalPacketsOut);
    }

    check(fixture.projectiles.empty(), "rocket impact removes exploded projectile");
    check(fixture.players.at(2).health < 300, "rocket explosion damages nonlethal target");
    check(glm::length(fixture.players.at(2).vel) > 0.0f, "rocket explosion applies knockback to nonlethal target");
    check(fixture.players.at(3).dead, "rocket explosion can kill low-health target");
    checkEq(fixture.players.at(3).deaths, 1, "rocket explosion increments victim death count");
    checkEq(fixture.players.at(1).kills, 1, "rocket explosion increments attacker kill count");

    auto explosionEvents = fixture.capture(shooter.id).packets<ProjectileExplodeEventPacket>(PACKET_PROJECTILE_EXPLODE_EVENT);
    auto damageEvents = fixture.capture(shooter.id).packets<DamageConfirmedEventPacket>(PACKET_DAMAGE_CONFIRMED_EVENT);
    check(!explosionEvents.empty(), "rocket explosion queues reliable explosion event");
    check(!damageEvents.empty(), "rocket explosion queues damage confirmed events");
    if (!explosionEvents.empty())
    {
        checkEq(explosionEvents.back().weapon, static_cast<uint8_t>(NETWORK_WEAPON_ROCKET_LAUNCHER), "explosion event weapon type");
        check(explosionEvents.back().victimCount >= 2, "explosion event records damaged victims");
    }

    const int armoredHealthAfterExplosion = fixture.players.at(2).health;
    const int fragileDeathsAfterExplosion = fixture.players.at(3).deaths;
    for (uint32_t i = 0; i < 5; ++i)
    {
        tickServerProjectiles(
            INVALID_SOCKET,
            fixture.players,
            fixture.npcs,
            fixture.projectiles,
            fixture.world,
            1.0f / 60.0f,
            1200 + i,
            fixture.totalPacketsOut);
    }
    checkEq(fixture.players.at(2).health, armoredHealthAfterExplosion, "post-explosion ticks do not apply duplicate damage");
    checkEq(fixture.players.at(3).deaths, fragileDeathsAfterExplosion, "post-explosion ticks do not duplicate death");

    fixture.players.at(3).ownedWeaponIds.clear();
    fixture.players.at(3).ownedWeaponIds.push_back(rocket.id);
    resetPlayerForSpawn(fixture.players.at(3), false);
    fixture.players.at(3).spawnState = ServerPlayer::Active;
    fixture.players.at(3).equippedSlot = rocket.slot;
    fixture.players.at(3).pos = glm::vec3(0.0f, 4.0f, 0.0f);

    const int respawnAmmoBefore = runtimeAmmo(fixture.players.at(3), rocket);
    fixture.clearCaptures();
    AttackRequestPacket respawnFire = makeAttackRequest(
        fixture.players.at(3), rocket, 9001, 1300,
        fixture.players.at(3).pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, fixture.players.at(3), respawnFire, 1300);
    auto respawnResult = onlyProjectileResult(fixture.capture(3));
    check(respawnResult.has_value(), "respawned victim can send rocket fire result");
    if (respawnResult)
    {
        checkEq(respawnResult->accepted, static_cast<uint8_t>(1), "respawned victim rocket fire accepted");
        checkEq(respawnResult->weapon, static_cast<uint8_t>(NETWORK_WEAPON_ROCKET_LAUNCHER), "respawned victim result weapon");
    }
    checkEq(runtimeAmmo(fixture.players.at(3), rocket), respawnAmmoBefore - 1, "respawned victim fire consumes one round");
}

void testAttackReconcilesValidEquipRace(const WeaponDefinition& grenade)
{
    Fixture fixture;
    fixture.addPlayer(1, 4801);

    auto& shooter = fixture.players.at(1);
    shooter.pos = glm::vec3(0.0f);
    equipProjectileWeapon(shooter, grenade, 50);
    shooter.equippedSlot = 0;

    fixture.clearCaptures();
    AttackRequestPacket request = makeAttackRequest(
        shooter, grenade, 9100, 1400,
        shooter.pos, glm::vec3(1.0f, 0.0f, 0.0f));
    sendGenericAttack(fixture, shooter, request, 1400);

    auto fireResult = onlyProjectileResult(fixture.capture(shooter.id));
    check(fireResult.has_value(), "equip-race attack returns projectile result");
    if (fireResult)
        checkEq(fireResult->accepted, static_cast<uint8_t>(1), "equip-race attack accepted");
    checkEq(shooter.equippedSlot, grenade.slot, "equip-race reconciles authoritative slot");
    checkEq(fixture.projectiles.size(), static_cast<size_t>(1), "equip-race spawns projectile");
}

// ── Reliable predicted-projectile adoption ───────────────────────────
// The local owner's predicted rocket/grenade must be renamed provisional →
// authoritative the moment the (reliable, retried) AttackResult arrives, so the
// client never depends on the lossy spawn broadcast to collapse the duplicate.

static uint32_t provisionalIdFor(uint32_t requestId)
{
    return 0x80000000u | (requestId & 0x7fffffffu);
}

void testAttackResultAdoptsPredictedProjectile(const ProjectileWeaponRefs& refs)
{
    MultiplayerContext ctx;
    ctx.localPlayerId = 9;

    const uint32_t provisionalId = provisionalIdFor(501);
    NetworkProjectile pred;
    pred.projectileId = provisionalId;
    pred.ownerPlayerId = ctx.localPlayerId;
    pred.fireSerial = 501;
    pred.weaponType = NETWORK_WEAPON_ROCKET_LAUNCHER;
    pred.predicted = true;
    pred.position = glm::vec3(10.0f, 0.0f, 1.0f);
    pred.velocity = glm::vec3(20.0f, 0.0f, 0.0f);
    ctx.networkProjectiles[provisionalId] = pred;
    ctx.predictedProjectileIds.insert(provisionalId);

    MultiplayerContext::PendingAttackRequest pending;
    pending.requestId = 501;
    pending.weaponDefNetworkId = refs.rocketNetId;
    ctx.pendingAttackRequests[501] = pending;

    AttackResultPacket result{};
    result.header.type = PACKET_ATTACK_RESULT;
    result.header.playerId = ctx.localPlayerId;
    result.requestId = 501;
    result.accepted = 1;
    result.projectileId = 2222;
    result.weaponDefNetworkId = refs.rocketNetId;
    result.magazineAmmo = 4;
    result.reserveAmmo = 20;
    mpProcessAttackResultPacket(ctx, &result);

    checkEq(ctx.networkProjectiles.count(2222), static_cast<size_t>(1), "attack result adopts predicted under authoritative id");
    checkEq(ctx.networkProjectiles.count(provisionalId), static_cast<size_t>(0), "attack result removes provisional projectile");
    checkEq(ctx.networkProjectiles.size(), static_cast<size_t>(1), "attack result leaves exactly one projectile");
    check(ctx.networkProjectiles[2222].predicted, "adopted projectile stays predicted");
    checkEq(ctx.networkProjectiles[2222].fireSerial, 501u, "adopted projectile keeps fireSerial");
    checkEq(ctx.networkProjectiles[2222].position.x, 10.0f, "adopted projectile keeps predicted pose");
    checkEq(ctx.predictedProjectileIds.count(2222), static_cast<size_t>(1), "adopted id tracked as predicted");
}

// Lost-spawn state recovery must adopt the predicted projectile instead of
// creating a second one (the badconn double-rocket bug).
void testStateRecoveryAdoptsPredictedProjectile()
{
    MultiplayerContext ctx;
    ctx.localPlayerId = 9;

    const uint32_t provisionalId = provisionalIdFor(601);
    NetworkProjectile pred;
    pred.projectileId = provisionalId;
    pred.ownerPlayerId = ctx.localPlayerId;
    pred.fireSerial = 601;
    pred.weaponType = NETWORK_WEAPON_GRENADE_LAUNCHER;
    pred.predicted = true;
    pred.position = glm::vec3(3.0f, 0.0f, 2.0f);
    ctx.networkProjectiles[provisionalId] = pred;
    ctx.predictedProjectileIds.insert(provisionalId);

    ProjectileStateEventPacket state{};
    state.header.type = PACKET_PROJECTILE_STATE_EVENT;
    state.header.tick = 500;
    state.projectileId = 3333;
    state.ownerPlayerId = ctx.localPlayerId;
    state.fireSerial = 601;
    state.weapon = NETWORK_WEAPON_GRENADE_LAUNCHER;
    state.posX = 3.1f; state.posY = 0.0f; state.posZ = 2.1f;
    state.velX = 0.0f; state.velY = 0.0f; state.velZ = 0.0f;
    state.rotW = 1.0f;
    state.age = 0.25f;
    mpProcessProjectileStateEventPacket(ctx, &state);

    checkEq(ctx.networkProjectiles.count(3333), static_cast<size_t>(1), "state recovery adopts predicted under authoritative id");
    checkEq(ctx.networkProjectiles.count(provisionalId), static_cast<size_t>(0), "state recovery removes provisional id");
    checkEq(ctx.networkProjectiles.size(), static_cast<size_t>(1), "state recovery leaves exactly one projectile");
    check(ctx.networkProjectiles[3333].predicted, "recovery-adopted projectile stays predicted");
    checkEq(ctx.networkProjectiles[3333].fireSerial, 601u, "recovery-adopted keeps fireSerial");
    checkEq(ctx.networkProjectiles[3333].latestAcceptedTick, 500u, "recovery-adopted accepts server tick");
    check(ctx.networkProjectiles[3333].hasTargetState, "recovery-adopted has target state");
}

// Ordering: state event adopts first, then the AttackResult arrives — no
// duplicate may be created by the late reliable result.
void testLateAttackResultDoesNotDuplicate(const ProjectileWeaponRefs& refs)
{
    MultiplayerContext ctx;
    ctx.localPlayerId = 9;

    const uint32_t provisionalId = provisionalIdFor(701);
    NetworkProjectile pred;
    pred.projectileId = provisionalId;
    pred.ownerPlayerId = ctx.localPlayerId;
    pred.fireSerial = 701;
    pred.weaponType = NETWORK_WEAPON_ROCKET_LAUNCHER;
    pred.predicted = true;
    pred.position = glm::vec3(5.0f, 0.0f, 1.0f);
    ctx.networkProjectiles[provisionalId] = pred;
    ctx.predictedProjectileIds.insert(provisionalId);

    ProjectileStateEventPacket state{};
    state.header.type = PACKET_PROJECTILE_STATE_EVENT;
    state.header.tick = 600;
    state.projectileId = 4444;
    state.ownerPlayerId = ctx.localPlayerId;
    state.fireSerial = 701;
    state.weapon = NETWORK_WEAPON_ROCKET_LAUNCHER;
    state.posX = 5.1f; state.posY = 0.0f; state.posZ = 1.1f;
    state.velX = 20.0f; state.velY = 0.0f; state.velZ = 0.0f;
    state.rotW = 1.0f;
    mpProcessProjectileStateEventPacket(ctx, &state);
    checkEq(ctx.networkProjectiles.size(), static_cast<size_t>(1), "state-first: one projectile after adoption");

    MultiplayerContext::PendingAttackRequest pending;
    pending.requestId = 701;
    pending.weaponDefNetworkId = refs.rocketNetId;
    ctx.pendingAttackRequests[701] = pending;

    AttackResultPacket result{};
    result.header.type = PACKET_ATTACK_RESULT;
    result.requestId = 701;
    result.accepted = 1;
    result.projectileId = 4444;
    result.weaponDefNetworkId = refs.rocketNetId;
    result.magazineAmmo = 4;
    result.reserveAmmo = 20;
    mpProcessAttackResultPacket(ctx, &result);

    checkEq(ctx.networkProjectiles.size(), static_cast<size_t>(1), "late attack result does not duplicate");
    checkEq(ctx.networkProjectiles.count(4444), static_cast<size_t>(1), "authoritative id survives late attack result");
    check(ctx.networkProjectiles[4444].predicted, "still predicted after late attack result");
}

// ── Splash line-of-sight helpers (shared kernel) ─────────────────────
// Walls/cover block splash; floors/ceilings never do; nearest-capsule-point
// targets the victim's nearest body part.
void testSplashLineOfSight()
{
    CollisionTriangle floor;
    floor.a = {-10.0f, -10.0f, 0.0f};
    floor.b = { 10.0f, -10.0f, 0.0f};
    floor.c = {-10.0f,  10.0f, 0.0f};
    floor.normal = {0.0f, 0.0f, 1.0f};

    CollisionTriangle wall;
    wall.a = {0.0f, -10.0f, 0.0f};
    wall.b = {0.0f,  10.0f, 0.0f};
    wall.c = {0.0f, -10.0f, 10.0f};
    wall.normal = {1.0f, 0.0f, 0.0f};

    std::vector<CollisionTriangle> tris = {floor, wall};
    std::vector<int> candidates = {0, 1};

    // Open line of sight: ray parallel to the wall, off to the side.
    check(!splashRayBlockedByWall({1.0f, -5.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, 10.0f,
                                  candidates, tris),
          "open splash ray is not blocked");
    // Wall between blast and target: blocked.
    check(splashRayBlockedByWall({-5.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 10.0f,
                                 candidates, tris),
          "wall blocks splash");
    // Floor-only path (ray stays above the floor): not blocked.
    check(!splashRayBlockedByWall({0.0f, 0.0f, 2.0f}, {1.0f, 0.0f, 0.0f}, 1.0f,
                                  candidates, tris),
          "floor never blocks splash");
    // Nearest body part: pick the box closest to the blast and target its
    // nearest point (head/arms/legs/torso, never a capsule).
    SplashBodyPartBox head;
    head.center = {0.0f, 0.0f, 2.8f}; head.half = {0.3f, 0.3f, 0.3f};
    SplashBodyPartBox torso;
    torso.center = {0.0f, 0.0f, 1.6f}; torso.half = {0.4f, 0.4f, 0.7f};
    SplashBodyPartBox legL;
    legL.center = {-0.3f, 0.0f, 0.5f}; legL.half = {0.25f, 0.25f, 0.6f};
    SplashBodyPartBox boxes[3] = {head, torso, legL};

    glm::vec3 pt;
    check(splashNearestBodyPartPoint({0.0f, 0.0f, 0.0f}, boxes, 3, pt),
          "nearest body part found");
    check(glm::length(pt - glm::vec3(-0.05f, 0.0f, 0.0f)) < 0.01f,
          "nearest body part targets the leg (lowest box)");
    check(splashNearestBodyPartPoint({0.0f, 0.0f, 5.0f}, boxes, 3, pt),
          "nearest body part from above found");
    check(glm::length(pt - glm::vec3(0.0f, 0.0f, 3.1f)) < 0.01f,
          "nearest body part from above targets the head");
    check(!splashNearestBodyPartPoint({0.0f, 0.0f, 0.0f}, boxes, 0, pt),
          "no body parts returns false");
}

} // namespace

int main()
{
    ProjectileWeaponRefs refs = ensureProjectileWeapons();
    testWeaponRouting();
    if (refs.rocket)
    {
        testGenericAcceptAndRejects(*refs.rocket, NETWORK_WEAPON_ROCKET_LAUNCHER, 1000);
        testDuplicateAndDifferentRequestIds(*refs.rocket, NETWORK_WEAPON_ROCKET_LAUNCHER, 2000);
        testOldLifeRejectionAndRespawnFire(*refs.rocket, NETWORK_WEAPON_ROCKET_LAUNCHER, 3000);
        testRocketExplosionDamageDeathAndRespawnFire(*refs.rocket);
    }
    if (refs.grenade)
    {
        testGenericAcceptAndRejects(*refs.grenade, NETWORK_WEAPON_GRENADE_LAUNCHER, 4000);
        testDuplicateAndDifferentRequestIds(*refs.grenade, NETWORK_WEAPON_GRENADE_LAUNCHER, 5000);
        testOldLifeRejectionAndRespawnFire(*refs.grenade, NETWORK_WEAPON_GRENADE_LAUNCHER, 6000);
        testAttackReconcilesValidEquipRace(*refs.grenade);
    }
    testRocketPredictionIdentityHelpers();
    testClientProjectileFireResultPendingState();
    testAttackResultAdoptsPredictedProjectile(refs);
    testStateRecoveryAdoptsPredictedProjectile();
    testLateAttackResultDoesNotDuplicate(refs);
    testSplashLineOfSight();

    if (gFailures != 0)
    {
        std::fprintf(stderr, "projectile-network-baseline-test: %d failure(s)\n", gFailures);
        return 1;
    }

    std::printf("projectile-network-baseline-test: PASS\n");
    return 0;
}
