// Pure test: community mode-pack registry discovery + validation.
//
// Build (from repo root):
//   g++ -std=c++17 -O2 -Iinclude -Isrc tests/mode-pack-registry-test.cpp \
//       src/gamemode/mode-pack-registry.cpp src/gamemode/capability-registry.cpp \
//       -o build/mode-pack-registry-test.exe
//   build/mode-pack-registry-test.exe

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "gamemode/mode-pack-registry.h"

namespace fs = std::filesystem;
using namespace MimitaGamemode;

static int gChecks = 0;
static int gFailures = 0;

static void check(bool condition, const char* what)
{
    ++gChecks;
    if (!condition) {
        ++gFailures;
        std::printf("  FAIL: %s\n", what);
    }
}

static fs::path makeTmpDir(const char* name)
{
    fs::path dir = fs::temp_directory_path() / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

static void writeFile(const fs::path& path, const std::string& contents)
{
    std::ofstream out(path);
    out << contents;
}

static std::string validPack(const std::string& id,
                             const std::string& name = "Test Mode",
                             const std::string& extraCapability = "")
{
    std::string capabilities =
        "\"lifecycle.intermission_countdown\",\n"
        "    \"participants.all_actors\",\n"
        "    \"inventory.random_per_actor\",\n"
        "    \"win.last_actor_alive\",\n"
        "    \"presentation.disaster_banner\"";
    if (!extraCapability.empty())
        capabilities += ",\n    \"" + extraCapability + "\"";
    return std::string("{\n") +
        "  \"schema_version\": 1,\n" +
        "  \"id\": \"" + id + "\",\n" +
        "  \"name\": \"" + name + "\",\n" +
        "  \"gamemode_id\": \"" + id + "\",\n" +
        "  \"capabilities\": [\n    " + capabilities + "\n  ],\n" +
        "  \"disasters\": [\n" +
        "    { \"id\": \"random_weapon_last_alive\", \"name\": \"RW\", \"duration_seconds\": 60,\n" +
        "      \"weapon_pool\": [\"revolver\", \"shotgun\"] }\n" +
        "  ]\n" +
        "}\n";
}

int main()
{
    // ── 1. One valid pack. ──────────────────────────────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-one");
        writeFile(dir / "anything.json", validPack("sandbox_plus", "Sandbox Plus"));
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(ok, "one valid pack loads");
        check(ModePackRegistry::instance().size() == 1, "one pack in catalog");
        const ModePack* pack = ModePackRegistry::instance().get("sandbox_plus");
        check(pack != nullptr, "pack is found by stable id");
        check(pack && pack->name == "Sandbox Plus", "display name parsed");
        // Id comes from the manifest, not the filename.
        check(ModePackRegistry::instance().get("anything") == nullptr,
              "id is independent of filename");
    }

    // ── 2. Many packs, no fixed-count assumption. ───────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-many");
        for (int i = 0; i < 40; ++i)
            writeFile(dir / ("pack_" + std::to_string(i) + ".json"),
                      validPack("mode_" + std::to_string(i)));
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(ok, "many packs load without a fixed limit");
        check(ModePackRegistry::instance().size() == 40, "all 40 packs in catalog");
        check(diagnostics.empty(), "no diagnostics on a clean load");
    }

    // ── 3. Stable ids independent of display names. ─────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-stable");
        writeFile(dir / "a.json", validPack("stable_id", "Display Name One"));
        ModePackRegistry::instance().loadDirectory(dir.string());
        check(ModePackRegistry::instance().has("stable_id"), "id resolves");
        check(!ModePackRegistry::instance().has("Display Name One"),
              "display name is not used as an id");
    }

    // ── 4. Unknown capability is rejected, previous catalog retained. ─
    {
        const fs::path dir = makeTmpDir("mimita-pack-unknown");
        writeFile(dir / "a.json", validPack("keep_me", "Keep Me"));
        ModePackRegistry::instance().loadDirectory(dir.string());
        check(ModePackRegistry::instance().has("keep_me"), "baseline catalog loaded");

        writeFile(dir / "b.json", validPack("bad_cap", "Bad Cap", "nonsense.capability"));
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(!ok, "unknown capability fails the load");
        check(!diagnostics.empty(), "unknown capability produces a diagnostic");
        bool named = false;
        for (const auto& line : diagnostics)
            named |= line.find("bad_cap") != std::string::npos &&
                     line.find("nonsense.capability") != std::string::npos;
        check(named, "diagnostic names the pack and the capability");
        check(ModePackRegistry::instance().has("keep_me"),
              "previous catalog retained after unknown capability");
        check(!ModePackRegistry::instance().has("bad_cap"),
              "failed pack is not in the catalog");
    }

    // ── 5. Malformed JSON is rejected atomically. ───────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-malformed");
        writeFile(dir / "a.json", validPack("good_mode"));
        ModePackRegistry::instance().loadDirectory(dir.string());
        writeFile(dir / "b.json", "{ this is not json ");
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(!ok, "malformed JSON fails the load");
        check(ModePackRegistry::instance().has("good_mode"),
              "previous catalog retained after malformed JSON");
    }

    // ── 6. Duplicate ids are rejected. ──────────────────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-duplicate");
        writeFile(dir / "a.json", validPack("dupe"));
        writeFile(dir / "b.json", validPack("dupe"));
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(!ok, "duplicate ids fail the load");
        bool named = false;
        for (const auto& line : diagnostics)
            named |= line.find("duplicate") != std::string::npos;
        check(named, "duplicate diagnostic is explicit");
    }

    // ── 7. Unsupported schema version is rejected. ──────────────────
    {
        const fs::path dir = makeTmpDir("mimita-pack-schema");
        writeFile(dir / "a.json",
                  "{\"schema_version\": 99, \"id\": \"future\", \"capabilities\": []}");
        std::vector<std::string> diagnostics;
        const bool ok = ModePackRegistry::instance().loadDirectory(dir.string(), &diagnostics);
        check(!ok, "unsupported schema version fails the load");
    }

    std::printf("mode-pack-registry-test: %d checks, %d failures\n", gChecks, gFailures);
    std::printf("%s\n", gFailures == 0 ? "PASS" : "FAIL");
    return gFailures == 0 ? 0 : 1;
}
