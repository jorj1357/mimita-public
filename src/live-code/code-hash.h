// 09 12 2026
/* purpose
* Compute SHA-256 content hashes for hot-module source files and byte buffers.
* Owns the hashing primitive used by live-code change detection and generation
* identity.
* Does NOT decide when to rebuild, load, or activate a module.
*/
#pragma once

#include <cstddef>
#include <string>

namespace LiveCodeHash {

// Lowercase hex SHA-256 of a file's bytes. Empty string if the file is missing.
std::string sha256File(const std::string& path);

// Lowercase hex SHA-256 of a byte buffer. Empty if hashing fails.
std::string sha256Bytes(const void* data, std::size_t size);

} // namespace LiveCodeHash
