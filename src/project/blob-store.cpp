// 09 12 2026
/* purpose
* Implements the content-addressed blob store.
* Does NOT own project trees, history, or builds.
*/
#include "project/blob-store.h"

#include <fstream>
#include <system_error>

namespace Project {

BlobStore::BlobStore(std::filesystem::path root)
    : root_(std::move(root))
{
}

std::filesystem::path BlobStore::pathFor(const ContentId& id) const
{
    return root_ / id.algorithm / id.digest;
}

bool BlobStore::has(const ContentId& id) const
{
    if (!id.valid())
        return false;
    std::error_code error;
    return std::filesystem::is_regular_file(pathFor(id), error);
}

bool BlobStore::putBytes(const ContentId& id, const void* data, std::size_t size)
{
    if (!id.valid())
        return false;
    if (has(id))
        return true;

    const std::filesystem::path target = pathFor(id);
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);

    const std::filesystem::path staging = target.string() + ".staging";
    {
        std::ofstream out(staging, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return false;
        out.write(reinterpret_cast<const char*>(data), (std::streamsize)size);
        out.flush();
        if (!out.good())
            return false;
    }
    std::filesystem::rename(staging, target, error);
    if (error) {
        std::filesystem::remove(target, error);
        std::filesystem::rename(staging, target, error);
    }
    return !error;
}

bool BlobStore::putFile(const std::filesystem::path& source, ContentId& outId)
{
    outId = ContentId::fromFile(source.string());
    if (!outId.valid())
        return false;
    if (has(outId))
        return true;

    std::ifstream in(source, std::ios::binary);
    if (!in.is_open())
        return false;
    std::string bytes((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    return putBytes(outId, bytes.data(), bytes.size());
}

bool BlobStore::read(const ContentId& id, std::string& out) const
{
    if (!id.valid())
        return false;
    std::ifstream in(pathFor(id), std::ios::binary);
    if (!in.is_open())
        return false;
    out.assign((std::istreambuf_iterator<char>(in)),
               std::istreambuf_iterator<char>());
    return true;
}

std::uint64_t BlobStore::blobCount() const
{
    std::error_code error;
    if (!std::filesystem::exists(root_, error))
        return 0;
    std::uint64_t count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_, error))
        if (entry.is_regular_file())
            ++count;
    return count;
}

} // namespace Project
