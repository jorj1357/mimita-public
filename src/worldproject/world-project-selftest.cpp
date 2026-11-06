// 2026-11-05
#include "worldproject/world-project-store.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "worldproject/world-project-sha256.h"

namespace MimitaWorldProject {
namespace {

namespace fs = std::filesystem;

void appendResult(std::string& report, bool ok, const std::string& name,
                  const std::string& detail = {})
{
    report += std::string(ok ? "PASS " : "FAIL ") + name;
    if (!detail.empty()) report += " (" + detail + ")";
    report += "\n";
}

bool writeText(const fs::path& path, const std::string& text)
{
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out << text;
    return out.good();
}

std::string canonical(const nlohmann::json& value)
{
    return value.dump() + "\n";
}

} // namespace

bool worldProjectStoreSelfTest(std::string& report)
{
    report.clear();
    bool ok = true;
    auto check = [&](bool condition, const std::string& name,
                     const std::string& detail = {}) {
        appendResult(report, condition, name, detail);
        ok = ok && condition;
    };

    std::string hashReport;
    check(sha256SelfTest(hashReport), "sha256 vectors");
    report += hashReport;

    WorldProjectStore& store = WorldProjectStore::instance();
    const std::string previousRoot = store.root();
    const fs::path tempRoot = fs::temp_directory_path() /
        ("mimita-worldproject-selftest-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    store.setRoot(tempRoot.string());

    const std::string mapOne = canonical(nlohmann::json{
        {"schema", 2},
        {"entities", nlohmann::json::array()},
        {"blocks", nlohmann::json::array()},
        {"title", "selftest-one"},
    });
    const std::string mapTwo = canonical(nlohmann::json{
        {"schema", 2},
        {"entities", nlohmann::json::array()},
        {"blocks", nlohmann::json::array({{{"id", "b1"}, {"position", {1, 2, 3}}}})},
        {"title", "selftest-two"},
    });

    std::string error;
    std::string versionOne;
    check(store.createFromMap("store_selftest", "owner", "Store Selftest",
                              "public", mapOne, &error),
          "create project", error);

    WorldProjectManifest manifest;
    error.clear();
    check(store.readManifest("store_selftest", manifest, &error) &&
              manifest.format == kWorldProjectFormat,
          "manifest is format 2", error);

    std::vector<WorldProjectVersion> versions;
    error.clear();
    check(store.listVersions("store_selftest", versions, &error) &&
              versions.size() == 1,
          "create writes one version", error);
    if (!versions.empty()) versionOne = versions.front().version;

    std::string versionAgain;
    error.clear();
    check(store.saveVersion("store_selftest", "owner", mapOne, "",
                            &versionAgain, &error) &&
              versionAgain == versionOne,
          "unchanged content is idempotent", error);
    versions.clear();
    store.listVersions("store_selftest", versions);
    check(versions.size() == 1, "unchanged save does not add a version");

    std::string versionTwo;
    error.clear();
    check(store.saveVersion("store_selftest", "owner", mapTwo, "two",
                            &versionTwo, &error) &&
              !versionTwo.empty() && versionTwo != versionOne,
          "changed content creates a new version", error);

    std::string loaded;
    WorldProjectVersion versionMeta;
    error.clear();
    check(store.readVersion("store_selftest", versionOne, loaded, &versionMeta,
                            &error) &&
              loaded == mapOne,
          "checkout verifies exact bytes", error);
    check(versionMeta.parent.empty(), "first version has no parent");

    error.clear();
    check(store.readVersion("store_selftest", versionTwo, loaded, &versionMeta,
                            &error) &&
              versionMeta.parent == versionOne,
          "second version records parent", error);

    const fs::path versionOneMap = tempRoot / "store_selftest" / "versions" /
        versionOne / "map.json";
    std::string originalBytes;
    {
        std::ifstream in(versionOneMap, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        originalBytes = buffer.str();
    }
    writeText(versionOneMap, "{\"tampered\":true}\n");
    error.clear();
    check(!store.readVersion("store_selftest", versionOne, loaded, nullptr, &error),
          "tampered version is rejected", error);
    writeText(versionOneMap, originalBytes);

    error.clear();
    check(store.fork("store_selftest", versionOne, "store_fork", "guest",
                     "Fork", "public", &error),
          "fork selected version", error);
    WorldProjectManifest forkManifest;
    error.clear();
    check(store.readManifest("store_fork", forkManifest, &error) &&
              forkManifest.parent.present &&
              forkManifest.parent.projectId == "store_selftest" &&
              forkManifest.parent.version == versionOne &&
              forkManifest.head == versionOne,
          "fork preserves lineage", error);

    WorldProjectExportResult exported;
    error.clear();
    const fs::path exportDir = tempRoot / "exported";
    check(store.exportPackage("store_selftest", versionTwo, exportDir.string(),
                              &exported, &error),
          "export package", error);

    WorldProjectImportResult imported;
    error.clear();
    check(store.importPackage(exportDir.string(), "store_import", &imported,
                              &error),
          "import package", error);
    std::string importedMap;
    error.clear();
    check(store.readVersion("store_import", imported.version, importedMap,
                            nullptr, &error) &&
              importedMap == mapTwo,
          "imported version verifies", error);

    // Format-1 migration keeps the original manifest as project-format1.json.
    const fs::path legacyDir = tempRoot / "legacy_project";
    const std::string legacyMap = canonical(nlohmann::json{
        {"entities", nlohmann::json::array()},
    });
    const std::string legacyHash = sha256Hex(legacyMap);
    const fs::path legacyVersion = legacyDir / "versions" / legacyHash;
    writeText(legacyDir / "working" / "map.json", legacyMap);
    writeText(legacyVersion / "map.json", legacyMap);
    writeText(legacyVersion / "version.json", canonical(nlohmann::json{
        {"version", legacyHash},
        {"parent", nullptr},
        {"map_sha256", legacyHash},
        {"created_utc", WorldProjectStore::utcNow()},
        {"author_account_id", "legacy-owner"},
    }));
    writeText(legacyDir / "project.json", canonical(nlohmann::json{
        {"format", 1},
        {"project_id", "legacy_project"},
        {"owner_account_id", "legacy-owner"},
        {"title", "Legacy"},
        {"visibility", "public"},
        {"license", nlohmann::json{
            {"code_default", "MIT"},
            {"media_default", "CC-BY-4.0"},
            {"dependency_licenses", nlohmann::json::array()},
            {"status", "creator-confirmation-required"},
        }},
        {"parent", nullptr},
        {"head", legacyHash},
        {"created_utc", WorldProjectStore::utcNow()},
    }));

    error.clear();
    check(store.migrateFormat1("legacy_project", &error),
          "migrate format 1", error);
    WorldProjectManifest migrated;
    error.clear();
    check(store.readManifest("legacy_project", migrated, &error) &&
              migrated.format == kWorldProjectFormat &&
              fs::exists(legacyDir / "project-format1.json"),
          "migration preserves source manifest", error);
    std::string legacyLoaded;
    error.clear();
    check(store.readVersion("legacy_project", legacyHash, legacyLoaded, nullptr,
                            &error) &&
              legacyLoaded == legacyMap,
          "migrated version verifies", error);

    WorldProjectDraft draft;
    draft.draftId = "guest_draft";
    draft.projectId = "store_selftest";
    draft.sessionId = "session-1";
    draft.authorAccountId = "";
    draft.baseVersion = versionTwo;
    draft.stateJson = mapOne;
    draft.pendingOperations = {"op-1"};
    error.clear();
    check(store.writeDraft(draft, &error), "guest draft write", error);
    std::vector<WorldProjectDraft> drafts;
    error.clear();
    check(store.listDrafts("store_selftest", drafts, &error) &&
              drafts.size() == 1 && drafts.front().authorAccountId.empty(),
          "guest draft listed", error);

    std::string promoted;
    error.clear();
    check(store.promoteDraft("store_selftest", "guest_draft", "", &promoted,
                             &error) &&
              !promoted.empty(),
          "draft promotes to version", error);

    {
        WorldProjectStore& same = WorldProjectStore::instance();
        std::vector<WorldProjectSummary> projects;
        error.clear();
        check(same.listProjects(projects, &error) && projects.size() >= 3,
              "projects persist in store root", error);
    }

    store.setRoot(previousRoot);
    fs::remove_all(tempRoot, ec);
    return ok;
}

} // namespace MimitaWorldProject
