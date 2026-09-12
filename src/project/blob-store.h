// 09 12 2026
/* purpose
* Content-addressed blob store under .mimita/store/<algorithm>/<digest>.
* Immutable writes: a blob is written once and reused by hash; deletion of a
* project path never deletes the blob, so history stays recoverable.
* Does NOT own project trees, history, or builds.
*/
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "project/project-types.h"

namespace Project {

class BlobStore {
public:
    explicit BlobStore(std::filesystem::path root);

    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path pathFor(const ContentId& id) const;

    bool has(const ContentId& id) const;
    bool putBytes(const ContentId& id, const void* data, std::size_t size);
    // Computes the id, stores the bytes if new, and returns the id.
    bool putFile(const std::filesystem::path& source, ContentId& outId);
    bool read(const ContentId& id, std::string& out) const;
    std::uint64_t blobCount() const;

private:
    std::filesystem::path root_;
};

} // namespace Project
