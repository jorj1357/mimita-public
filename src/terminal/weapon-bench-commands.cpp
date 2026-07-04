#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>

#include "devtools/terminal.h"
#include "terminal/terminal-state.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-fire.h"
#include "config.h"
#include "debug/log-manager.h"
#include "debug/debug-log.h"

static void logPathNotice(const char* label, const std::string& path)
{
    printf("[BENCH] %s: %s\n", label, path.c_str());
    Terminal::instance().addLog(std::string("[BENCH] ") + label + ": " + path);
}

static std::string benchLogPath(const std::string& suffix)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    char dateBuf[16], timeBuf[16];
    std::strftime(dateBuf, sizeof(dateBuf), "%d-%m-%Y", &tm);
    std::strftime(timeBuf, sizeof(timeBuf), "%H-%M-%S", &tm);
    std::error_code ec;
    std::filesystem::create_directories("logs/" + std::string(dateBuf), ec);
    return "logs/" + std::string(dateBuf) + "/" + timeBuf + "-" + suffix;
}

static void runWeaponBench(const WeaponDefinition* def, int shotCount)
{
    if (!def) {
        Terminal::instance().addLog("[BENCH] Weapon not found");
        return;
    }

    Player& player = THE_PLAYER;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;

    int prevSlot = player.equippedSlot;
    weapons.equip(player, def->slot);

    double totalMs = 0.0;
    double minMs = 1e9, maxMs = 0.0;
    int fired = 0;

    printf("[BENCH] Firing %s %d times...\n", def->id.c_str(), shotCount);
    Terminal::instance().addLog(std::string("[BENCH] Firing ") + def->id + " " + std::to_string(shotCount) + " times...");

    for (int i = 0; i < shotCount; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        weapons.fire(camera, player, npcSystem, world, nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms > 0.001) {
            totalMs += ms;
            minMs = std::min(minMs, ms);
            maxMs = std::max(maxMs, ms);
            fired++;
        }
    }

    weapons.equip(player, prevSlot);

    double avgMs = fired > 0 ? totalMs / fired : 0.0;

    std::string benchFile = benchLogPath("weapon-bench.txt");
    {
        std::ofstream f(benchFile);
        f << "==================================================\n";
        f << "WEAPON BENCHMARK\n";
        f << "Weapon: " << def->id << "\n";
        f << "Shots: " << fired << "\n";
        f << "AvgMs: " << avgMs << "\n";
        f << "MinMs: " << minMs << "\n";
        f << "MaxMs: " << maxMs << "\n";
        f << "TotalMs: " << totalMs << "\n";
        f << "==================================================\n";
    }

    printf("\n==================================================\n");
    printf("WEAPON BENCH DONE.\n");
    printf("Weapon: %s\n", def->id.c_str());
    printf("Shots:  %d\n", fired);
    printf("Avg:    %.3f ms\n", avgMs);
    printf("Min:    %.3f ms\n", minMs);
    printf("Max:    %.3f ms\n", maxMs);
    printf("\n");
    printf("Run log:\n%s\n", LogManager::instance().path().c_str());
    printf("\n");
    printf("Weapon bench log:\n%s\n", benchFile.c_str());
    printf("Ctrl+Click either path above to open it.\n");
    printf("==================================================\n");

    Terminal::instance().addLog(std::string("[BENCH] ") + def->id + " avg=" +
        std::to_string(avgMs) + "ms min=" + std::to_string(minMs) +
        "ms max=" + std::to_string(maxMs) + "ms");

    // Write latest-weapon-bench-path.txt
    {
        std::error_code ec;
        std::filesystem::create_directories("logs", ec);
        FILE* f = fopen("logs/latest-weapon-bench-path.txt", "w");
        if (f) {
            std::string absPath = std::filesystem::absolute(benchFile).string();
            fprintf(f, "%s\n", absPath.c_str());
            fclose(f);
        }
    }
}

