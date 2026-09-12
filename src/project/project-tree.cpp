// 09 12 2026
/* purpose
* Implements project tree scanning, tree hashing, and tree diffs.
* Does NOT own history or builds.
*/
#include "project/project-tree.h"

#include "live-code/code-hash.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <system_error>
#include <unordered_map>

namespace Project {

namespace {

bool matchesFilter(const std::filesystem::path& relative, const TreeFilter& filter)
{
    const std::string path = relative.generic_string();
    for (const auto& prefix : filter.excludePrefixes)
        if (path.rfind(prefix, 0) == 0)
            return false;

    if (!filter.includePrefixes.empty()) {
        bool included = false;
        for (const auto& prefix : filter.includePrefixes)
            if (path.rfind(prefix, 0) == 0)
                included = true;
        if (!included)
            return false;
    }

    if (!filter.includeExtensions.empty()) {
        const std::string ext = relative.extension().string();
        bool included = false;
        for (const auto& wanted : filter.includeExtensions)
            if (ext == wanted)
                included = true;
        if (!included)
            return false;
    }
    return true;
}

std::string dirOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

} // namespace

const ProjectTreeEntry* ProjectTree::find(const std::string& path) const
{
    for (const auto& entry : entries)
        if (entry.path == path)
            return &entry;
    return nullptr;
}

std::string ProjectTreeScanner::canonical(const std::vector<ProjectTreeEntry>& entries)
{
    std::string canonical;
    canonical.reserve(entries.size() * 96);
    for (const auto& entry : entries) {
        canonical += entry.path;
        canonical += '\0';
        canonical += entry.content.algorithm;
        canonical += ':';
        canonical += entry.content.digest;
        canonical += '\n';
    }
    return canonical;
}

std::string ProjectTreeScanner::hashEntries(const std::vector<ProjectTreeEntry>& entries)
{
    const std::string canonical = ProjectTreeScanner::canonical(entries);
    return LiveCodeHash::sha256Bytes(canonical.data(), canonical.size());
}

bool ProjectTreeScanner::scan(const std::filesystem::path& root, const TreeFilter& filter,
                              BlobStore* store, ProjectTree& out)
{
    out.entries.clear();
    std::error_code error;
    if (!std::filesystem::exists(root, error))
        return false;

    for (const auto& dirEntry :
         std::filesystem::recursive_directory_iterator(root, error)) {
        if (error)
            break;
        if (!dirEntry.is_regular_file())
            continue;
        const std::filesystem::path relative =
            std::filesystem::relative(dirEntry.path(), root, error);
        if (error)
            continue;
        if (!matchesFilter(relative, filter))
            continue;

        ContentId id;
        if (store) {
            if (!store->putFile(dirEntry.path(), id))
                continue;
        } else {
            id = ContentId::fromFile(dirEntry.path().string());
        }

        ProjectTreeEntry entry;
        entry.path = relative.generic_string();
        entry.content = id;
        entry.size = (std::uint64_t)dirEntry.file_size(error);
        out.entries.push_back(std::move(entry));
    }

    std::sort(out.entries.begin(), out.entries.end(),
              [](const ProjectTreeEntry& a, const ProjectTreeEntry& b) {
                  return a.path < b.path;
              });
    out.treeHash = hashEntries(out.entries);

    // Store the tree manifest itself so any version can be checked out later.
    if (store) {
        const std::string manifest = canonical(out.entries);
        ContentId manifestId;
        manifestId.algorithm = "sha256";
        manifestId.digest = out.treeHash;
        store->putBytes(manifestId, manifest.data(), manifest.size());
    }
    return true;
}

bool ProjectTreeScanner::checkout(const std::filesystem::path& root, const TreeFilter& filter,
                                  BlobStore& store, const std::string& treeHash)
{
    ContentId manifestId;
    manifestId.algorithm = "sha256";
    manifestId.digest = treeHash;
    std::string manifest;
    if (!store.read(manifestId, manifest))
        return false;

    std::vector<std::string> wanted;
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
        const std::string path = line.substr(0, nul);
        const std::string contentStr = line.substr(nul + 1);
        const std::size_t colon = contentStr.find(':');
        if (colon == std::string::npos)
            continue;
        ContentId id;
        id.algorithm = contentStr.substr(0, colon);
        id.digest = contentStr.substr(colon + 1);

        const std::filesystem::path target = root / path;
        std::error_code error;
        std::filesystem::create_directories(target.parent_path(), error);
        std::string bytes;
        if (store.read(id, bytes)) {
            std::ofstream outFile(target, std::ios::binary | std::ios::trunc);
            if (outFile.is_open())
                outFile.write(bytes.data(), (std::streamsize)bytes.size());
        }
        wanted.push_back(path);
    }

