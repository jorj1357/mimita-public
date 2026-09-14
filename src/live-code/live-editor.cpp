// 09 13 2026
/* purpose
* Implements the EXE-side editor module bridge and capabilities.
* Does NOT own selection policy, inspector formatting, or overlay layout.
*/
#include "live-code/live-editor.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "editor/entity-inspector.h"
#include "ecs/entity-registry.h"
#include "gui/ui-system.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"
#include "physics/ray-utils.h"
#include "ragdoll/ragdoll-components.h"
#include "world/world.h"

namespace {

const GameEditorModuleV1* editorModule()
{
    return static_cast<const GameEditorModuleV1*>(
        LiveModules::findFunctions("editor", sizeof(GameEditorModuleV1)));
}

const World* gLastWorld = nullptr;
EditorStateV1 gLastState{};
EditorResultV1 gLastResult{};

bool raySphere(const glm::vec3& origin, const glm::vec3& dir,
               const glm::vec3& center, float radius, float& t)
{
    const glm::vec3 oc = origin - center;
    const float b = glm::dot(oc, dir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f)
        return false;
    const float s = std::sqrt(disc);
    float hit = -b - s;
    if (hit < 0.0f)
        hit = -b + s;
    if (hit < 0.0f)
        return false;
    t = hit;
    return true;
}

void MIMITA_GAME_CALL editorQueryRayCap(void* host, const float origin[3],
                                        const float dir[3], float maxDistance,
                                        std::uint32_t maxHits, EditorQueryV1* out)
{
    if (!out)
        return;
    out->count = 0;
    const World* world = static_cast<const World*>(host);
    if (!world || !origin || !dir)
        return;

    const glm::vec3 o(origin[0], origin[1], origin[2]);
    glm::vec3 d(dir[0], dir[1], dir[2]);
    if (glm::length(d) < 1e-6f)
        return;
    d = glm::normalize(d);

    std::vector<EditorCandidateV1> candidates;

    // Nearest world triangle.
    const int triangle = selectWorldTriangle(*world, o, d);
    if (triangle >= 0 && triangle < (int)world->collisionMesh.triangles.size()) {
        EditorCandidateV1 c{};
        c.kind = EDITOR_HIT_WORLD;
        c.worldTriangle = (std::uint32_t)triangle;
        const CollisionTriangle& tri = world->collisionMesh.triangles[triangle];
        c.point[0] = (tri.a.x + tri.b.x + tri.c.x) / 3.0f;
        c.point[1] = (tri.a.y + tri.b.y + tri.c.y) / 3.0f;
        c.point[2] = (tri.a.z + tri.b.z + tri.c.z) / 3.0f;
        c.distance = glm::length(glm::vec3(c.point[0], c.point[1], c.point[2]) - o);
        if (c.distance <= maxDistance)
            candidates.push_back(c);
    }

    // Entity bounds.
    EntityRegistry& registry = EntityRegistry::instance();
    for (EntityId id : registry.all()) {
        const EntityIdentity* ident = registry.identity(id);
        if (!ident)
            continue;
        if (ident->domain == EntityDomain::None ||
            ident->domain == EntityDomain::WorldObject ||
            ident->domain == EntityDomain::Constraint)
            continue;
        glm::vec3 center(0.0f);
        float radius = 0.0f;
        if (const auto* limb = registry.tryGet<Ragdoll::LimbComponent>(id)) {
            center = limb->position;
            radius = limb->radius + limb->halfHeight;
        } else if (const auto* t = registry.tryGet<TransformComponent>(id)) {
            center = t->position;
            if (const auto* body = registry.tryGet<BodyComponent>(id))
                radius = std::max(body->radius, body->height * 0.5f);
            else if (const auto* col = registry.tryGet<ColliderComponent>(id))
                radius = std::max(col->radius, col->height * 0.5f);
            else
                radius = 0.5f;
        } else {
            continue;
        }
        if (radius <= 0.0f)
            radius = 0.5f;
        float t = 0.0f;
        if (raySphere(o, d, center, radius, t) && t <= maxDistance) {
            EditorCandidateV1 c{};
            c.kind = EDITOR_HIT_ENTITY;
            c.domain = (std::uint32_t)ident->domain;
            c.entity = (std::uint64_t)id;
            c.distance = t;
            const glm::vec3 p = o + d * t;
            c.point[0] = p.x;
            c.point[1] = p.y;
            c.point[2] = p.z;
            candidates.push_back(c);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const EditorCandidateV1& a, const EditorCandidateV1& b) {
                  return a.distance < b.distance;
              });
    const std::size_t count = std::min<std::size_t>(candidates.size(), maxHits);
    for (std::size_t i = 0; i < count; ++i)
        out->hits[i] = candidates[i];
    out->count = (std::uint32_t)count;
}

std::uint32_t MIMITA_GAME_CALL editorInspectCap(void*, std::uint64_t entity,
                                                EditorInspectionV1* out)
{
    if (!out)
        return 0;
    *out = EditorInspectionV1{};
    if (entity == 0)
        return 0;
    const Editor::EntityInspection ins = Editor::inspectEntity((EntityId)entity);
    out->valid = ins.alive ? 1u : 0u;
    out->realm = (std::uint32_t)ins.realm;
    out->domain = (std::uint32_t)ins.domain;
    out->legacyId = ins.legacyId;
    out->generation = ins.generation;
    const std::size_t count = std::min<std::size_t>(
        ins.components.size(), (std::size_t)EDITOR_COMPONENT_COUNT);
    out->componentCount = (std::uint32_t)count;
    for (std::size_t i = 0; i < count; ++i) {
        std::strncpy(out->components[i], ins.components[i].c_str(),
                     sizeof(out->components[i]) - 1);
        out->components[i][sizeof(out->components[i]) - 1] = '\0';
    }
    out->position[0] = ins.transform.position.x;
    out->position[1] = ins.transform.position.y;
    out->position[2] = ins.transform.position.z;
    out->yaw = ins.transform.yaw;
    if (ins.hasConstraint) {
        out->hasConstraint = 1;
        out->constraintSerial = ins.constraint.constraintSerial;
        out->constraintActive = ins.constraint.constraint.active ? 1u : 0u;
        out->constraintType = (std::uint32_t)ins.constraint.constraint.type;
        out->constraintBodyA = ins.constraint.constraint.bodyA;
        out->constraintBodyB = ins.constraint.constraint.bodyB;
        out->constraintStrength = ins.constraint.constraint.strength;
    }
    out->linkedConstraintSerial = ins.linkedConstraintSerial;
    return 1;
}

void MIMITA_GAME_CALL editorDrawTextCap(void*, const char* text, float x, float y,
                                        float scale, const float rgba[4])
{
    uiDrawText(text ? text : "", x, y, scale,
               glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

void MIMITA_GAME_CALL editorDrawRectCap(void*, float x, float y, float w, float h,
                                        const float rgba[4])
{
    UIRect r{x, y, w, h};
    uiDrawRect(r, glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]), "editor-hot");
}

void MIMITA_GAME_CALL editorScreenSizeCap(void*, float* outWidth, float* outHeight)
{
    if (outWidth)
        *outWidth = uiScreenW();
    if (outHeight)
        *outHeight = uiScreenH();
}

EditorContextV1 makeContext(const World* world, std::uint64_t tick)
{
    GameMemory& memory = HotReloadSystem::instance().gameMemory();
    const HotReloadSystem::Status status = HotReloadSystem::instance().status();
    EditorContextV1 context{};
    context.abiVersion = MIMITA_GAME_API_VERSION;
    context.structSize = sizeof(EditorContextV1);
    context.host = const_cast<World*>(world);
    context.tick = tick;
    context.generation = status.activeGeneration;
    context.queryRay = &editorQueryRayCap;
    context.inspect = &editorInspectCap;
    context.drawText = &editorDrawTextCap;
    context.drawRect = &editorDrawRectCap;
    context.screenSize = &editorScreenSizeCap;
    context.permanentStorage = memory.permanentStorage;
    context.permanentStorageSize = memory.permanentStorageSize;
    return context;
}

} // namespace

namespace LiveEditor {

bool available()
{
    const GameEditorModuleV1* module = editorModule();
    return module && module->onTick != nullptr;
}

bool tick(const World& world, const EditorStateV1& state, EditorResultV1& out)
{
    const GameEditorModuleV1* module = editorModule();
    if (!module || !module->onTick)
        return false;
    gLastWorld = &world;
    gLastState = state;
    gLastResult = EditorResultV1{};
    EditorContextV1 context = makeContext(&world, state.tick);
    module->onTick(&state, &context, &gLastResult);
    out = gLastResult;
    return gLastResult.handled != 0;
}

bool drawOverlay()
{
    const GameEditorModuleV1* module = editorModule();
    if (!module || !module->onDraw)
        return false;
    EditorContextV1 context = makeContext(gLastWorld, gLastState.tick);
    module->onDraw(&gLastState, &gLastResult, &context);
    return true;
}

const EditorStateV1& lastState() { return gLastState; }
const EditorResultV1& lastResult() { return gLastResult; }

} // namespace LiveEditor
