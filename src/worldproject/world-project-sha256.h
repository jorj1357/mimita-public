// 2026-11-05
/* purpose
* Provide one portable SHA-256 implementation for world-project content
* identity and integrity checks.
* This file DOES NOT own project storage, networking, or signing.
*/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MimitaWorldProject {

std::string sha256Hex(const void* data, size_t bytes);
std::string sha256Hex(const std::string& bytes);
std::string sha256File(const std::string& path, std::string* error = nullptr);

// Focused NIST test-vector check for the shared hash owner.
bool sha256SelfTest(std::string& report);

} // namespace MimitaWorldProject
