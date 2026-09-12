// 09 12 2026
/* purpose
* Implements the shared project authoring command surface.
* Does NOT compile or activate code.
*/
#include "project/project-control.h"

#include "project/state-schema.h"

#include <sstream>

#include <nlohmann/json.hpp>

namespace Project {

ProjectControl& ProjectControl::instance()
{
    static ProjectControl control;
    return control;
}

void ProjectControl::init(const std::filesystem::path& root)
{
    root_ = root;
    // Phase 0 scope: source only. Assets/shaders/config join the same tree
    // later without changing the model.
    filter_.includePrefixes = {"src/"};
    filter_.includeExtensions = {".cpp", ".h", ".hpp", ".cc", ".cxx", ".c"};
    filter_.excludePrefixes = {"src/generated/"};

    history_ = std::make_unique<ProjectHistory>(root_, root_ / ".mimita", filter_);
    history_->load();
    initialized_ = true;
}

bool ProjectControl::ready() const
{
    return initialized_ && history_ != nullptr;
}

std::string ProjectControl::record()
{
    if (!ready())
        return "project not initialized";
    ChangeSet changes;
    const bool committed = history_->record(changes);
    std::ostringstream summary;
    if (!committed) {
        summary << "no project change";
        lastResponse_ = summary.str();
        return lastResponse_;
    }
    summary << "project version " << history_->current()->versionHash.substr(0, 12)
            << " changes=" << changes.changes.size();
    for (const auto& change : changes.changes)
        summary << " " << changeOpName(change.op) << ":" << change.path;
    lastResponse_ = summary.str();
    return lastResponse_;
}

bool ProjectControl::undo()
{
    return ready() && history_->undo();
}

bool ProjectControl::redo()
{
    return ready() && history_->redo();
}

bool ProjectControl::checkpoint(const std::string& label)
{
    return ready() && history_->checkpoint(label);
}

bool ProjectControl::restore(const std::string& versionHash)
{
    return ready() && history_->restore(versionHash);
}

std::string ProjectControl::statusJson() const
{
    nlohmann::json json;
    if (!ready()) {
        json["ready"] = false;
        return json.dump();
    }
    const ProjectVersion* current = history_->current();
    json["ready"] = true;
    json["root"] = root_.string();
    json["versionCount"] = history_->versions().size();
    json["canUndo"] = history_->canUndo();
    json["canRedo"] = history_->canRedo();
    json["blobCount"] = history_->blobs().blobCount();
    json["schemaCount"] = StateSchemaRegistry::instance().schemaCount();
    json["migrationCount"] = StateSchemaRegistry::instance().migrationCount();
    if (current) {
        json["currentVersion"] = current->versionHash;
        json["currentTree"] = current->treeHash;
        json["label"] = current->label;
        json["timestampMs"] = current->timestampMs;
    }
    return json.dump();
}

std::string ProjectControl::historyJson() const
{
    nlohmann::json json = nlohmann::json::array();
    if (ready()) {
        for (const auto& version : history_->versions()) {
            nlohmann::json entry;
            entry["version"] = version.versionHash;
            entry["parent"] = version.parentHash;
            entry["tree"] = version.treeHash;
            entry["label"] = version.label;
            entry["timestampMs"] = version.timestampMs;
            json.push_back(std::move(entry));
        }
    }
    return json.dump();
}

std::string ProjectControl::diffJson(const std::string& a, const std::string& b) const
{
    nlohmann::json json;
    if (!ready()) {
        json["ready"] = false;
        return json.dump();
    }
    ChangeSet changes;
    if (!history_->diffVersions(a, b, changes)) {
        json["error"] = "unknown version";
        return json.dump();
    }
    json["parentTree"] = changes.parentTreeHash;
    json["newTree"] = changes.newTreeHash;
    json["changes"] = nlohmann::json::array();
    for (const auto& change : changes.changes) {
        nlohmann::json entry;
        entry["op"] = changeOpName(change.op);
        entry["path"] = change.path;
        if (!change.oldPath.empty())
            entry["oldPath"] = change.oldPath;
        json["changes"].push_back(std::move(entry));
    }
    return json.dump();
}

std::string ProjectControl::handle(const std::string& commandLine)
{
    if (!ready())
        return "error: project not initialized";

    std::istringstream stream(commandLine);
    std::string command;
    stream >> command;

    if (command == "status")
        return statusJson();
    if (command == "history")
        return historyJson();
    if (command == "record")
        return record();
    if (command == "undo")
        return undo() ? "undo ok" : "undo unavailable";
    if (command == "redo")
        return redo() ? "redo ok" : "redo unavailable";
    if (command == "checkpoint") {
        std::string label;
        std::getline(stream, label);
        while (!label.empty() && label.front() == ' ')
            label.erase(label.begin());
        return checkpoint(label) ? "checkpoint ok" : "checkpoint failed";
    }
    if (command == "restore") {
        std::string hash;
        stream >> hash;
        return restore(hash) ? "restore ok" : "restore failed";
    }
    if (command == "diff") {
        std::string a, b;
        stream >> a >> b;
        return diffJson(a, b);
    }
    return "error: unknown project command";
}

} // namespace Project
