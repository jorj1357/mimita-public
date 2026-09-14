// 09 13 2026
/* purpose
* Implements the EXE-side editor module bridge and capabilities.
* Does NOT own selection policy, inspector formatting, or overlay layout.
*/
#include "live-code/live-editor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "camera.h"
#include "debug/debug-visuals.h"
#include "devtools/terminal.h"
#include "ecs/entity-registry.h"
#include "editor/creation-mode.h"
#include "editor/entity-inspector.h"
#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"
#include "hot-reload/hot-reload-system.h"
#include "live-code/live-modules.h"
#include "physics/constraints/constraint-store.h"
#include "physics/ray-utils.h"
#include "ragdoll/ragdoll-components.h"
#include "terminal/terminal-state.h"
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
EditorInputV1 gInput{};
EditorInputV1 gPrevInput{};

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

// Match a world hit point to a legacy block or a GLB mesh batch (smallest
// containing AABB), else fall back to the raw triangle.
void MIMITA_GAME_CALL editorWorldObjectInfoCap(void* host, const float pointIn[3],
                                               EditorWorldObjectV1* out)
{
    if (!out)
        return;
    *out = EditorWorldObjectV1{};
    const World* world = static_cast<const World*>(host);
    if (!world || !pointIn)
        return;
    const glm::vec3 point(pointIn[0], pointIn[1], pointIn[2]);
    out->valid = 1;
    out->kind = 3;  // raw triangle
    out->center[0] = point.x;
    out->center[1] = point.y;
    out->center[2] = point.z;

    float bestVolume = 1e30f;
    for (std::size_t i = 0; i < world->blocks.size(); ++i) {
        const Block& b = world->blocks[i];
        const glm::vec3 mn = b.pos - b.size * 0.5f;
        const glm::vec3 mx = b.pos + b.size * 0.5f;
        if (point.x < mn.x || point.x > mx.x || point.y < mn.y || point.y > mx.y ||
            point.z < mn.z || point.z > mx.z)
            continue;
        const float volume = b.size.x * b.size.y * b.size.z;
        if (volume < bestVolume) {
            bestVolume = volume;
            out->kind = 1;
            out->index = (std::uint32_t)i;
            out->center[0] = b.pos.x;
            out->center[1] = b.pos.y;
            out->center[2] = b.pos.z;
            out->size[0] = b.size.x;
            out->size[1] = b.size.y;
            out->size[2] = b.size.z;
            std::strncpy(out->material, b.texName.c_str(), sizeof(out->material) - 1);
            out->material[sizeof(out->material) - 1] = '\0';
        }
    }
    for (std::size_t i = 0; i < world->mesh.batches.size(); ++i) {
        const Mesh::Batch& batch = world->mesh.batches[i];
        if (!batch.hasBounds)
            continue;
        if (point.x < batch.boundsMin.x || point.x > batch.boundsMax.x ||
            point.y < batch.boundsMin.y || point.y > batch.boundsMax.y ||
            point.z < batch.boundsMin.z || point.z > batch.boundsMax.z)
            continue;
        const glm::vec3 size = batch.boundsMax - batch.boundsMin;
        const float volume = size.x * size.y * size.z;
        if (volume < bestVolume) {
            bestVolume = volume;
            out->kind = 2;
            out->index = (std::uint32_t)i;
            const glm::vec3 center = (batch.boundsMin + batch.boundsMax) * 0.5f;
            out->center[0] = center.x;
            out->center[1] = center.y;
            out->center[2] = center.z;
            out->size[0] = size.x;
            out->size[1] = size.y;
            out->size[2] = size.z;
            std::strncpy(out->material, batch.materialName.c_str(),
                         sizeof(out->material) - 1);
            out->material[sizeof(out->material) - 1] = '\0';
        }
    }
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

    const int triangle = selectWorldTriangle(*world, o, d);
    if (triangle >= 0 && triangle < (int)world->collisionMesh.triangles.size()) {
        EditorCandidateV1 c{};
        c.kind = EDITOR_HIT_WORLD;
        c.worldTriangle = (std::uint32_t)triangle;
        const CollisionTriangle& tri = world->collisionMesh.triangles[triangle];
        const glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
        c.point[0] = center.x;
        c.point[1] = center.y;
        c.point[2] = center.z;
        c.distance = glm::length(center - o);
        if (c.distance <= maxDistance)
            candidates.push_back(c);
    }

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
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityId id = (EntityId)entity;
    const Editor::EntityInspection ins = Editor::inspectEntity(id);
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

std::uint32_t MIMITA_GAME_CALL editorInspectExCap(void*, std::uint64_t entity,
                                                  EditorInspectionExV1* out)
{
    if (!out)
        return 0;
    *out = EditorInspectionExV1{};
    if (entity == 0)
        return 0;
    EntityRegistry& registry = EntityRegistry::instance();
    const EntityId id = (EntityId)entity;
    const Editor::EntityInspection ins = Editor::inspectEntity(id);
    if (!ins.alive)
        return 0;
    out->valid = 1;
    if (const auto* hp = registry.tryGet<HealthComponent>(id)) {
        out->hasHealth = 1;
        out->health = hp->current;
        out->maxHealth = hp->max;
        out->dead = hp->dead ? 1u : 0u;
    }
    if (const auto* cs = registry.tryGet<ControlSourceComponent>(id)) {
        out->hasControl = 1;
        out->controlSource = (std::uint32_t)cs->source;
    }
    if (const auto* na = registry.tryGet<NetworkAuthorityComponent>(id))
        out->authority = (std::uint32_t)na->authority;
    if (const auto* owner = registry.tryGet<OwnerComponent>(id))
        out->ownerEntity = owner->owner;
    if (const auto* inv = registry.tryGet<WeaponInventoryComponent>(id)) {
        out->hasWeapon = 1;
        out->weaponNetworkId = inv->equippedNetworkId;
        out->weaponSlot = (std::int32_t)inv->slot;
    }
    if (const auto* limb = registry.tryGet<Ragdoll::LimbComponent>(id)) {
        out->isRagdollLimb = 1;
        out->limbIndex = limb->limbIndex;
        out->limbParent = limb->parentIndex;
    }
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

void MIMITA_GAME_CALL editorDrawWorldLabelCap(void*, const float worldPos[3],
                                              const char* text, const float rgba[4])
{
    if (!worldPos || !text)
        return;
    DebugVis::drawWorldLabel(glm::vec3(worldPos[0], worldPos[1], worldPos[2]), text,
                             glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

// Box edges via the weapon-line primitive (bypasses the master debug gate).
void drawWireBoxEdges(const Camera& cam, const glm::vec3& center,
                      const glm::vec3& half, const glm::vec4& color)
{
    const glm::vec3 c[8] = {
        center + glm::vec3(-half.x, -half.y, -half.z),
        center + glm::vec3(half.x, -half.y, -half.z),
        center + glm::vec3(half.x, half.y, -half.z),
        center + glm::vec3(-half.x, half.y, -half.z),
        center + glm::vec3(-half.x, -half.y, half.z),
        center + glm::vec3(half.x, -half.y, half.z),
        center + glm::vec3(half.x, half.y, half.z),
        center + glm::vec3(-half.x, half.y, half.z),
    };
    const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for (const auto& e : edges)
        DebugVis::drawWeaponLine(cam, c[e[0]], c[e[1]], color);
}

void MIMITA_GAME_CALL editorDrawOutlineCap(void*, std::uint32_t hitKind,
                                           std::uint64_t entity,
                                           std::uint32_t worldKind,
                                           std::uint32_t worldIndex,
                                           const float rgba[4], float thickness,
                                           std::uint32_t throughWalls)
{
    (void)throughWalls;
    const glm::vec4 color(rgba[0], rgba[1], rgba[2], rgba[3]);
    const Camera& cam = THE_CAMERA;
    const float t = std::max(0.01f, thickness);

    if (hitKind == EDITOR_HIT_ENTITY && entity != 0) {
        EntityRegistry& registry = EntityRegistry::instance();
        const EntityId id = (EntityId)entity;
        if (const auto* limb = registry.tryGet<Ragdoll::LimbComponent>(id)) {
            Capsule cap;
            cap.a = limb->position + limb->orientation * glm::vec3(0.0f, 0.0f, limb->halfHeight);
            cap.b = limb->position - limb->orientation * glm::vec3(0.0f, 0.0f, limb->halfHeight);
            cap.r = limb->radius + t * 0.02f;
            DebugVis::drawWeaponCapsuleWire(cam, cap, color);
            return;
        }
        if (const auto* tr = registry.tryGet<TransformComponent>(id)) {
            float radius = 0.6f;
            if (const auto* body = registry.tryGet<BodyComponent>(id))
                radius = std::max(body->radius, body->height * 0.5f);
            else if (const auto* col = registry.tryGet<ColliderComponent>(id))
                radius = std::max(col->radius, col->height * 0.5f);
            DebugVis::drawWeaponWireSphere(cam, tr->position, radius + t * 0.02f, color);
            return;
        }
        return;
    }

    if (hitKind == EDITOR_HIT_WORLD && gLastWorld) {
        const World& world = *gLastWorld;
        if (worldKind == 1 && worldIndex < world.blocks.size()) {
            const Block& b = world.blocks[worldIndex];
            drawWireBoxEdges(cam, b.pos, b.size * 0.5f + glm::vec3(t * 0.01f), color);
        } else if (worldKind == 2 && worldIndex < world.mesh.batches.size()) {
            const Mesh::Batch& batch = world.mesh.batches[worldIndex];
            if (batch.hasBounds) {
                const glm::vec3 center = (batch.boundsMin + batch.boundsMax) * 0.5f;
                const glm::vec3 half = (batch.boundsMax - batch.boundsMin) * 0.5f;
                drawWireBoxEdges(cam, center, half + glm::vec3(t * 0.01f), color);
            }
        } else if (world.collisionMesh.triangles.size() > worldIndex) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[worldIndex];
            DebugVis::drawWeaponLine(cam, tri.a, tri.b, color);
            DebugVis::drawWeaponLine(cam, tri.b, tri.c, color);
            DebugVis::drawWeaponLine(cam, tri.c, tri.a, color);
        }
        DebugVis::flushWeaponLines(cam);
    }
}

void MIMITA_GAME_CALL editorDrawWireBoxCap(void*, const float center[3],
                                           const float size[3], const float rgba[4])
{
    if (!center || !size)
        return;
    const Camera& cam = THE_CAMERA;
    const glm::vec3 c(center[0], center[1], center[2]);
    glm::vec3 half(size[0] * 0.5f, size[1] * 0.5f, size[2] * 0.5f);
    if (half.x <= 0.0f) half.x = 0.5f;
    if (half.y <= 0.0f) half.y = 0.5f;
    if (half.z <= 0.0f) half.z = 0.5f;
    drawWireBoxEdges(cam, c, half, glm::vec4(rgba[0], rgba[1], rgba[2], rgba[3]));
    DebugVis::flushWeaponLines(cam);
}

std::uint32_t MIMITA_GAME_CALL editorForkOpCountCap(void*)
{
    return (std::uint32_t)Editor::CreationMode::instance().patch().size();
}

std::uint32_t MIMITA_GAME_CALL editorForkOpCap(void*, std::uint32_t index,
                                               EditorForkArgsV1* out)
{
    if (!out)
        return 0;
    const std::vector<Editor::PatchOp>& patch = Editor::CreationMode::instance().patch();
    if (index >= patch.size())
        return 0;
    const Editor::PatchOp& op = patch[index];
    *out = EditorForkArgsV1{};
    switch (op.kind) {
    case Editor::PatchOp::Kind::Duplicate: out->op = EDITOR_FORK_DUPLICATE; break;
    case Editor::PatchOp::Kind::Transform: out->op = EDITOR_FORK_SET_TRANSFORM; break;
    case Editor::PatchOp::Kind::Delete: out->op = EDITOR_FORK_DELETE; break;
    case Editor::PatchOp::Kind::Material: out->op = EDITOR_FORK_SET_MATERIAL; break;
    case Editor::PatchOp::Kind::Label: out->op = EDITOR_FORK_SET_LABEL; break;
    default: out->op = EDITOR_FORK_SET_TRANSFORM; break;
    }
    out->sourceEntity = op.sourceEntity;
    out->outObjectId = op.newEntity;
    out->position[0] = op.position.x;
    out->position[1] = op.position.y;
    out->position[2] = op.position.z;
    out->rotation[0] = op.rotation.x;
    out->rotation[1] = op.rotation.y;
    out->rotation[2] = op.rotation.z;
    out->scale[0] = op.scale.x;
    out->scale[1] = op.scale.y;
    out->scale[2] = op.scale.z;
    out->size[0] = op.size.x;
    out->size[1] = op.size.y;
    out->size[2] = op.size.z;
    std::strncpy(out->label, op.label.c_str(), sizeof(out->label) - 1);
    std::strncpy(out->material, op.material.c_str(), sizeof(out->material) - 1);
    return 1;
}

void MIMITA_GAME_CALL editorMapInfoCap(void* host, EditorMapInfoV1* out)
{
    if (!out)
        return;
    *out = EditorMapInfoV1{};
    const World* world = static_cast<const World*>(host);
    if (!world)
        return;
    out->valid = 1;
    out->blockCount = (std::uint32_t)world->blocks.size();
    out->batchCount = (std::uint32_t)world->mesh.batches.size();
    out->spawnCount = (std::uint32_t)world->spawnPoints.size();
    out->triangleCount = (std::uint32_t)world->collisionMesh.triangles.size();
    out->boundsMin[0] = world->collisionMesh.boundsMin.x;
    out->boundsMin[1] = world->collisionMesh.boundsMin.y;
    out->boundsMin[2] = world->collisionMesh.boundsMin.z;
    out->boundsMax[0] = world->collisionMesh.boundsMax.x;
    out->boundsMax[1] = world->collisionMesh.boundsMax.y;
    out->boundsMax[2] = world->collisionMesh.boundsMax.z;
    std::strncpy(out->mapPath, ACTIVE_MAP_PATH.c_str(), sizeof(out->mapPath) - 1);
    out->mapPath[sizeof(out->mapPath) - 1] = '\0';
}

void MIMITA_GAME_CALL editorForkCap(void*, EditorForkArgsV1* args)
{
    if (!args)
        return;
    Editor::CreationMode& mode = Editor::CreationMode::instance();
    args->ok = 0;
    args->outObjectId = 0;
    switch (args->op) {
    case EDITOR_FORK_DUPLICATE: {
        Editor::WorldObjectRef ref;
        ref.entity = args->sourceEntity;
        ref.kind = args->sourceKind == EDITOR_HIT_WORLD ? "triangle" : "entity";
        ref.sourceIndex = args->sourceWorldIndex;
        ref.position = glm::vec3(args->position[0], args->position[1], args->position[2]);
        ref.rotation = glm::vec3(args->rotation[0], args->rotation[1], args->rotation[2]);
        ref.scale = glm::vec3(args->scale[0], args->scale[1], args->scale[2]);
        ref.size = glm::vec3(args->size[0], args->size[1], args->size[2]);
        const std::uint64_t id = mode.duplicate(ref, glm::vec3(0.0f));
        args->outObjectId = id;
        args->ok = id != 0 ? 1u : 0u;
        break;
    }
    case EDITOR_FORK_DELETE:
        args->ok = mode.remove(args->sourceEntity) ? 1u : 0u;
        break;
    case EDITOR_FORK_SET_TRANSFORM:
        args->ok = mode.transform(args->sourceEntity,
                                  glm::vec3(args->position[0], args->position[1], args->position[2]),
                                  glm::vec3(args->rotation[0], args->rotation[1], args->rotation[2]),
                                  glm::vec3(args->scale[0], args->scale[1], args->scale[2]))
            ? 1u : 0u;
        break;
    case EDITOR_FORK_SET_LABEL:
        args->ok = mode.setLabel(args->sourceEntity, args->label) ? 1u : 0u;
        break;
    case EDITOR_FORK_SET_MATERIAL:
        args->ok = mode.setMaterial(args->sourceEntity, args->material) ? 1u : 0u;
        break;
    case EDITOR_FORK_UNDO:
        args->ok = mode.undo() ? 1u : 0u;
        break;
    case EDITOR_FORK_REDO:
        args->ok = mode.redo() ? 1u : 0u;
        break;
    default:
        break;
    }
    args->opCount = (std::uint32_t)mode.patch().size();
    const std::string hash = mode.forkHash();
    std::strncpy(args->forkHash, hash.c_str(), sizeof(args->forkHash) - 1);
    args->forkHash[sizeof(args->forkHash) - 1] = '\0';
}

void pollInput()
{
    gPrevInput = gInput;
    gInput = EditorInputV1{};
    gInput.enabled = Editor::CreationMode::instance().enabled() ? 1u : 0u;
    GLFWwindow* window = Terminal::instance().window();
    if (!window)
        return;
    auto edge = [&](int key) -> std::uint32_t {
        return glfwGetKey(window, key) == GLFW_PRESS ? 1u : 0u;
    };
    gInput.dropPressed = edge(GLFW_KEY_BACKSPACE);
    gInput.interactPressed = edge(GLFW_KEY_F);
    gInput.confirmPressed = edge(GLFW_KEY_ENTER);
    gInput.cancelPressed = edge(GLFW_KEY_ESCAPE);
    gInput.copyPressed = edge(GLFW_KEY_C);
    gInput.pastePressed = edge(GLFW_KEY_V);
    gInput.deletePressed = edge(GLFW_KEY_DELETE);
    gInput.rotatePressed = edge(GLFW_KEY_R);
    gInput.scalePressed = edge(GLFW_KEY_T);

    // Selection: CTRL + left mouse.
    const bool ctrlDown =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool leftMouse =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    gInput.selectPressed = (ctrlDown && leftMouse) ? 1u : 0u;

    // Overlap cycling: mouse wheel (diffed from the shared scroll offset).
    static double sLastScroll = 0.0;
    const double scroll = UISys::gScrollYOffset;
    if (scroll > sLastScroll + 0.01)
        gInput.cyclePrev = 1;
    else if (scroll < sLastScroll - 0.01)
        gInput.cycleNext = 1;
    sLastScroll = scroll;

    gInput.moveUp = edge(GLFW_KEY_SPACE);
    gInput.moveDown = edge(GLFW_KEY_LEFT_SHIFT);
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
    context.drawWorldLabel = &editorDrawWorldLabelCap;
    context.drawOutline = &editorDrawOutlineCap;
    context.mapInfo = &editorMapInfoCap;
    context.fork = &editorForkCap;
    context.input = &gInput;
    context.inspectEx = &editorInspectExCap;
    context.worldObjectInfo = &editorWorldObjectInfoCap;
    context.editorAbiVersion = 2;
    context.drawWireBox = &editorDrawWireBoxCap;
    context.forkOpCount = &editorForkOpCountCap;
    context.forkOp = &editorForkOpCap;
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
    pollInput();
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
