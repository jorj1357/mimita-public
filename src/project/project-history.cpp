// 09 12 2026
/* purpose
* Implements the project version chain and checkout.
* Does NOT compile or activate runtime code.
*/
#include "project/project-history.h"

#include "live-code/code-hash.h"
#include "utils/time-format.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace Project {

namespace {

std::uint64_t nowEpochMs()
{
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::vector<ProjectTreeEntry> parseManifest(const BlobStore& store,
                                            const std::string& treeHash)
{
    std::vector<ProjectTreeEntry> entries;
    ContentId id;
    id.algorithm = "sha256";
    id.digest = treeHash;
    std::string manifest;
    if (!store.read(id, manifest))
        return entries;

    std::size_t cursor = 0;
    while (cursor < manifest.size()) {
        const std::size_t end = manifest.find('\n', cursor);
        if (end == std::string::npos)
            break;
        const std::string line = manifest.substr(cursor, end - cursor);
        cursor = end + 1;
        const std::size_t nul = line.find('\0');
        if (nul == std::string::npos)
            continue;
        const std::string contentStr = line.substr(nul + 1);
        const std::size_t colon = contentStr.find(':');
        if (colon == std::string::npos)
            continue;
        ProjectTreeEntry entry;
        entry.path = line.substr(0, nul);
        entry.content.algorithm = contentStr.substr(0, colon);
        entry.content.digest = contentStr.substr(colon + 1);
        entries.push_back(std::move(entry));
    }
    return entries;
}

} // namespace

ProjectHistory::ProjectHistory(std::filesystem::path root, std::filesystem::path dotDir,
                               TreeFilter filter)
    : root_(std::move(root))
    , dotDir_(std::move(dotDir))
    , filter_(std::move(filter))
    , blobs_(dotDir_ / "store")
{
}

std::string ProjectHistory::changeSetHash(const ChangeSet& changes) const
{
    std::string canonical;
    for (const auto& change : changes.changes) {
        canonical += changeOpName(change.op);
        canonical += '|';
        canonical += change.path;
        canonical += '|';
        canonical += change.oldPath;
        canonical += '|';
        canonical += change.before.str();
        canonical += '|';
        canonical += change.after.str();
        canonical += '\n';
    }
    return LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
}

void ProjectHistory::load()
{
    versions_.clear();
    redoStack_.clear();
    currentHash_.clear();

    std::error_code error;
    std::filesystem::create_directories(dotDir_, error);

    std::ifstream in(dotDir_ / "history.jsonl");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty())
            continue;
        try {
            nlohmann::json json = nlohmann::json::parse(line);
            ProjectVersion version;
            version.versionHash = json.value("version", "");
            version.parentHash = json.value("parent", "");
            version.treeHash = json.value("tree", "");
            version.changeSetHash = json.value("changeSet", "");
            version.changeId = json.value("changeId", "");
            version.label = json.value("label", "");
            version.timestampMs = json.value("timestampMs", (std::uint64_t)0);
            if (!version.versionHash.empty())
                versions_.push_back(std::move(version));
        } catch (...) {
        }
    }

    std::ifstream currentFile(dotDir_ / "current.txt");
    std::getline(currentFile, currentHash_);
    if (currentHash_.empty() && !versions_.empty())
        currentHash_ = versions_.back().versionHash;

    std::ifstream redo(dotDir_ / "redo.txt");
    while (std::getline(redo, line))
        if (!line.empty())
            redoStack_.push_back(line);

    const ProjectVersion* cur = current();
    if (cur) {
        committedTree_.entries = parseManifest(blobs_, cur->treeHash);
        committedTree_.treeHash = cur->treeHash;
        committedValid_ = true;
    }
}

bool ProjectHistory::save() const
{
    std::error_code error;
    std::filesystem::create_directories(dotDir_, error);

    {
        std::ofstream out(dotDir_ / "history.jsonl", std::ios::trunc);
        if (!out.is_open())
            return false;
        for (const auto& version : versions_) {
            nlohmann::json json;
            json["version"] = version.versionHash;
            json["parent"] = version.parentHash;
            json["tree"] = version.treeHash;
            json["changeSet"] = version.changeSetHash;
            json["changeId"] = version.changeId;
            json["label"] = version.label;
            json["timestampMs"] = version.timestampMs;
            out << json.dump() << '\n';
        }
    }
    {
        std::ofstream out(dotDir_ / "current.txt", std::ios::trunc);
        out << currentHash_;
    }
    {
        std::ofstream out(dotDir_ / "redo.txt", std::ios::trunc);
        for (const auto& hash : redoStack_)
            out << hash << '\n';
    }
    return true;
}

bool ProjectHistory::scanCurrent(ProjectTree& out)
{
    return ProjectTreeScanner::scan(root_, filter_, &blobs_, out);
}

