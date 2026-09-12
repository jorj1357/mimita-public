// 09 12 2026
/* purpose
* Implements SHA-256 hashing with BCrypt for live-code change detection.
* Owns the digest conversion and file read used to fingerprint hot sources.
* Does NOT start builds or touch the hot-reload state machine.
*/
#include "live-code/code-hash.h"

#include <cstdio>
#include <vector>

#include <windows.h>
#include <bcrypt.h>

namespace {

const char* hexDigits = "0123456789abcdef";

std::string toHex(const unsigned char* bytes, std::size_t size)
{
    std::string out;
    out.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2] = hexDigits[(bytes[i] >> 4) & 0x0f];
        out[i * 2 + 1] = hexDigits[bytes[i] & 0x0f];
    }
    return out;
}

bool digest(const void* data, std::size_t size, unsigned char out[32])
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        if (BCryptHashData(hash,
                           reinterpret_cast<PUCHAR>(const_cast<void*>(data)),
                           static_cast<ULONG>(size), 0) >= 0 &&
            BCryptFinishHash(hash, out, 32, 0) >= 0) {
            ok = true;
        }
    }

    if (hash)
        BCryptDestroyHash(hash);
    if (algorithm)
        BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

} // namespace

namespace LiveCodeHash {

std::string sha256Bytes(const void* data, std::size_t size)
{
    if (!data && size != 0)
        return {};
    unsigned char out[32]{};
    if (!digest(data, size, out))
        return {};
    return toHex(out, sizeof(out));
}

std::string sha256File(const std::string& path)
{
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return {};

    std::vector<unsigned char> buffer;
    unsigned char chunk[65536];
    std::size_t read = 0;
    while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
        buffer.insert(buffer.end(), chunk, chunk + read);
    std::fclose(file);

    if (buffer.empty())
        return sha256Bytes("", 0);
    return sha256Bytes(buffer.data(), buffer.size());
}

} // namespace LiveCodeHash
