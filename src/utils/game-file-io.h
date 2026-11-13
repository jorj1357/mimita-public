// 2026-11-13
// Per-call RAII file I/O for game code.
//
// These helpers keep no file handle beyond the call, so they do not need a
// resident handle lease. Long-lived streaming handles still belong in a host
// resource service.

#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace mimita::game {

inline std::vector<std::uint8_t> readFileBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

inline std::string readFileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

inline bool writeFileBytes(const std::string& path, const void* data,
                           std::size_t size) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (size) out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return out.good();
}

inline bool writeFileText(const std::string& path, const std::string& text) {
    return writeFileBytes(path, text.data(), text.size());
}

inline bool appendFileText(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) return false;
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good();
}

inline std::uint64_t fileSize(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return 0;
    return static_cast<std::uint64_t>(in.tellg());
}

}  // namespace mimita::game
