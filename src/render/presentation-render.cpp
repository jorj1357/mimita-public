// 09 14 2026
/* purpose
* Implements the cold generic mesh presentation path: resource loaders for a
* procedural mesh and a texture, plus submitMesh which resolves generation-aware
* handles and draws with the basic shader. Hot presentation owns what to draw;
* this file only executes generic GPU work.
* Does NOT own scene traversal or gameplay policy.
*/
#include "render/presentation-render.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "camera.h"
#include "renderer/renderer.h"
#include "map/map_common.h"
#include "map/map_loader.h"
#include "project/presentation-resource.h"
#include "utils/path_utils.h"
#include "stb_image.h"

extern Renderer* gRenderer;
extern Camera* gpCamera;

namespace PresentationRender {
namespace {

const std::uint64_t kCubeMesh = gameHash("mesh.cube");
const std::uint64_t kRocketMesh = gameHash("mesh.rocket");
const std::uint64_t kGrenadeMesh = gameHash("mesh.grenade");
const std::uint64_t kGlbMesh = gameHash("mesh.demo.glb");
const std::uint64_t kActorMesh = gameHash("mesh.actor");
const char* const kGlbMeshPath =
    "assets/entity/player/default/mimita-char-no-animations-v4.glb";
const std::uint64_t kDefaultTexture = gameHash("texture.default");
const std::uint64_t kRocketTexture = gameHash("texture.rocket");
const std::uint64_t kGrenadeTexture = gameHash("texture.grenade");
const char* const kDefaultTexturePath = "assets/textures/circuitryv1.png";
const char* const kRocketTexturePath = "assets/textureshq/colorful2.png";
const char* const kGrenadeTexturePath = "assets/textureshq/meat1.png";

struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
};

struct GpuVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    glm::vec3 normal;
};

std::uint64_t g_submitted = 0;
bool g_initialized = false;

std::uint64_t fnv1a(std::uint64_t hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint64_t>(bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::vector<std::uint8_t> readFileBytes(const char* path, bool* ok)
{
    *ok = false;
    const std::string resolved = resolveAssetPath(path);
    FILE* f = fopen(resolved.c_str(), "rb");
    if (!f)
        return {};
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return {};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    const std::size_t read = fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    if (read != bytes.size())
        return {};
    *ok = true;
    return bytes;
}

bool loadCubeMesh(void* /*user*/, void** outHandle)
{
    if (!gRenderer)
        return false;
    const float h = 0.5f;
    const GpuVertex verts[] = {
        {{-h, -h, -h}, {0, 0}, {0, 0, -1}}, {{h, -h, -h}, {1, 0}, {0, 0, -1}},
        {{h, h, -h}, {1, 1}, {0, 0, -1}},   {{-h, h, -h}, {0, 1}, {0, 0, -1}},
        {{-h, -h, h}, {0, 0}, {0, 0, 1}},   {{h, -h, h}, {1, 0}, {0, 0, 1}},
        {{h, h, h}, {1, 1}, {0, 0, 1}},     {{-h, h, h}, {0, 1}, {0, 0, 1}},
        {{-h, -h, -h}, {0, 0}, {0, -1, 0}}, {{h, -h, -h}, {1, 0}, {0, -1, 0}},
        {{h, -h, h}, {1, 1}, {0, -1, 0}},   {{-h, -h, h}, {0, 1}, {0, -1, 0}},
        {{-h, h, -h}, {0, 0}, {0, 1, 0}},   {{h, h, -h}, {1, 0}, {0, 1, 0}},
        {{h, h, h}, {1, 1}, {0, 1, 0}},     {{-h, h, h}, {0, 1}, {0, 1, 0}},
        {{-h, -h, -h}, {0, 0}, {-1, 0, 0}}, {{-h, -h, h}, {1, 0}, {-1, 0, 0}},
        {{-h, h, h}, {1, 1}, {-1, 0, 0}},   {{-h, h, -h}, {0, 1}, {-1, 0, 0}},
        {{h, -h, -h}, {0, 0}, {1, 0, 0}},   {{h, -h, h}, {1, 0}, {1, 0, 0}},
        {{h, h, h}, {1, 1}, {1, 0, 0}},     {{h, h, -h}, {0, 1}, {1, 0, 0}},
    };
    const std::uint32_t indices[] = {
        0, 1, 2, 0, 2, 3,       4, 6, 5, 4, 7, 6,
        8, 9, 10, 8, 10, 11,    12, 14, 13, 12, 15, 14,
        16, 17, 18, 16, 18, 19, 20, 22, 21, 20, 23, 22,
    };

    auto* mesh = new GpuMesh();
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, normal));
    glBindVertexArray(0);
    mesh->indexCount = sizeof(indices) / sizeof(indices[0]);
    *outHandle = mesh;
    return true;
}

