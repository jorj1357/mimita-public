// 09 12 2026
/* purpose
* Scan a source/project subtree into content-addressed entries and compute a
* whole-tree hash. Diff two trees into a ChangeSet with add/delete/modify/
* rename/move detection (rename/move inferred from identical content).
* Does NOT own history, builds, or runtime generations.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "project/blob-store.h"
#include "project/project-types.h"

namespace Project {

struct ProjectTreeEntry {
    std::string path;   // relative, forward slashes
    ContentId content;  // blob id
    std::uint64_t size = 0;
};

struct ProjectTree {
    std::string treeHash;
    std::vector<ProjectTreeEntry> entries;  // sorted by path

    const ProjectTreeEntry* find(const std::string& path) const;
};

struct TreeFilter {
    std::vector<std::string> includePrefixes;   // relative dirs, e.g. "src/"
    std::vector<std::string> includeExtensions; // e.g. ".cpp", ".h"
    std::vector<std::string> excludePrefixes;   // e.g. "src/generated/"
};

class ProjectTreeScanner {
public:
    // Scans and (when store != nullptr) stores new blobs so history is
    // recoverable. Returns false only on fatal filesystem errors.
    static bool scan(const std::filesystem::path& root, const TreeFilter& filter,
                     BlobStore* store, ProjectTree& out);

    static std::string canonical(const std::vector<ProjectTreeEntry>& entries);
    static std::string hashEntries(const std::vector<ProjectTreeEntry>& entries);
    static void diff(const ProjectTree& before, const ProjectTree& after,
                     ChangeSet& out);

    // Restores a tree manifest (blob id = {"sha256", treeHash}) onto disk.
    static bool checkout(const std::filesystem::path& root, const TreeFilter& filter,
                         BlobStore& store, const std::string& treeHash);
};

} // namespace Project