    // Remove filtered files that are not part of the target tree. The blobs
    // remain in the store, so this is a source-tree checkout, not data loss.
    std::error_code error;
    std::vector<std::filesystem::path> toRemove;
    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(root, error)) {
        if (error || !dirEntry.is_regular_file())
            continue;
        const std::filesystem::path relative =
            std::filesystem::relative(dirEntry.path(), root, error);
        if (error || !matchesFilter(relative, filter))
            continue;
        const std::string path = relative.generic_string();
        bool present = false;
        for (const auto& w : wanted)
            if (w == path)
                present = true;
        if (!present)
            toRemove.push_back(dirEntry.path());
    }
    for (const auto& path : toRemove)
        std::filesystem::remove(path, error);
    return true;
}

void ProjectTreeScanner::diff(const ProjectTree& before, const ProjectTree& after,
                              ChangeSet& out)
{
    out.changes.clear();
    out.parentTreeHash = before.treeHash;
    out.newTreeHash = after.treeHash;

    std::unordered_map<std::string, const ProjectTreeEntry*> beforeByPath;
    std::unordered_map<std::string, const ProjectTreeEntry*> afterByPath;
    for (const auto& entry : before.entries)
        beforeByPath[entry.path] = &entry;
    for (const auto& entry : after.entries)
        afterByPath[entry.path] = &entry;

    std::vector<const ProjectTreeEntry*> deleted;
    std::vector<const ProjectTreeEntry*> added;

    for (const auto& entry : before.entries) {
        auto it = afterByPath.find(entry.path);
        if (it == afterByPath.end()) {
            deleted.push_back(&entry);
        } else if (it->second->content.str() != entry.content.str()) {
            ChangeEntry change;
            change.op = ChangeOp::Modify;
            change.path = entry.path;
            change.before = entry.content;
            change.after = it->second->content;
            out.changes.push_back(std::move(change));
        }
    }
    for (const auto& entry : after.entries)
        if (beforeByPath.find(entry.path) == beforeByPath.end())
            added.push_back(&entry);

    // Rename/move detection: pair deletions and additions with identical
    // content. Same basename = rename, otherwise a move.
    std::vector<bool> addUsed(added.size(), false);
    std::vector<bool> delUsed(deleted.size(), false);
    for (std::size_t d = 0; d < deleted.size(); ++d) {
        int match = -1;
        for (std::size_t i = 0; i < added.size(); ++i) {
            if (addUsed[i])
                continue;
            if (added[i]->content.str() == deleted[d]->content.str()) {
                match = (int)i;
                break;
            }
        }
        if (match < 0)
            continue;
        addUsed[(std::size_t)match] = true;
        delUsed[d] = true;
        ChangeEntry change;
        // Same directory means the file was renamed; a different directory
        // means it was moved.
        change.op = (dirOf(deleted[d]->path) == dirOf(added[(std::size_t)match]->path))
                        ? ChangeOp::Rename
                        : ChangeOp::Move;
        change.path = added[(std::size_t)match]->path;
        change.oldPath = deleted[d]->path;
        change.before = deleted[d]->content;
        change.after = added[(std::size_t)match]->content;
        out.changes.push_back(std::move(change));
    }

    for (std::size_t d = 0; d < deleted.size(); ++d) {
        if (delUsed[d])
            continue;
        ChangeEntry change;
        change.op = ChangeOp::Delete;
        change.path = deleted[d]->path;
        change.before = deleted[d]->content;
        out.changes.push_back(std::move(change));
    }
    for (std::size_t i = 0; i < added.size(); ++i) {
        if (addUsed[i])
            continue;
        ChangeEntry change;
        change.op = ChangeOp::Add;
        change.path = added[i]->path;
        change.after = added[i]->content;
        out.changes.push_back(std::move(change));
    }

    std::sort(out.changes.begin(), out.changes.end(),
              [](const ChangeEntry& a, const ChangeEntry& b) { return a.path < b.path; });
}

} // namespace Project
