// 2026-11-13
// Game-DLL side of resident GL object lifetime.

#include "game/gl-resource-bridge-module.h"

#include "host/abi/gl-resource-capability.h"

namespace mimita::game {
namespace {
const mimita::host::GlResourceCapabilityV1* gGl = nullptr;
void* gHostData = nullptr;
#define GL_CAP(call) (gGl && gGl->call ? gGl->call(gHostData) : 0)
}  // namespace

void initializeHostGlResources(const mimita::host::HostServicesV1* services) {
    gGl = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::GlResourceCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::GlResource),
            mimita::host::kGlResourceCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kGlResourceCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::GlResourceCapabilityV1)) {
        gGl = capability;
    }
}

std::uint32_t genTexture() { return GL_CAP(gen_texture); }
void deleteTexture(std::uint32_t id) { if (gGl && gGl->delete_texture) gGl->delete_texture(gHostData, id); }
std::uint32_t genBuffer() { return GL_CAP(gen_buffer); }
void deleteBuffer(std::uint32_t id) { if (gGl && gGl->delete_buffer) gGl->delete_buffer(gHostData, id); }
std::uint32_t genVertexArray() { return GL_CAP(gen_vertex_array); }
void deleteVertexArray(std::uint32_t id) { if (gGl && gGl->delete_vertex_array) gGl->delete_vertex_array(gHostData, id); }
std::uint32_t genFramebuffer() { return GL_CAP(gen_framebuffer); }
void deleteFramebuffer(std::uint32_t id) { if (gGl && gGl->delete_framebuffer) gGl->delete_framebuffer(gHostData, id); }
std::uint32_t genRenderbuffer() { return GL_CAP(gen_renderbuffer); }
void deleteRenderbuffer(std::uint32_t id) { if (gGl && gGl->delete_renderbuffer) gGl->delete_renderbuffer(gHostData, id); }
std::uint32_t genQuery() { return GL_CAP(gen_query); }
void deleteQuery(std::uint32_t id) { if (gGl && gGl->delete_query) gGl->delete_query(gHostData, id); }
void deleteShader(std::uint32_t id) { if (gGl && gGl->delete_shader) gGl->delete_shader(gHostData, id); }
void deleteProgram(std::uint32_t id) { if (gGl && gGl->delete_program) gGl->delete_program(gHostData, id); }
void generateMipmap(std::uint32_t target) { if (gGl && gGl->generate_mipmap) gGl->generate_mipmap(gHostData, target); }
#undef GL_CAP
}  // namespace mimita::game