static void runWeaponBenchCompare()
{
    const auto& all = WeaponRegistry::instance().all();

    printf("[BENCH] Running weapon benchmark comparison...\n");
    Terminal::instance().addLog("[BENCH] Running weapon benchmark comparison...");

    std::vector<std::pair<std::string, double>> results;
    int SHOTS = 10;

    DebugConfig::WEAPON_PERF_SHOTS = false;

    for (const auto& kv : all) {
        if (kv.second.pelletCount <= 1) continue;
        Player& player = THE_PLAYER;
        NpcSystem& npcSystem = THE_NPC_SYSTEM;
        WeaponSystem& weapons = THE_WEAPONS;
        Camera& camera = THE_CAMERA;
        World& world = THE_WORLD;

        int prevSlot = player.equippedSlot;
        weapons.equip(player, kv.second.slot);

        double totalMs = 0.0;
        int fired = 0;
        for (int i = 0; i < SHOTS; ++i) {
            auto t0 = std::chrono::steady_clock::now();
            weapons.fire(camera, player, npcSystem, world, nullptr);
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            if (ms > 0.001) { totalMs += ms; fired++; }
        }
        weapons.equip(player, prevSlot);
        if (fired > 0)
            results.emplace_back(kv.second.id, totalMs / fired);
    }

    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::string benchFile = benchLogPath("weapon-bench-compare.txt");
    {
        std::ofstream f(benchFile);
        f << "==================================================\n";
        f << "WEAPON BENCHMARK COMPARISON\n";
        f << "Shots per weapon: " << SHOTS << "\n";
        f << "Sorted by avg ms (slowest first)\n";
        f << "==================================================\n";
        for (const auto& r : results)
            f << r.first << ": " << r.second << " ms\n";
        f << "==================================================\n";
    }

    printf("\n==================================================\n");
    printf("WEAPON BENCH DONE.\n");
    printf("Shots per weapon: %d\n", SHOTS);
    printf("\n");
    for (const auto& r : results)
        printf("  %s: %.3f ms\n", r.first.c_str(), r.second);
    printf("\n");
    printf("Run log:\n%s\n", LogManager::instance().path().c_str());
    printf("Weapon bench log:\n%s\n", benchFile.c_str());
    printf("Ctrl+Click either path above to open it.\n");
    printf("==================================================\n");

    DebugConfig::WEAPON_PERF_SHOTS = true;
}

void registerWeaponBenchCommands()
{
    Terminal::instance().registerCommand({
        "weapon_perf_shots",
        "Toggle per-shot weapon timing summary (shotgun/AA12)",
        "weapon_perf_shots [0|1]",
        [](const std::vector<std::string>& args) {
            if (args.empty())
                DebugConfig::WEAPON_PERF_SHOTS = !DebugConfig::WEAPON_PERF_SHOTS;
            else
                DebugConfig::WEAPON_PERF_SHOTS = args[0] != "0";
            printf("[BENCH] weapon_perf_shots=%d\n", (int)DebugConfig::WEAPON_PERF_SHOTS);
            printf("Logging to: %s\n", LogManager::instance().path().c_str());
            Terminal::instance().addLog(std::string("[BENCH] weapon_perf_shots=") +
                (DebugConfig::WEAPON_PERF_SHOTS ? "ON" : "OFF"));
        }
    });

    Terminal::instance().registerCommand({
        "weapon_bench",
        "Benchmark a specific weapon: weapon_bench <weaponName> <shotCount>",
        "weapon_bench <weaponName> [shotCount]",
        [](const std::vector<std::string>& args) {
            if (args.empty()) {
                Terminal::instance().addLog("[BENCH] Usage: weapon_bench <weaponName> [shotCount]");
                return;
            }
            int shots = 20;
            if (args.size() > 1) shots = std::max(1, std::atoi(args[1].c_str()));
            const WeaponDefinition* def = WeaponRegistry::instance().get(args[0]);
            if (!def) {
                Terminal::instance().addLog(std::string("[BENCH] Unknown weapon: ") + args[0]);
                return;
            }
            printf("Logging to: %s\n", LogManager::instance().path().c_str());
            runWeaponBench(def, shots);
        }
    });

    Terminal::instance().registerCommand({
        "shotgun_bench",
        "Benchmark Shotgun: shotgun_bench <shotCount>",
        "shotgun_bench [shotCount]",
        [](const std::vector<std::string>& args) {
            int shots = 20;
            if (!args.empty()) shots = std::max(1, std::atoi(args[0].c_str()));
            const WeaponDefinition* def = WeaponRegistry::instance().get("shotgun");
            if (!def) { Terminal::instance().addLog("[BENCH] Shotgun not found"); return; }
            printf("Logging to: %s\n", LogManager::instance().path().c_str());
            runWeaponBench(def, shots);
        }
    });

    Terminal::instance().registerCommand({
        "aa12_bench",
        "Benchmark AA12: aa12_bench <shotCount>",
        "aa12_bench [shotCount]",
        [](const std::vector<std::string>& args) {
            int shots = 100;
            if (!args.empty()) shots = std::max(1, std::atoi(args[0].c_str()));
            const WeaponDefinition* def = WeaponRegistry::instance().get("aa12");
            if (!def) { Terminal::instance().addLog("[BENCH] AA12 not found"); return; }
            printf("Logging to: %s\n", LogManager::instance().path().c_str());
            runWeaponBench(def, shots);
        }
    });

    Terminal::instance().registerCommand({
        "weapon_bench_compare",
        "Benchmark all multi-pellet weapons and compare",
        "weapon_bench_compare",
        [](const std::vector<std::string>&) {
            printf("Logging to: %s\n", LogManager::instance().path().c_str());
            runWeaponBenchCompare();
        }
    });
}
