// 2026-11-05
#include "mapeditor/map-editor-projects.h"

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "gamemode/map-config.h"
#include "worldproject/world-project-store.h"

namespace {

namespace fs = std::filesystem;
using MimitaWorldProject::WorldProjectStore;
using MimitaWorldProject::WorldProjectVersion;

std::string gCurrentProjectId;

void log(const std::string& message)
{
    Terminal::instance().addLog(message);
}

void event(const char* name, const std::string& subject,
           const nlohmann::json& fields)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important, name, subject,
        "world project operation", 0, fields, __FILE__, __LINE__, __func__);
}

std::string workingPath(const std::string& projectId)
{
    return (fs::path(WorldProjectStore::instance().root()) / projectId /
            "working" / "map.json").string();
}

bool openProject(const std::string& projectId, const std::string& version,
                 std::string& error)
{
    WorldProjectStore& store = WorldProjectStore::instance();
    std::string mapJson;
    if (version.empty()) {
        if (!store.readWorking(projectId, mapJson, &error)) {
            MimitaWorldProject::WorldProjectManifest manifest;
            if (!store.readManifest(projectId, manifest, &error))
                return false;
            if (manifest.head.empty()) {
                error = "project has no saved version";
                return false;
            }
            if (!store.readVersion(projectId, manifest.head, mapJson, nullptr,
                                   &error))
                return false;
        }
    } else if (!store.readVersion(projectId, version, mapJson, nullptr, &error)) {
        return false;
    }

    if (!store.writeWorking(projectId, mapJson, &error))
        return false;
    if (!MapConfigRegistry::instance().loadFromFile(projectId,
            workingPath(projectId))) {
        error = "project map could not be loaded";
        return false;
    }
    gCurrentProjectId = projectId;
    return true;
}

} // namespace

std::string currentWorldProjectId()
{
    return gCurrentProjectId;
}

