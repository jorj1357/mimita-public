// 09 12 2026
/* purpose
* Implements content identity helpers for the project layer.
* Does NOT own storage, scanning, or history.
*/
#include "project/project-types.h"

#include "live-code/code-hash.h"

#include <cstddef>

namespace Project {

ContentId ContentId::fromBytes(const void* data, std::size_t size)
{
    ContentId id;
    id.algorithm = "sha256";
    id.digest = LiveCodeHash::sha256Bytes(data, size);
    return id;
}

ContentId ContentId::fromString(const std::string& value)
{
    return fromBytes(value.data(), value.size());
}

ContentId ContentId::fromFile(const std::string& path)
{
    ContentId id;
    id.algorithm = "sha256";
    id.digest = LiveCodeHash::sha256File(path);
    return id;
}

} // namespace Project