bool ProjectHistory::commit(const ProjectTree& tree, const ChangeSet& changes,
                            ProjectVersion& out)
{
    ChangeSet committed = changes;
    committed.parentTreeHash = committedValid_ ? committedTree_.treeHash : std::string();
    committed.newTreeHash = tree.treeHash;
    committed.changeId = LiveCodeHash::sha256Bytes(tree.treeHash.data(), tree.treeHash.size());

    out.parentHash = currentHash_;
    out.treeHash = tree.treeHash;
    out.changeSetHash = changeSetHash(committed);
    out.changeId = committed.changeId;
    out.timestampMs = nowEpochMs();

    const std::string identity = out.parentHash + "|" + out.treeHash + "|" +
                                 out.changeSetHash + "|" + std::to_string(out.timestampMs);
    out.versionHash = LiveCodeHash::sha256Bytes(identity.data(), identity.size());

    versions_.push_back(out);
    currentHash_ = out.versionHash;
    redoStack_.clear();
    committedTree_ = tree;
    committedValid_ = true;
    save();
    return true;
}

bool ProjectHistory::record(ChangeSet& outChanges)
{
    ProjectTree tree;
    if (!scanCurrent(tree))
        return false;

    ProjectTree before;
    before.treeHash = committedValid_ ? committedTree_.treeHash : std::string();
    before.entries = committedValid_ ? committedTree_.entries
                                     : std::vector<ProjectTreeEntry>();
    ProjectTreeScanner::diff(before, tree, outChanges);
    if (outChanges.empty() && committedValid_) {
        committedTree_ = tree;
        return false;
    }

    ProjectVersion version;
    return commit(tree, outChanges, version);
}

const ProjectVersion* ProjectHistory::current() const
{
    return find(currentHash_);
}

const ProjectVersion* ProjectHistory::find(const std::string& versionHash) const
{
    for (const auto& version : versions_)
        if (version.versionHash == versionHash)
            return &version;
    return nullptr;
}

std::vector<ProjectVersion> ProjectHistory::versions() const
{
    return versions_;
}

bool ProjectHistory::canUndo() const
{
    const ProjectVersion* cur = current();
    return cur && !cur->parentHash.empty() && find(cur->parentHash) != nullptr;
}

bool ProjectHistory::canRedo() const
{
    return !redoStack_.empty();
}

bool ProjectHistory::checkout(const std::string& treeHash)
{
    return ProjectTreeScanner::checkout(root_, filter_, blobs_, treeHash);
}

bool ProjectHistory::undo()
{
    const ProjectVersion* cur = current();
    if (!cur || cur->parentHash.empty())
        return false;
    const ProjectVersion* parent = find(cur->parentHash);
    if (!parent)
        return false;

    const std::string oldHash = currentHash_;
    if (!checkout(parent->treeHash))
        return false;
    currentHash_ = parent->versionHash;
    redoStack_.push_back(oldHash);
    committedTree_.entries = parseManifest(blobs_, parent->treeHash);
    committedTree_.treeHash = parent->treeHash;
    committedValid_ = true;
    save();
    return true;
}

bool ProjectHistory::redo()
{
    if (redoStack_.empty())
        return false;
    const std::string targetHash = redoStack_.back();
    const ProjectVersion* target = find(targetHash);
    if (!target)
        return false;
    if (!checkout(target->treeHash))
        return false;
    redoStack_.pop_back();
    currentHash_ = target->versionHash;
    committedTree_.entries = parseManifest(blobs_, target->treeHash);
    committedTree_.treeHash = target->treeHash;
    committedValid_ = true;
    save();
    return true;
}

bool ProjectHistory::checkpoint(const std::string& label)
{
    for (auto& version : versions_)
        if (version.versionHash == currentHash_) {
            version.label = label;
            save();
            return true;
        }
    return false;
}

bool ProjectHistory::restore(const std::string& versionHash)
{
    const ProjectVersion* target = find(versionHash);
    if (!target)
        return false;
    if (!checkout(target->treeHash))
        return false;
    currentHash_ = target->versionHash;
    redoStack_.clear();
    committedTree_.entries = parseManifest(blobs_, target->treeHash);
    committedTree_.treeHash = target->treeHash;
    committedValid_ = true;
    save();
    return true;
}

bool ProjectHistory::diffVersions(const std::string& a, const std::string& b,
                                  ChangeSet& out) const
{
    const ProjectVersion* va = find(a);
    const ProjectVersion* vb = find(b);
    if (!va || !vb)
        return false;
    ProjectTree ta;
    ta.treeHash = va->treeHash;
    ta.entries = parseManifest(blobs_, va->treeHash);
    ProjectTree tb;
    tb.treeHash = vb->treeHash;
    tb.entries = parseManifest(blobs_, vb->treeHash);
    ProjectTreeScanner::diff(ta, tb, out);
    return true;
}

} // namespace Project