void retireCubeMesh(void* /*user*/, void* handle)
{
    auto* mesh = static_cast<GpuMesh*>(handle);
    if (!mesh)
        return;
    if (gRenderer) {
        if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
        if (mesh->vbo) glDeleteBuffers(1, &mesh->vbo);
        if (mesh->vao) glDeleteVertexArrays(1, &mesh->vao);
    }
    delete mesh;
}

GpuMesh* uploadMesh(const std::vector<GpuVertex>& verts,
                    const std::vector<std::uint32_t>& indices)
{
    auto* mesh = new GpuMesh();
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(GpuVertex)), verts.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 (GLsizeiptr)(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, uv));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex),
                          (void*)offsetof(GpuVertex, normal));
    glBindVertexArray(0);
    mesh->indexCount = (GLsizei)indices.size();
    return mesh;
}

// Cylinder along local +Z (the projectile forward axis).
bool loadRocketMesh(void* /*user*/, void** outHandle)
{
    if (!gRenderer)
        return false;
    const float length = 1.5f;
    const float radius = 0.18f;
    const int seg = 16;
    const float h = length * 0.5f;
    std::vector<GpuVertex> verts;
    std::vector<std::uint32_t> indices;
    for (int i = 0; i <= seg; ++i) {
        const float a = (float)i / (float)seg * 6.2831853f;
        const float cx = std::cos(a) * radius;
        const float cy = std::sin(a) * radius;
        const glm::vec3 n = glm::normalize(glm::vec3(cx, cy, 0.0f));
        verts.push_back({{cx, cy, -h}, {(float)i / seg, 0.0f}, n});
        verts.push_back({{cx, cy, h}, {(float)i / seg, 1.0f}, n});
    }
    for (int i = 0; i < seg; ++i) {
        const std::uint32_t a = i * 2, b = i * 2 + 1, c = i * 2 + 2, d = i * 2 + 3;
        indices.insert(indices.end(), {a, c, b, b, c, d});
    }
    *outHandle = uploadMesh(verts, indices);
    return true;
}

bool loadGrenadeMesh(void* /*user*/, void** outHandle)
{
    if (!gRenderer)
        return false;
    const float radius = 0.28f;
    const int seg = 12, rings = 16;
    std::vector<GpuVertex> verts;
    std::vector<std::uint32_t> indices;
    for (int y = 0; y <= rings; ++y) {
        const float v = (float)y / (float)rings;
        const float phi = v * 3.14159265f;
        for (int x = 0; x <= seg; ++x) {
            const float u = (float)x / (float)seg;
            const float theta = u * 6.2831853f;
            const glm::vec3 n(std::sin(phi) * std::cos(theta),
                              std::sin(phi) * std::sin(theta), std::cos(phi));
            verts.push_back({{n.x * radius, n.y * radius, n.z * radius}, {u, v}, n});
        }
    }
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < seg; ++x) {
            const std::uint32_t a = y * (seg + 1) + x;
            const std::uint32_t b = a + seg + 1;
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    *outHandle = uploadMesh(verts, indices);
    return true;
}

