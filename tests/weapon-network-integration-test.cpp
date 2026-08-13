// 07 21 2026, 21 45
/* purpose
* Tests Stage 4A source integration across generic attack, server ticks, and legacy packet gates.
* Verifies physical-contact ticking is wired into server loops and old in-scope packets are inert.
* Protects UDP/ICE parity by requiring generic packet dispatch to remain transport-neutral.
* Does NOT open sockets, run process harnesses, render frames, or modify runtime files.
* Does NOT test projectile migration, coordinator behavior, or NAT candidate negotiation.
* Does NOT replace the required two-client process networking validation.
*/

#include <cstdio>
#include <fstream>
#include <string>

static int gFailures = 0;

static std::string readFile(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

int main()
{
    const std::string server = readFile("src/network/server.cpp");
    const std::string packets = readFile("src/network/server-packets.cpp");
    const std::string attack = readFile("src/network/server-attack.cpp");
    const std::string handlers = readFile("src/network/server-packet-handlers.cpp");
    const std::string clientTick = readFile("src/network/multiplayer-tick.cpp");
    const std::string engineCombat = readFile("src/engine/engine-tick-combat.cpp");
    const std::string runtimeHeader = readFile("src/combat/weapon-runtime.h");
    const std::string runtime = readFile("src/combat/weapon-runtime.cpp");
    const std::string serverPlayers = readFile("src/network/server-players.cpp");
    const std::string weaponSystem = readFile("src/combat/weapon-system.cpp");
    const std::string terminal = readFile("src/terminal/weapon-commands.cpp");
    const std::string packetsSchema = readFile("src/network/packets.h");
    const std::string clientProjectiles = readFile("src/network/multiplayer-projectiles.cpp");
    const std::string engineNet = readFile("src/engine/engine-tick-net.cpp");
    const std::string reconcile = readFile("src/network/weapon-runtime-reconciliation.cpp");

    check(server.find("tickServerPhysicalContactWeapons") != std::string::npos,
          "server loops tick generic physical-contact weapons");
    check(server.find("tickServerSwordCombat(") == std::string::npos,
          "server loops no longer tick old sword-specific combat path");
    check(packets.find("handleAttackRequest(sock, from, buffer, bytes") != std::string::npos,
          "server dispatch keeps generic AttackRequest transport-neutral");
    check(attack.find("WeaponExecutionType::PhysicalContact") != std::string::npos,
          "generic attack dispatch includes physical-contact family");
    check(handlers.find("(void)pkt;\n    return;") != std::string::npos,
          "legacy Godball state handler returns inert");
    check(clientTick.find("PACKET_GODBALL_STATE") != std::string::npos &&
          clientTick.find("gbPkt.header.type = PACKET_GODBALL_STATE") == std::string::npos,
          "client no longer transmits legacy Godball state packets");
    check(engineCombat.find("collectRemoteGodballHits") == std::string::npos &&
          engineCombat.find("NETWORK_WEAPON_GODBALL") == std::string::npos,
          "engine combat no longer converts local Godball overlap to shot packets");
    check(runtimeHeader.find("initialReserveAmmoForDefinition") != std::string::npos &&
          runtime.find("return (it != def.customParams.end()) ? (int)it->second : 1337") != std::string::npos,
          "initial reserve ammo has one canonical fallback of 1337");
    check(runtime.find("reserveAmmo = initialReserveAmmoForDefinition(def)") != std::string::npos &&
          runtime.find("rt.reserveAmmo = initialReserveAmmoForDefinition(def)") != std::string::npos,
          "client runtime reset and init use canonical reserve helper");
    check(serverPlayers.find("rt.reserveAmmo = initialReserveAmmoForDefinition(*def)") != std::string::npos &&
          serverPlayers.find("reserveAmmo = (it != def->customParams.end()) ? (int)it->second : 0") == std::string::npos,
          "server spawn reset uses canonical reserve helper, not fallback 0");
    check(weaponSystem.find("result.autoReloadTriggered = true") != std::string::npos &&
          terminal.find("shot.autoReloadTriggered") != std::string::npos &&
          terminal.find("sendReloadRequestForWeapon") != std::string::npos &&
          reconcile.find("PACKET_RELOAD_REQUEST") != std::string::npos &&
          reconcile.find("req.magazineAmmo") != std::string::npos,
          "auto-reload (dry fire, R, unequip) sends reload request with client ammo");
    check(packetsSchema.find("uint16_t weaponDefNetworkId = 0;") != std::string::npos &&
          attack.find("result.weaponDefNetworkId = req->weaponDefNetworkId") != std::string::npos,
          "AttackResult carries weapon definition id from request");
    check(clientProjectiles.find("ctx.pendingAttackResults.push_back(*event)") != std::string::npos &&
          engineNet.find("reconcileAuthoritativeWeaponRuntime(") != std::string::npos &&
          engineNet.find("attack-result-accepted") != std::string::npos &&
          engineNet.find("attack-result-rejected") != std::string::npos,
          "client applies authoritative weapon state from AttackResult");

    if (gFailures)
    {
        std::printf("[weapon-network-integration-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[weapon-network-integration-test] PASS\n");
    return 0;
}