void registerWorldProjectCommands()
{
    Terminal& terminal = Terminal::instance();
    terminal.registerCommand({
        "worldproject",
        "Create, open, fork, save, export, or recover native world projects",
        "worldproject <list|status|create <id> [title]|open <id> [version]|save [title]|fork <source> <new-id> [version]|versions <id>|export <id> <dir> [version]|import <dir> [new-id]|drafts <id>|recover <id> <draft>|migrate>",
        [](const std::vector<std::string>& args) {
            WorldProjectStore& store = WorldProjectStore::instance();
            if (args.empty()) {
                log("[WORLDPROJECT] " + currentWorldProjectId());
                return;
            }
            const std::string& command = args[0];
            std::string error;

            if (command == "list") {
                std::vector<MimitaWorldProject::WorldProjectSummary> projects;
                if (!store.listProjects(projects, &error)) {
                    log("[WORLDPROJECT] list failed: " + error);
                    return;
                }
                for (const auto& project : projects) {
                    log("[WORLDPROJECT] " + project.manifest.projectId +
                        " head=" + (project.manifest.head.empty()
                                        ? "unsaved" : project.manifest.head.substr(0, 12)) +
                        " versions=" + std::to_string(project.versionCount) +
                        " visibility=" + project.manifest.visibility +
                        " title=" + project.manifest.title);
                }
                if (projects.empty())
                    log("[WORLDPROJECT] no local projects");
                return;
            }

            if (command == "status") {
                log("[WORLDPROJECT] root=" + store.root() +
                    " current=" + (currentWorldProjectId().empty()
                                       ? std::string("none")
                                       : currentWorldProjectId()));
                return;
            }

            if (command == "create") {
                if (args.size() < 2) {
                    log("[WORLDPROJECT] create <id> [title]");
                    return;
                }
                const std::string title = args.size() >= 3 ? args[2] : args[1];
                if (!store.createFromMap(args[1], "local", title, "public",
                                         MapConfigRegistry::instance().toJsonString(),
                                         &error)) {
                    log("[WORLDPROJECT] create failed: " + error);
                    return;
                }
                gCurrentProjectId = args[1];
                event("world-project.created", args[1],
                      {{"project", args[1]}, {"owner", "local"},
                       {"root", store.root()}, {"source", "map_editor"}});
                log("[WORLDPROJECT] created " + args[1]);
                return;
            }

            if (command == "open") {
                if (args.size() < 2) {
                    log("[WORLDPROJECT] open <id> [version]");
                    return;
                }
                const std::string version = args.size() >= 3 ? args[2] : "";
                if (!openProject(args[1], version, error)) {
                    log("[WORLDPROJECT] open failed: " + error);
                    return;
                }
                event("world-project.opened", args[1],
                      {{"project", args[1]},
                       {"version", version.empty() ? std::string("working") : version},
                       {"root", store.root()}});
                log("[WORLDPROJECT] opened " + args[1] +
                    (version.empty() ? std::string() : " @" + version.substr(0, 12)));
                return;
            }

            if (command == "save") {
                if (gCurrentProjectId.empty()) {
                    log("[WORLDPROJECT] no current project; create or open one");
                    return;
                }
                const std::string mapJson =
                    MapConfigRegistry::instance().toJsonString();
                std::string version;
                if (!store.writeWorking(gCurrentProjectId, mapJson, &error) ||
                    !store.saveVersion(gCurrentProjectId, "local", mapJson,
                                       args.size() >= 2 ? args[1] : "",
                                       &version, &error)) {
                    log("[WORLDPROJECT] save failed: " + error);
                    return;
                }
                event("world-project.version-saved", gCurrentProjectId,
                      {{"project", gCurrentProjectId}, {"version", version},
                       {"author", "local"}, {"root", store.root()}});
                log("[WORLDPROJECT] saved " + gCurrentProjectId + " @" +
                    version.substr(0, 12));
                return;
            }

            if (command == "fork") {
                if (args.size() < 3) {
                    log("[WORLDPROJECT] fork <source> <new-id> [version]");
                    return;
                }
                const std::string version = args.size() >= 4 ? args[3] : "";
                if (!store.fork(args[1], version, args[2], "local", args[2],
                                "public", &error)) {
                    log("[WORLDPROJECT] fork failed: " + error);
                    return;
                }
                if (!openProject(args[2], "", error)) {
                    log("[WORLDPROJECT] forked but open failed: " + error);
                    return;
                }
                event("world-project.forked", args[2],
                      {{"source", args[1]}, {"fork", args[2]},
                       {"version", version}, {"owner", "local"}});
                log("[WORLDPROJECT] forked " + args[1] + " as " + args[2]);
                return;
            }

            if (command == "versions") {
                if (args.size() < 2) {
                    log("[WORLDPROJECT] versions <id>");
                    return;
                }
                std::vector<WorldProjectVersion> versions;
                if (!store.listVersions(args[1], versions, &error)) {
                    log("[WORLDPROJECT] versions failed: " + error);
                    return;
                }
                for (const auto& version : versions) {
                    log("[WORLDPROJECTVERSION] " + version.version.substr(0, 12) +
                        " parent=" + (version.parent.empty()
                                          ? std::string("none")
                                          : version.parent.substr(0, 12)) +
                        " author=" + version.authorAccountId +
                        " created=" + version.createdUtc);
                }
                return;
            }

            if (command == "export") {
                if (args.size() < 3) {
                    log("[WORLDPROJECT] export <id> <dir> [version]");
                    return;
                }
                MimitaWorldProject::WorldProjectExportResult result;
                if (!store.exportPackage(args[1],
                        args.size() >= 4 ? args[3] : "", args[2], &result,
                        &error)) {
                    log("[WORLDPROJECT] export failed: " + error);
                    return;
                }
                event("world-project.exported", args[1],
                      {{"project", args[1]}, {"version", result.version},
                       {"directory", result.directory},
                       {"manifest_sha256", result.manifestSha256},
                       {"map_sha256", result.mapSha256}});
                log("[WORLDPROJECT] exported " + result.version.substr(0, 12) +
                    " to " + result.directory);
                return;
            }

            if (command == "import") {
                if (args.size() < 2) {
                    log("[WORLDPROJECT] import <dir> [new-id]");
                    return;
                }
                MimitaWorldProject::WorldProjectImportResult result;
                if (!store.importPackage(args[1],
                        args.size() >= 3 ? args[2] : "", &result, &error)) {
                    log("[WORLDPROJECT] import failed: " + error);
                    return;
                }
                if (!openProject(result.projectId, "", error)) {
                    log("[WORLDPROJECT] imported but open failed: " + error);
                    return;
                }
                event("world-project.imported", result.projectId,
                      {{"project", result.projectId}, {"version", result.version},
                       {"directory", result.directory}});
                log("[WORLDPROJECT] imported " + result.projectId + " @" +
                    result.version.substr(0, 12));
                return;
            }

            if (command == "drafts") {
                if (args.size() < 2) {
                    log("[WORLDPROJECT] drafts <id>");
                    return;
                }
                std::vector<MimitaWorldProject::WorldProjectDraft> drafts;
                if (!store.listDrafts(args[1], drafts, &error)) {
                    log("[WORLDPROJECT] drafts failed: " + error);
                    return;
                }
                for (const auto& draft : drafts) {
                    log("[WORLDPROJECTDRAFT] " + draft.draftId +
                        " author=" + (draft.authorAccountId.empty()
                                          ? std::string("guest")
                                          : draft.authorAccountId) +
                        " pending=" + std::to_string(draft.pendingOperations.size()));
                }
                return;
            }

            if (command == "recover") {
                if (args.size() < 3) {
                    log("[WORLDPROJECT] recover <id> <draft>");
                    return;
                }
                std::string version;
                if (!store.promoteDraft(args[1], args[2], "local", &version,
                                        &error)) {
                    log("[WORLDPROJECT] recover failed: " + error);
                    return;
                }
                if (!openProject(args[1], "", error)) {
                    log("[WORLDPROJECT] recovered but open failed: " + error);
                    return;
                }
                event("world-project.recovered", args[1],
                      {{"project", args[1]}, {"draft", args[2]},
                       {"version", version}});
                log("[WORLDPROJECT] recovered draft " + args[2] + " @" +
                    version.substr(0, 12));
                return;
            }

            if (command == "migrate") {
                std::vector<std::string> migrated;
                if (!store.migrateAllFormat1(migrated, &error)) {
                    log("[WORLDPROJECT] migrate failed: " + error);
                    return;
                }
                event("world-project.migrated", "world-project",
                      {{"count", migrated.size()}, {"root", store.root()}});
                log("[WORLDPROJECT] migrated " + std::to_string(migrated.size()) +
                    " format-1 project(s)");
                return;
            }

            log("[WORLDPROJECT] unknown command: " + command);
        }
    });
}