// Real GLB loader: reuses the existing map loader's tinygltf parse, then uploads
// vertices/indices through the same GpuMesh path. One more loader type for the
// generation-aware provider; no separate GLB subsystem.
bool loadGlbMesh(void* user, void** outHandle)
{
    if (!gRenderer)
        return false;
    const char* path = user ? static_cast<const char*>(user) : kGlbMeshPath;
    if (!path || !*path)
        return false;
    // Validate the GLB container before parsing so a malformed replacement is
    // rejected and the last-good generation stays active.
    {
        bool readOk = false;
        const std::vector<std::uint8_t> bytes = readFileBytes(path, &readOk);
        if (!readOk || bytes.size() < 12)
            return false;
        std::uint32_t magic = 0, version = 0;
        std::memcpy(&magic, bytes.data(), 4);
        std::memcpy(&version, bytes.data() + 4, 4);
        if (magic != 0x46546C67u || version != 2u)
            return false;
    }
    Mesh mesh = loadGLB(path, false);
    if (mesh.verts.empty())
        return false;
    std::vector<GpuVertex> verts;
    verts.reserve(mesh.verts.size());
    for (const ::Vertex& v : mesh.verts) {
        GpuVertex out;
        out.pos = v.pos;
        out.uv = v.uv;
        out.normal = v.normal;
        verts.push_back(out);
    }
    std::vector<std::uint32_t> indices(verts.size());
    for (std::size_t i = 0; i < indices.size(); ++i)
        indices[i] = static_cast<std::uint32_t>(i);
    *outHandle = uploadMesh(verts, indices);
    return true;
}

bool loadDefaultTexture(void* user, void** outHandle)
{
    if (!gRenderer)
        return false;
    const char* path = user ? static_cast<const char*>(user) : kDefaultTexturePath;
    bool ok = false;
    const std::vector<std::uint8_t> bytes = readFileBytes(path, &ok);
    if (!ok)
        return false;
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h,
                                            &channels, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return false;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    *outHandle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(tex));
    return true;
}

void retireDefaultTexture(void* /*user*/, void* handle)
{
    const GLuint tex = static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(handle));
    if (tex && gRenderer)
        glDeleteTextures(1, &tex);
}

std::uint64_t fileContentHash(const char* path)
{
    bool ok = false;
    const std::vector<std::uint8_t> bytes = readFileBytes(path, &ok);
    if (!ok)
        return 0;
    return fnv1a(1469598103934665603ull, bytes.data(), bytes.size());
}

} // namespace

void init()
{
    if (g_initialized)
        return;
    g_initialized = true;
    MimitaRuntime::PresentationResourceProvider& provider =
        MimitaRuntime::PresentationResourceProvider::instance();
    provider.setLoader(kCubeMesh, &loadCubeMesh, &retireCubeMesh, nullptr);
    provider.setLoader(kRocketMesh, &loadRocketMesh, &retireCubeMesh, nullptr);
    provider.setLoader(kGrenadeMesh, &loadGrenadeMesh, &retireCubeMesh, nullptr);
    provider.setLoader(kGlbMesh, &loadGlbMesh, &retireCubeMesh,
                       const_cast<char*>(kGlbMeshPath));
    // Actor mesh: the same GLB loader, a distinct logical id used by generic
    // actor presentation. No NPC-specific asset cache.
    provider.setLoader(kActorMesh, &loadGlbMesh, &retireCubeMesh,
                       const_cast<char*>(kGlbMeshPath));
    provider.setLoader(kDefaultTexture, &loadDefaultTexture, &retireDefaultTexture,
                       const_cast<char*>(kDefaultTexturePath));
    provider.setLoader(kRocketTexture, &loadDefaultTexture, &retireDefaultTexture,
                       const_cast<char*>(kRocketTexturePath));
    provider.setLoader(kGrenadeTexture, &loadDefaultTexture, &retireDefaultTexture,
                       const_cast<char*>(kGrenadeTexturePath));
    provider.apply(kCubeMesh, gameHash("mesh.cube.v1"));
    provider.apply(kRocketMesh, gameHash("mesh.rocket.v1"));
    provider.apply(kGrenadeMesh, gameHash("mesh.grenade.v1"));
    provider.apply(kGlbMesh, fileContentHash(kGlbMeshPath));
    provider.apply(kActorMesh, fileContentHash(kGlbMeshPath));
    provider.apply(kDefaultTexture, fileContentHash(kDefaultTexturePath));
    provider.apply(kRocketTexture, fileContentHash(kRocketTexturePath));
    provider.apply(kGrenadeTexture, fileContentHash(kGrenadeTexturePath));
}

