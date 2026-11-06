// 2026-11-05
#include "worldproject/world-project-sha256.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace MimitaWorldProject {
namespace {

constexpr uint32_t kRound[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

uint32_t rotr(uint32_t value, uint32_t count)
{
    return (value >> count) | (value << (32u - count));
}

void transform(std::array<uint32_t, 8>& state, const uint8_t* block)
{
    uint32_t w[64] = {};
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4]) << 24) |
               (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) |
               uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + s1 + ch + kRound[i] + w[i];
        const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

std::string bytesToHex(const uint8_t* bytes, size_t count)
{
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.resize(count * 2);
    for (size_t i = 0; i < count; ++i) {
        out[i * 2] = digits[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = digits[bytes[i] & 0xF];
    }
    return out;
}

} // namespace

std::string sha256Hex(const void* data, size_t bytes)
{
    std::array<uint32_t, 8> state = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    const uint8_t* input = static_cast<const uint8_t*>(data);
    size_t offset = 0;
    while (offset + 64 <= bytes) {
        transform(state, input + offset);
        offset += 64;
    }

    uint8_t tail[128] = {};
    const size_t remaining = bytes - offset;
    if (remaining > 0)
        std::memcpy(tail, input + offset, remaining);
    tail[remaining] = 0x80;
    const size_t tailBlocks = (remaining + 1 + 8 + 63) / 64;
    const uint64_t bitLength = uint64_t(bytes) * 8u;
    for (int i = 0; i < 8; ++i)
        tail[tailBlocks * 64 - 1 - i] = uint8_t(bitLength >> (i * 8));
    for (size_t block = 0; block < tailBlocks; ++block)
        transform(state, tail + block * 64);

    uint8_t digest[32] = {};
    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = uint8_t(state[i] >> 24);
        digest[i * 4 + 1] = uint8_t(state[i] >> 16);
        digest[i * 4 + 2] = uint8_t(state[i] >> 8);
        digest[i * 4 + 3] = uint8_t(state[i]);
    }
    return bytesToHex(digest, sizeof(digest));
}

std::string sha256Hex(const std::string& bytes)
{
    return sha256Hex(bytes.data(), bytes.size());
}

std::string sha256File(const std::string& path, std::string* error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (error) *error = "cannot open file";
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!file.good() && !file.eof()) {
        if (error) *error = "cannot read file";
        return {};
    }
    return sha256Hex(buffer.str());
}

bool sha256SelfTest(std::string& report)
{
    report.clear();
    bool ok = true;
    auto check = [&](const std::string& input, const char* expected, const char* name) {
        const std::string actual = sha256Hex(input);
        if (actual != expected) {
            report += std::string("FAIL ") + name + " expected=" + expected +
                      " actual=" + actual + "\n";
            ok = false;
        } else {
            report += std::string("PASS ") + name + "\n";
        }
    };
    check("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty");
    check("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
    check("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
          "nist-two-block");
    return ok;
}

} // namespace MimitaWorldProject
