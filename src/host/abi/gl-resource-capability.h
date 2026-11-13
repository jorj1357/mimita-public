// 2026-11-13
// Resident GL object lifetime capability.
//
// The host creates/deletes GL objects; game code may bind/upload/draw with the
// returned ids. This is a migration seam until the full CAP_RENDER command
// stream owns all draw state and resources.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kGlResourceCapabilityVersion = 1u;

struct GlResourceCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint32_t (*gen_texture)(void*);
    void (*delete_texture)(void*, std::uint32_t);
    std::uint32_t (*gen_buffer)(void*);
    void (*delete_buffer)(void*, std::uint32_t);
    std::uint32_t (*gen_vertex_array)(void*);
    void (*delete_vertex_array)(void*, std::uint32_t);
    std::uint32_t (*gen_framebuffer)(void*);
    void (*delete_framebuffer)(void*, std::uint32_t);
    std::uint32_t (*gen_renderbuffer)(void*);
    void (*delete_renderbuffer)(void*, std::uint32_t);
    std::uint32_t (*gen_query)(void*);
    void (*delete_query)(void*, std::uint32_t);
    void (*delete_shader)(void*, std::uint32_t);
    void (*delete_program)(void*, std::uint32_t);
    void (*generate_mipmap)(void*, std::uint32_t target);
};

}  // namespace mimita::host