void poll()
{
    MimitaRuntime::PresentationResourceProvider& provider =
        MimitaRuntime::PresentationResourceProvider::instance();
    const std::uint64_t rocket = fileContentHash(kRocketTexturePath);
    if (rocket != 0)
        provider.apply(kRocketTexture, rocket);
    const std::uint64_t grenade = fileContentHash(kGrenadeTexturePath);
    if (grenade != 0)
        provider.apply(kGrenadeTexture, grenade);
    const std::uint64_t def = fileContentHash(kDefaultTexturePath);
    if (def != 0)
        provider.apply(kDefaultTexture, def);
}

void submitMesh(const GameRenderMeshCommandV1& command)
{
    ++g_submitted;
    if (!gRenderer || !gRenderer->shaderProgram || !gpCamera)
        return;
    auto* mesh = static_cast<GpuMesh*>(
        MimitaRuntime::PresentationResourceProvider::instance().handleOf(
            command.meshResourceId));
    if (!mesh)
        return;
    const GLuint texture = static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(
        MimitaRuntime::PresentationResourceProvider::instance().handleOf(
            command.textureResourceId)));

    const Camera& camera = *gpCamera;
    const glm::vec3 position(command.position[0], command.position[1],
                             command.position[2]);
    const glm::quat rotation(command.rotation[3], command.rotation[0],
                             command.rotation[1], command.rotation[2]);
    const glm::vec3 scale(command.scale[0], command.scale[1], command.scale[2]);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position) *
                      glm::mat4_cast(rotation) *
                      glm::scale(glm::mat4(1.0f), scale);
    const glm::mat4 view = camera.getView();
    const glm::mat4 proj =
        camera.getProj((float)gRenderer->width, (float)gRenderer->height);

    const GLuint shader = gRenderer->shaderProgram;
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE,
                       glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(shader, "uOutlinePass"), 0);
    if (texture != 0) {
        glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
    } else {
        glUniform1i(glGetUniformLocation(shader, "uUseColor"), 1);
        glUniform4fv(glGetUniformLocation(shader, "uColor"),
                     1, command.color);
    }
    glBindVertexArray(mesh->vao);
    glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    if (texture != 0)
        glBindTexture(GL_TEXTURE_2D, 0);
}

std::uint64_t submittedMeshCount()
{
    return g_submitted;
}

bool validateGlbFile(const char* path, std::string& error)
{
    bool ok = false;
    const std::vector<std::uint8_t> bytes = readFileBytes(path, &ok);
    if (!ok || bytes.size() < 12) {
        error = "read failed or too short";
        return false;
    }
    std::uint32_t magic = 0, version = 0, length = 0;
    std::memcpy(&magic, bytes.data(), 4);
    std::memcpy(&version, bytes.data() + 4, 4);
    std::memcpy(&length, bytes.data() + 8, 4);
    if (magic != 0x46546C67u) { error = "bad magic"; return false; }
    if (version != 2u) { error = "unsupported version"; return false; }
    if (length < 12u || length > bytes.size()) { error = "bad length"; return false; }
    return true;
}

} // namespace PresentationRender
