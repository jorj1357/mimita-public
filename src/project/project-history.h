// 09 12 2026
/* purpose
* Own the project version chain: scan, diff, commit, undo/redo, checkpoint,
* restore, and checkout. Source history is independent of runtime generations.
* Does NOT compile or activate runtime code.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "project/blob-store.h"
#include "project/project-tree.h"
#include "project/project-types.h"

namespace Project {

class ProjectHistory {
public:
    ProjectHistory(std::filesystem::path root, std::filesystem::path dotDir,
                   TreeFilter filter);

    void load();
    BlobStore& blobs() { return blobs_; }
    const std::filesystem::path& root() const { return root_; }

    // Scans the project tree (storing blobs) into `out`.
    bool scanCurrent(ProjectTree& out);

    // Scans, diffs against the last committed tree, and commits a new version
    // when something changed. Returns false when there is no change.
    bool record(ChangeSet& outChanges);

    bool commit(const ProjectTree& tree, const ChangeSet& changes, ProjectVersion& out);

    bool undo();
    bool redo();
    bool checkpoint(const std::string& label);
    bool restore(const std::string& versionHash);
    bool checkout(const std::string& treeHash);

    const ProjectVersion* current() const;
    const ProjectVersion* find(const std::string& versionHash) const;
    std::vector<ProjectVersion> versions() const;
    bool canUndo() const;
    bool canRedo() const;

    bool diffVersions(const std::string& a, const std::string& b, ChangeSet& out) const;

    bool save() const;

private:
    std::string changeSetHash(const ChangeSet& changes) const;
    const ProjectTreeEntry* manifestEntry(const std::string& treeHash,
                                          const std::string& path) const;

    std::filesystem::path root_;
    std::filesystem::path dotDir_;
    TreeFilter filter_;
    BlobStore blobs_;
    std::vector<ProjectVersion> versions_;
    std::vector<std::string> redoStack_;
    std::string currentHash_;
    ProjectTree committedTree_;
    bool committedValid_ = false;
};

} // namespace Project
