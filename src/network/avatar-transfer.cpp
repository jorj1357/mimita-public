// 09 01 2026, 12 00
/* purpose
* Implements SHA-256 identity and strict hexadecimal conversion for avatar assets.
* Uses Windows BCrypt so hashes are real SHA-256 and require no third-party runtime.
* Provides one content identity for sender, server storage, and receiver cache lookup.
* Does NOT send packets, write files, parse avatar JSON, or apply textures to Players.
* Does NOT accept paths as identity; paths remain local to each installation.
* Does NOT perform compression because PNG/JPEG data is already compressed.
*/

#include "network/avatar-transfer.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <vector>

namespace MimitaNet {

std::string avatarSha256Hex(const uint8_t* bytes, size_t size)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::string result;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) != 0)
        return result;

    DWORD objectBytes = 0;
    DWORD dataBytes = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          reinterpret_cast<PUCHAR>(&objectBytes),
                          sizeof(objectBytes), &dataBytes, 0) != 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return result;
    }

    std::vector<uint8_t> object(objectBytes);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectBytes,
                         nullptr, 0, 0) != 0 ||
        BCryptHashData(hash, const_cast<PUCHAR>(bytes),
                       static_cast<ULONG>(size), 0) != 0)
    {
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return result;
    }

    uint8_t digest[32] = {};
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0)
    {
        static constexpr char hex[] = "0123456789abcdef";
        result.reserve(64);
        for (uint8_t byte : digest)
        {
            result.push_back(hex[(byte >> 4) & 0xf]);
            result.push_back(hex[byte & 0xf]);
        }
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return result;
}

bool avatarSha256HexToBytes(const std::string& hex, uint8_t out[32])
{
    if (hex.size() != 64 || !out)
        return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (int i = 0; i < 32; ++i)
    {
        const int high = nibble(hex[i * 2]);
        const int low = nibble(hex[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        out[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

} // namespace MimitaNet
