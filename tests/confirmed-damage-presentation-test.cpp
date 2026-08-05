// 07 19 2026, 13 10
/* purpose
* Validates generic confirmed-damage client presentation decisions.
* Covers attacker-only feedback, duplicate suppression, stale lifecycle rejection, and self-damage policy.
* Proves multiple weapon categories use presentConfirmedDamage without touching health.
* Does NOT start networking, audio, rendering, or the game executable.
* Does NOT validate server damage math or reliable retransmission internals.
* Does NOT require real sound/effect assets.
*/

#include "network/confirmed-damage-presentation.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "combat/death-system.h"
#include "config/weapon-hitfx-config.h"
#include "terminal/terminal-state.h"
#include "ui/hitmarker.h"
#include "audio/hitmarker-audio.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace {

struct Counts
{
    int hitmarkers = 0;
    int sounds = 0;
    int damageNumbers = 0;
    int totalDamage = 0;
};

WeaponHitFxPresentationConfig gPresentation;

void countHitmarker(int, void* user)
{
    ((Counts*)user)->hitmarkers += 1;
}

void countSound(int, void* user)
{
    ((Counts*)user)->sounds += 1;
}

void countDamageNumber(const HitEvent& event, void* user)
{
    Counts* counts = (Counts*)user;
    counts->damageNumbers += 1;
    counts->totalDamage += event.damage;
}

MimitaNet::ConfirmedDamagePresentationSink sinkFor(Counts& counts)
{
    MimitaNet::ConfirmedDamagePresentationSink sink;
    sink.user = &counts;
    sink.showHitmarker = countHitmarker;
    sink.playHitSound = countSound;
    sink.showDamageNumber = countDamageNumber;
    return sink;
}

MimitaNet::MultiplayerContext makeContext(uint32_t localPlayerId = 1)
{
    MimitaNet::MultiplayerContext ctx;
    ctx.localPlayerId = localPlayerId;
    ctx.reliableEventSessionId = 7;
    ctx.lastKnownSpawnGeneration = 10;
    ctx.localServerHealth = 77;
    ctx.playerRegistry[1] = {"attacker", 1, 0};
    ctx.playerRegistry[2] = {"victim", 2, 0};
    ctx.playerRegistry[3] = {"victim2", 3, 0};
    return ctx;
}

MimitaNet::DamageConfirmedEventPacket eventBase(uint32_t eventId = 1)
{
    MimitaNet::DamageConfirmedEventPacket e{};
    e.eventId = eventId;
    e.eventSessionId = 7;
    e.attackerPlayerId = 1;
    e.targetPlayerId = 2;
    e.causeSerial = 55;
    e.attackerSpawnGeneration = 10;
    e.targetSpawnGeneration = 20;
    e.damage = 25;
    e.weapon = MimitaNet::NETWORK_WEAPON_REVOLVER;
    e.source = MimitaNet::DAMAGE_CONFIRMED_HITSCAN;
    e.hitX = 1.0f;
    e.hitY = 2.0f;
    e.hitZ = 3.0f;
    e.normalZ = 1.0f;
    return e;
}

bool expect(bool value, const char* name)
{
    std::printf("%-70s %s\n", name, value ? "PASS" : "FAIL");
    return value;
}

} // namespace

void hitmarkerVisualOnly(int) {}
void hitmarker(int) {}
void playHitmarkerSound(int) {}

namespace HitEffects {
void spawnHitEffects(glm::vec3, const glm::vec3&, const glm::vec3&, int,
                     const std::string&, const std::string&, bool) {}
}

Player* gpPlayer = nullptr;

DeathSystem& DeathSystem::instance()
{
    static DeathSystem instance;
    return instance;
}

void DeathSystem::healKillerToFull(Player& player, const std::string& killerName)
{
    (void)player;
    (void)killerName;
}

namespace MimitaNet {
const char* networkWeaponTypeName(uint8_t) { return "configured_weapon"; }
}

WeaponHitFxConfig& WeaponHitFxConfig::instance()
{
    static WeaponHitFxConfig config;
    return config;
}

const WeaponHitFxPresentationConfig& WeaponHitFxConfig::presentationFor(const std::string&) const
{
    return gPresentation;
}

