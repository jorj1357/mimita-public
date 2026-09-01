// 09 01 2026, 12 00
/* purpose
* Declares the shared SHA-256 helper used by avatar upload and cache validation.
* Keeps the wire identity a content hash instead of a local filename or path.
* Lets client and server verify the same bytes before accepting or using assets.
* Does NOT own packet transport, avatar rendering, filesystem cache policy, or gameplay.
* Does NOT serialize avatar definitions or decide which cosmetic models are bundled.
* Does NOT expose private player data or trust an unverified asset payload.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace MimitaNet {

std::string avatarSha256Hex(const uint8_t* bytes, size_t size);
bool avatarSha256HexToBytes(const std::string& hex, uint8_t out[32]);

} // namespace MimitaNet
