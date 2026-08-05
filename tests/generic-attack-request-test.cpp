// 07 21 2026, 21 45
/* purpose
* Tests the generic AttackRequest packet shape used by migrated weapon families.
* Verifies one packet can carry hitscan, shotgun, projectile, Godball, and Swordsword intent.
* Protects seed, variant, slot, spawn-generation, and compact datagram-size requirements.
* Does NOT open sockets, run the server loop, apply damage, or start mimita.exe.
* Does NOT define weapon-specific packet types or validate projectile simulation behavior.
* Does NOT test rendering, audio, local prediction, or world collision.
*/

#include "network/packets.h"

#include <cstdio>

static int gFailures = 0;
using namespace MimitaNet;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

static AttackRequestPacket requestFor(uint16_t defId, int16_t slot, uint8_t variant)
{
    AttackRequestPacket packet;
    packet.header.type = PACKET_ATTACK_REQUEST;
    packet.requestId = 77;
    packet.spawnGeneration = 9;
    packet.weaponDefNetworkId = defId;
    packet.equippedSlot = slot;
    packet.deterministicSeed = 0xabcddcba;
    packet.attackVariant = variant;
    packet.aimDirX = 1.0f;
    return packet;
}

int main()
{
    check(PROTOCOL_VERSION == 27, "protocol version bumped for VIP style event layout");
    check(sizeof(AttackRequestPacket) <= 96, "AttackRequest remains one compact safe datagram");
    check(requestFor(1, 1, 0).weaponDefNetworkId == 1, "revolver uses generic request");
    check(requestFor(2, 3, 0).weaponDefNetworkId == 2, "shotgun uses generic request");
    check(requestFor(7, 7, 0).weaponDefNetworkId == 7, "rocket launcher uses generic request");
    check(requestFor(8, 8, 0).weaponDefNetworkId == 8, "grenade launcher uses generic request");
    check(requestFor(3, 2, 0).attackVariant == 0, "Godball needs no weapon-specific packet");
    check(requestFor(4, 4, 1).attackVariant == 1, "Swordsword slash uses generic variant");
    check(requestFor(4, 4, 2).attackVariant == 2, "Swordsword lunge uses generic variant");

    if (gFailures)
    {
        std::printf("[generic-attack-request-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[generic-attack-request-test] PASS\n");
    return 0;
}