int main()
{
    int failed = 0;
    gPresentation = WeaponHitFxPresentationConfig{};

    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(1);
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "hitscan confirmed damage shows generic presentation");
        failed += !expect(counts.hitmarkers == 1 && counts.sounds == 1 &&
            counts.damageNumbers == 1 && counts.totalDamage == 25,
            "hitscan produced hitmarker, damage number, and hit sound");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(2);
        e.source = MimitaNet::DAMAGE_CONFIRMED_ROCKET_EXPLOSION;
        e.weapon = MimitaNet::NETWORK_WEAPON_ROCKET_LAUNCHER;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink) && counts.damageNumbers == 1,
            "projectile explosion uses same presentation function");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(3);
        e.source = MimitaNet::DAMAGE_CONFIRMED_MELEE;
        e.weapon = MimitaNet::NETWORK_WEAPON_SWORDSWORD;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink) && counts.damageNumbers == 1,
            "melee uses same presentation function");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(4);
        e.weapon = MimitaNet::NETWORK_WEAPON_SHOTGUN;
        e.damage = 141;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink) &&
            counts.damageNumbers == 1 && counts.totalDamage == 141,
            "shotgun aggregate event preserves summed confirmed damage");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto a = eventBase(5);
        auto b = eventBase(6);
        b.targetPlayerId = 3;
        b.targetSpawnGeneration = 30;
        b.damage = 40;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, a, &sink) &&
            MimitaNet::presentConfirmedDamage(ctx, b, &sink) &&
            counts.damageNumbers == 2 && counts.totalDamage == 65,
            "multiple explosion victims remain separate damage numbers");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(7);
        bool first = MimitaNet::presentConfirmedDamage(ctx, e, &sink);
        bool second = MimitaNet::presentConfirmedDamage(ctx, e, &sink);
        failed += !expect(first && !second && counts.damageNumbers == 1,
            "duplicate event shows presentation once");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(8);
        e.eventSessionId = 99;
        failed += !expect(!MimitaNet::presentConfirmedDamage(ctx, e, &sink) && counts.damageNumbers == 0,
            "stale session event shows nothing");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(9);
        e.attackerSpawnGeneration = 9;
        failed += !expect(!MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "stale attacker spawn generation shows nothing");
        e = eventBase(10);
        e.targetPlayerId = 1;
        e.targetSpawnGeneration = 9;
        gPresentation.selfDamageFeedback = true;
        failed += !expect(!MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "stale victim spawn generation shows nothing");
        gPresentation.selfDamageFeedback = false;
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(11);
        e.targetPlayerId = 1;
        e.targetSpawnGeneration = 10;
        failed += !expect(!MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "self-damage default policy shows nothing");
        gPresentation.selfDamageFeedback = true;
        e.eventId = 12;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "self-damage explicit policy can show feedback");
        gPresentation.selfDamageFeedback = false;
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(13);
        e.damage = 0;
        failed += !expect(!MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "rejected or zero damage shows nothing");
    }
    {
        auto victimCtx = makeContext(2);
        auto observerCtx = makeContext(9);
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(14);
        failed += !expect(!MimitaNet::presentConfirmedDamage(victimCtx, e, &sink) &&
            !MimitaNet::presentConfirmedDamage(observerCtx, e, &sink) &&
            counts.damageNumbers == 0,
            "victim and observer do not receive attacker UI");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(15);
        const int before = ctx.localServerHealth;
        MimitaNet::presentConfirmedDamage(ctx, e, &sink);
        failed += !expect(ctx.localServerHealth == before,
            "presentation never writes client health");
    }
    {
        auto ctx = makeContext();
        Counts counts;
        auto sink = sinkFor(counts);
        auto e = eventBase(16);
        e.weapon = MimitaNet::NETWORK_WEAPON_NONE;
        failed += !expect(MimitaNet::presentConfirmedDamage(ctx, e, &sink),
            "missing sound/effect configuration is safe");
    }
    {
        std::ifstream file("src/network/confirmed-damage-presentation.cpp");
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        const bool clean = text.find("rocket_launcher") == std::string::npos &&
            text.find("grenade_launcher") == std::string::npos &&
            text.find("shotgun") == std::string::npos &&
            text.find("revolver") == std::string::npos &&
            text.find("swordsword") == std::string::npos;
        failed += !expect(clean, "no weapon-name comparisons exist in generic presentation path");
    }

    std::printf("\n=== Confirmed Damage Presentation: %s ===\n", failed ? "FAIL" : "PASS");
    return failed ? 1 : 0;
}
