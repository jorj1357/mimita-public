// 2026-10-23
#include "mapeditor/map-editor.h"
#include "mapeditor/map-editor-projects.h"
#include "mapeditor/map-editor-config.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "game/game-state.h"
#include "gamemode/map-config.h"
#include "gui/gui-layout.h"
#include "gui/hud/chat-window.h"
#include "gui/menus/pause-menu.h"
#include "gui/ui-system.h"
#include "hot-reload/module-host.h"
#include "input/mouse-lock.h"
#include "modules/api/map-editor-api.h"
#include "map/map-authored-geometry.h"
#include "map/map-loader-collision.h"
#include "network/multiplayer-context.h"
#include "network/server-gamemode.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "procedural/infinite-world-runtime.h"
#include "renderer/renderer.h"
#include "terminal/terminal-state.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-system.h"
#include "world/streamed-world-layer.h"
#include "world/world.h"

extern Renderer* gRenderer;

namespace {

constexpr float kVertexSnapRadius = 0.85f;
constexpr float kOutlinerHeaderHeight = 68.0f;
constexpr float kOutlinerListGap = 8.0f;
constexpr size_t kMaxHistory = 60;

std::string activeMapId()
{
    const std::string& serverMap = MimitaNet::serverGamemodeState().mapId;
    if (!serverMap.empty()) return serverMap;
    if (gpActiveMapPath && !gpActiveMapPath->empty())
        return mapIdFromAssetPath(*gpActiveMapPath);
    return "zombietower1";
}

MapConfigRegistry& registry()
{
    static std::string loaded;
    auto& reg = MapConfigRegistry::instance();
    const std::string mapId = activeMapId();
    // Do not reload a map that is already active: editor-local authoring and
    // worldchunk/worldedit commands may hold unsaved in-memory changes.
    if (reg.mapId() != mapId) {
        reg.load(mapId);
        loaded = mapId;
    } else if (loaded != mapId) {
        loaded = mapId;
    }
    return reg;
}

glm::mat4 blockTransform(const AuthoredBlock& block)
{
    const glm::mat4 translate = glm::translate(glm::mat4(1.0f), block.position);
    const glm::mat4 rotateZ = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.z), glm::vec3(0, 0, 1));
    const glm::mat4 rotateY = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.y), glm::vec3(0, 1, 0));
    const glm::mat4 rotateX = glm::rotate(glm::mat4(1.0f),
        glm::radians(block.rotationEuler.x), glm::vec3(1, 0, 0));
    const glm::mat4 scale = glm::scale(glm::mat4(1.0f), block.size);
    return translate * rotateZ * rotateY * rotateX * scale;
}

float screenWidth()
{
    return gRenderer ? (float)(gRenderer->stretchesWholeFrame()
        ? gRenderer->sceneWidth : gRenderer->width) : uiScreenW();
}

float screenHeight()
{
    return gRenderer ? (float)(gRenderer->stretchesWholeFrame()
        ? gRenderer->sceneHeight : gRenderer->height) : uiScreenH();
}

glm::vec2 cursorScreen(GLFWwindow* window, double mouseX, double mouseY)
{
    int winW = 1;
    int winH = 1;
    if (window) glfwGetWindowSize(window, &winW, &winH);
    const float w = screenWidth();
    const float h = screenHeight();
    return {
        (float)mouseX * w / (float)std::max(1, winW),
        (float)mouseY * h / (float)std::max(1, winH)
    };
}

bool isNumberChar(unsigned int codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.' ||
           codepoint == '-' || codepoint == '+';
}

float pointSegmentDistance(float px, float py, float ax, float ay, float bx, float by)
{
    const float vx = bx - ax;
    const float vy = by - ay;
    const float wx = px - ax;
    const float wy = py - ay;
    const float len2 = vx * vx + vy * vy;
    float t = len2 > 1e-6f ? (wx * vx + wy * vy) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const float dx = px - (ax + vx * t);
    const float dy = py - (ay + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

glm::vec3 rayAxisPoint(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                       const glm::vec3& axisOrigin, const glm::vec3& axisDir)
{
    const glm::vec3 w0 = axisOrigin - rayOrigin;
    const float a = glm::dot(rayDir, rayDir);
    const float b = glm::dot(rayDir, axisDir);
    const float c = glm::dot(axisDir, axisDir);
    const float d = glm::dot(rayDir, w0);
    const float e = glm::dot(axisDir, w0);
    const float denom = a * c - b * b;
    if (std::abs(denom) < 1e-6f) return axisOrigin;
    const float tAxis = (a * e - b * d) / denom;
    return axisOrigin + axisDir * tAxis;
}

void drawOrientedBox(const Camera& camera, const glm::vec3& center,
                     const glm::vec3& size, const glm::vec3& rotationEuler,
                     const glm::vec4& color)
{
    AuthoredBlock block;
    block.position = center;
    block.size = size;
    block.rotationEuler = rotationEuler;
    const glm::mat4 transform = blockTransform(block);
    glm::vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 local(
            (i & 1) ? 0.5f : -0.5f,
            (i & 2) ? 0.5f : -0.5f,
            (i & 4) ? 0.5f : -0.5f);
        corners[i] = glm::vec3(transform * glm::vec4(local, 1.0f));
    }
    static const int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0},
        {4,5},{5,7},{7,6},{6,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (const auto& edge : edges)
        DebugVis::drawLine(camera, corners[edge[0]], corners[edge[1]], color);
}

} // namespace

MapEditor& MapEditor::instance()
{
    static MapEditor editor;
    return editor;
}

bool MapEditor::setActive(bool enabled)
{
    if (enabled == mActive) return true;
    if (enabled) {
        if (GAME_STATE != GAME_PLAYING) {
            notify("enter a game before using the map editor");
            return false;
        }
        if (!gpWorld || !WORLD_LOADED) {
            notify("world is not loaded");
            return false;
        }
        mActive = true;
        mFreezeWorld = true;
        mPrevFreecam = FREECAM_ENABLED;
        FREECAM_ENABLED = true;
        DebugVis::setMasterEnabled(true);
        DebugConfig::DEBUG_RENDER = true;
        THE_CAMERA.firstMouse = true;
        GLFWwindow* window = glfwGetCurrentContext();
        mPrevMouseLock = MouseLock::locked();
        if (window) {
            MouseLock::set(window, false);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        registry();
        notify("enabled map=" + mapId() + " objects=" +
               std::to_string(registry().current().entities.size()) + "/" +
               std::to_string(registry().current().blocks.size()) + "/" +
               std::to_string(registry().current().payload.route.size()));
        return true;
    }

    endDrag(false);
    if (dirty()) autosave();
    flushCollisionNow();
    mActive = false;
    mLooking = false;
    mHoverValid = false;
    mHoverAxis = -1;
    FREECAM_ENABLED = mPrevFreecam;
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        MouseLock::set(window, mPrevMouseLock);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        THE_CAMERA.firstMouse = true;
    }
    notify("disabled; unsaved changes remain in memory");
    return true;
}

void MapEditor::toggle()
{
    setActive(!mActive);
}

void MapEditor::setFreezeWorld(bool freeze)
{
    if (!freeze)
        flushCollisionNow();
    mFreezeWorld = freeze;
    notify(std::string("world ") + (freeze ? "frozen" : "unfrozen"));
}

bool MapEditor::dirty() const
{
    return MapConfigRegistry::instance().dirty();
}

std::string MapEditor::mapId() const
{
    return activeMapId();
}

void MapEditor::notify(const std::string& message)
{
    mStatus = message;
    mLastMessageTime = glfwGetTime();
    Terminal::instance().addLog("[MAPEDIT] " + message);
}

void MapEditor::clearSelection()
{
    mSelection.clear();
    mHoverValid = false;
    mHoverAxis = -1;
}

void MapEditor::selectOnly(const EditorObjectRef& ref)
{
    mSelection.clear();
    mSelection.push_back(ref);
    std::string chunkId;
    std::string itemId;
    if ((ref.kind == EditorObjectKind::ChunkPart ||
         ref.kind == EditorObjectKind::ChunkConnector) &&
        splitChunkRef(ref.id, chunkId, itemId))
        ensureChunkPreview(chunkId);
}

void MapEditor::toggleSelection(const EditorObjectRef& ref)
{
    const auto it = std::find(mSelection.begin(), mSelection.end(), ref);
    if (it == mSelection.end()) {
        mSelection.push_back(ref);
        std::string chunkId;
        std::string itemId;
        if ((ref.kind == EditorObjectKind::ChunkPart ||
             ref.kind == EditorObjectKind::ChunkConnector) &&
            splitChunkRef(ref.id, chunkId, itemId))
            ensureChunkPreview(chunkId);
    } else {
        mSelection.erase(it);
    }
}

bool MapEditor::isSelected(const EditorObjectRef& ref) const
{
    return std::find(mSelection.begin(), mSelection.end(), ref) != mSelection.end();
}

const MapEntity* MapEditor::entityFor(const EditorObjectRef& ref) const
{
    return ref.kind == EditorObjectKind::Entity
        ? MapConfigRegistry::instance().findEntity(ref.id) : nullptr;
}

const AuthoredBlock* MapEditor::blockFor(const EditorObjectRef& ref) const
{
    return ref.kind == EditorObjectKind::Block
        ? MapConfigRegistry::instance().findBlock(ref.id) : nullptr;
}

const MapVehicleSpawn* MapEditor::vehicleFor(const EditorObjectRef& ref) const
{
    return ref.kind == EditorObjectKind::Vehicle
        ? MapConfigRegistry::instance().findVehicle(ref.id) : nullptr;
}

const PayloadRoutePoint* MapEditor::routeFor(const EditorObjectRef& ref) const
{
    return ref.kind == EditorObjectKind::PayloadRoutePoint
        ? MapConfigRegistry::instance().findPayloadRoutePoint(ref.id) : nullptr;
}

MapGLBObject* MapEditor::mapObjectFor(const EditorObjectRef& ref) const
{
    if (ref.kind != EditorObjectKind::MapGLBObject || !gpWorld)
        return nullptr;
    for (MapGLBObject& object : gpWorld->mesh.objects)
        if (object.id == ref.id)
            return &object;
    return nullptr;
}

bool MapEditor::splitChunkRef(const std::string& id, std::string& chunkId,
                              std::string& itemId)
{
    const size_t separator = id.find('/');
    if (separator == std::string::npos || separator == 0 ||
        separator + 1 >= id.size())
        return false;
    chunkId = id.substr(0, separator);
    itemId = id.substr(separator + 1);
    return true;
}

AuthoredChunk* MapEditor::chunkFor(const EditorObjectRef& ref) const
{
    if (ref.kind != EditorObjectKind::ChunkPart &&
        ref.kind != EditorObjectKind::ChunkConnector &&
        ref.kind != EditorObjectKind::ChunkSpawn)
        return nullptr;
    std::string chunkId;
    std::string itemId;
    if (!splitChunkRef(ref.id, chunkId, itemId)) return nullptr;
    for (AuthoredChunk& chunk : MapConfigRegistry::instance().chunksMutable())
        if (chunk.id == chunkId) return &chunk;
    return nullptr;
}

ChunkPart* MapEditor::chunkPartFor(const EditorObjectRef& ref) const
{
    if (ref.kind != EditorObjectKind::ChunkPart) return nullptr;
    std::string chunkId;
    std::string partId;
    if (!splitChunkRef(ref.id, chunkId, partId)) return nullptr;
    AuthoredChunk* chunk = chunkFor(ref);
    if (!chunk) return nullptr;
    for (ChunkPart& part : chunk->parts)
        if (part.id == partId) return &part;
    return nullptr;
}

AuthoredChunkConnector* MapEditor::chunkConnectorFor(
    const EditorObjectRef& ref) const
{
    if (ref.kind != EditorObjectKind::ChunkConnector) return nullptr;
    std::string chunkId;
    std::string connectorId;
    if (!splitChunkRef(ref.id, chunkId, connectorId)) return nullptr;
    AuthoredChunk* chunk = chunkFor(ref);
    if (!chunk) return nullptr;
    for (AuthoredChunkConnector& connector : chunk->connectors)
        if (connector.id == connectorId) return &connector;
    return nullptr;
}

ChunkSpawnPoint* MapEditor::chunkSpawnFor(const EditorObjectRef& ref) const
{
    if (ref.kind != EditorObjectKind::ChunkSpawn) return nullptr;
    std::string chunkId;
    std::string spawnId;
    if (!splitChunkRef(ref.id, chunkId, spawnId)) return nullptr;
    AuthoredChunk* chunk = chunkFor(ref);
    if (!chunk) return nullptr;
    for (ChunkSpawnPoint& spawn : chunk->spawnPoints)
        if (spawn.id == spawnId) return &spawn;
    return nullptr;
}

bool MapEditor::ensureChunkPreview(const std::string& chunkId)
{
    for (const AuthoredChunk& chunk : MapConfigRegistry::instance().chunks())
        if (chunk.id == chunkId) {
            if (!mChunkPreviewValid || mChunkPreviewChunkId != chunkId) {
                mChunkPreviewChunkId = chunkId;
                mChunkPreviewOrigin = THE_CAMERA.pos + THE_CAMERA.front * 12.0f;
                mChunkPreviewValid = true;
            }
            return true;
        }
    return false;
}

void MapEditor::refreshPreviewChunk()
{
    if (!mChunkPreviewValid) return;
    for (const AuthoredChunk& chunk : MapConfigRegistry::instance().chunks())
        if (chunk.id == mChunkPreviewChunkId) {
            StreamedWorldLayer::instance().refreshChunk(chunk);
            return;
        }
}

bool MapEditor::previewChunk(const std::string& chunkId)
{
    if (!ensureChunkPreview(chunkId)) {
        notify("chunk not found: " + chunkId);
        return false;
    }
    mChunkPreviewOrigin = THE_CAMERA.pos + THE_CAMERA.front * 12.0f;
    notify("previewing chunk " + chunkId);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-preview", chunkId,
        "chunk preview placed in the editor viewport", 0,
        {{"chunk", chunkId},
         {"origin", {mChunkPreviewOrigin.x, mChunkPreviewOrigin.y,
                     mChunkPreviewOrigin.z}}},
        __FILE__, __LINE__, __func__);
    return true;
}

glm::vec3 MapEditor::objectPosition(const EditorObjectRef& ref) const
{
    if (ref.kind == EditorObjectKind::ChunkPart) {
        if (const ChunkPart* part = chunkPartFor(ref))
            return mChunkPreviewOrigin + part->position;
        return glm::vec3(0.0f);
    }
    if (ref.kind == EditorObjectKind::ChunkConnector) {
        if (const AuthoredChunkConnector* connector = chunkConnectorFor(ref))
            return mChunkPreviewOrigin + connector->position;
        return glm::vec3(0.0f);
    }
    if (ref.kind == EditorObjectKind::ChunkSpawn) {
        if (const ChunkSpawnPoint* spawn = chunkSpawnFor(ref))
            return mChunkPreviewOrigin + spawn->position;
        return glm::vec3(0.0f);
    }
    if (ref.kind == EditorObjectKind::PayloadCartOrigin)
        return MapConfigRegistry::instance().payload().cartOrigin;
    if (ref.kind == EditorObjectKind::BombSite) {
        if (const BombSite* site = MapConfigRegistry::instance().findSite(ref.id))
            return site->position;
        return glm::vec3(0.0f);
    }
    if (const auto* object = mapObjectFor(ref)) return glm::vec3(object->transform[3]);
    if (const auto* entity = entityFor(ref)) return entity->position;
    if (const auto* block = blockFor(ref)) return block->position;
    if (const auto* vehicle = vehicleFor(ref)) return vehicle->position;
    if (const auto* route = routeFor(ref)) return route->position;
    return glm::vec3(0.0f);
}

void MapEditor::setObjectPosition(const EditorObjectRef& ref, const glm::vec3& position)
{
    auto& reg = MapConfigRegistry::instance();
    if (ref.kind == EditorObjectKind::ChunkPart) {
        if (ChunkPart* part = chunkPartFor(ref)) {
            part->position = position - mChunkPreviewOrigin;
            reg.markChanged();
            refreshPreviewChunk();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::ChunkConnector) {
        if (AuthoredChunkConnector* connector = chunkConnectorFor(ref)) {
            connector->position = position - mChunkPreviewOrigin;
            reg.markChanged();
            refreshPreviewChunk();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::ChunkSpawn) {
        if (ChunkSpawnPoint* spawn = chunkSpawnFor(ref)) {
            spawn->position = position - mChunkPreviewOrigin;
            reg.markChanged();
            refreshPreviewChunk();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
        auto& payload = reg.payloadMutable();
        payload.present = true;
        payload.hasCartOrigin = true;
        payload.cartOrigin = position;
        reg.markChanged();
        return;
    }
    if (ref.kind == EditorObjectKind::BombSite) {
        reg.setSitePosition(ref.id, position);
        return;
    }
    if (ref.kind == EditorObjectKind::MapGLBObject) {
        MapGLBObject* object = mapObjectFor(ref);
        if (!object || !gpWorld) return;
        const glm::vec3 oldPosition(object->transform[3]);
        const glm::vec3 delta = position - oldPosition;
        if (glm::dot(delta, delta) <= 1e-10f) return;
        const size_t last = std::min(object->vertexFirst + object->vertexCount,
                                     gpWorld->mesh.verts.size());
        for (size_t i = object->vertexFirst; i < last; ++i)
            gpWorld->mesh.verts[i].pos += delta;
        object->transform[3] += glm::vec4(delta, 0.0f);
        object->boundsMin += delta;
        object->boundsMax += delta;
        reg.setGLBObjectOverride(object->id, object->name, position);
        mGeometryDirty = true;
        return;
    }
    if (ref.kind == EditorObjectKind::Entity) {
        if (auto* entity = reg.findEntityMutable(ref.id)) {
            entity->position = position;
            reg.markChanged();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::Block) {
        if (auto* block = reg.findBlockMutable(ref.id)) {
            const glm::vec3 delta = position - block->position;
            block->position = position;
            for (const MapAttachment& attachment : reg.attachments()) {
                if (!attachment.anchored ||
                    attachment.parentId != ref.id ||
                    attachment.childKind != "block")
                    continue;
                if (auto* child = reg.findBlockMutable(attachment.childId))
                    child->position += delta;
            }
            reg.markChanged();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::Vehicle) {
        if (auto* vehicle = reg.findVehicleMutable(ref.id)) {
            vehicle->position = position;
            reg.markChanged();
        }
        return;
    }
    if (ref.kind == EditorObjectKind::PayloadRoutePoint) {
        if (auto* route = reg.findPayloadRoutePointMutable(ref.id)) {
            route->position = position;
            reg.markChanged();
        }
    }
}

void MapEditor::syncGLBOverridesFromWorld()
{
    if (!gpWorld) return;
    auto& reg = MapConfigRegistry::instance();
    for (const MapGLBObject& object : gpWorld->mesh.objects) {
        const glm::vec3 current(object.transform[3]);
        if (glm::length(current - object.basePosition) <= 0.0001f)
            reg.removeGLBObjectOverride(object.id);
        else
            reg.setGLBObjectOverride(object.id, object.name, current);
    }
}

void MapEditor::applyGLBOverridesToWorld()
{
    if (!gpWorld) return;
    bool changed = false;
    AABB oldBounds;
    oldBounds.min = glm::vec3(std::numeric_limits<float>::max());
    oldBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (const MapGLBObject& object : gpWorld->mesh.objects) {
        glm::vec3 target = object.basePosition;
        for (const MapGLBObjectOverride& override :
             MapConfigRegistry::instance().glbObjects()) {
            if (override.id == object.id) {
                target = override.position;
                break;
            }
        }
        if (glm::length(target - glm::vec3(object.transform[3])) <= 0.0001f)
            continue;
        changed = true;
        oldBounds.min = glm::min(oldBounds.min, object.boundsMin);
        oldBounds.max = glm::max(oldBounds.max, object.boundsMax);
    }
    if (!changed) return;
    applyGLBObjectOverrides(*gpWorld, MapConfigRegistry::instance().glbObjects());
    AABB newBounds;
    newBounds.min = glm::vec3(std::numeric_limits<float>::max());
    newBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for (const MapGLBObject& object : gpWorld->mesh.objects) {
        newBounds.min = glm::min(newBounds.min, object.boundsMin);
        newBounds.max = glm::max(newBounds.max, object.boundsMax);
    }
    const bool collisionUpdated =
        updateCollisionForMovedObject(*gpWorld, oldBounds, newBounds);
    mGeometryDirty = false;
    mCollisionDirty = !collisionUpdated;
}
glm::vec3 MapEditor::selectionCenter() const
{
    if (mSelection.empty()) return glm::vec3(0.0f);
    glm::vec3 center(0.0f);
    for (const auto& ref : mSelection) center += objectPosition(ref);
    return center / (float)mSelection.size();
}

bool MapEditor::selectionHasBlocksOrRoutes() const
{
    for (const auto& ref : mSelection) {
        if (ref.kind == EditorObjectKind::Block ||
            ref.kind == EditorObjectKind::PayloadRoutePoint ||
            ref.kind == EditorObjectKind::PayloadCartOrigin ||
            ref.kind == EditorObjectKind::MapGLBObject)
            return true;
    }
    return false;
}

bool MapEditor::selectionHasBlocks() const
{
    return std::any_of(mSelection.begin(), mSelection.end(),
        [](const EditorObjectRef& ref) { return ref.kind == EditorObjectKind::Block; });
}

bool MapEditor::selectionHasGLBObjects() const
{
    return std::any_of(mSelection.begin(), mSelection.end(),
        [](const EditorObjectRef& ref) { return ref.kind == EditorObjectKind::MapGLBObject; });
}
bool MapEditor::rayFromCursor(glm::vec3& origin, glm::vec3& direction) const
{
    if (!gRenderer) return false;
    const float w = screenWidth();
    const float h = screenHeight();
    if (w <= 1.0f || h <= 1.0f) return false;

    const glm::vec2 screenCursor = cursorScreen(glfwGetCurrentContext(), mMouseX, mMouseY);
    const float mx = screenCursor.x;
    const float my = screenCursor.y;
    const float ndcX = (2.0f * mx / w) - 1.0f;
    const float ndcY = 1.0f - (2.0f * my / h);

    const glm::mat4 view = THE_CAMERA.getView();
    const glm::mat4 proj = THE_CAMERA.getProj(w, h);
    const glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 nearPoint = inv * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inv * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(nearPoint.w) < 1e-6f || std::abs(farPoint.w) < 1e-6f)
        return false;
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    origin = glm::vec3(nearPoint);
    direction = glm::normalize(glm::vec3(farPoint - nearPoint));
    return std::isfinite(direction.x) && std::isfinite(direction.y) && std::isfinite(direction.z);
}

bool MapEditor::rayAABB(glm::vec3 origin, glm::vec3 direction, const glm::vec3& mn,
                        const glm::vec3& mx, float& distance) const
{
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 1e-8f) {
            if (origin[axis] < mn[axis] || origin[axis] > mx[axis]) return false;
            continue;
        }
        const float inv = 1.0f / direction[axis];
        float t1 = (mn[axis] - origin[axis]) * inv;
        float t2 = (mx[axis] - origin[axis]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    distance = tmin;
    return tmax >= 0.0f;
}

bool MapEditor::raySphere(glm::vec3 origin, glm::vec3 direction, const glm::vec3& center,
                          float radius, float& distance) const
{
    const glm::vec3 oc = origin - center;
    const float b = glm::dot(oc, direction);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float root = std::sqrt(disc);
    float t = -b - root;
    if (t < 0.0f) t = -b + root;
    if (t < 0.0f) return false;
    distance = t;
    return true;
}

bool MapEditor::rayOBB(glm::vec3 origin, glm::vec3 direction, const glm::vec3& center,
                       const glm::vec3& size, const glm::vec3& rotationEuler,
                       float& distance) const
{
    AuthoredBlock block;
    block.position = center;
    block.size = glm::vec3(1.0f);
    block.rotationEuler = rotationEuler;
    const glm::mat4 transform = blockTransform(block);
    const glm::mat4 inv = glm::inverse(transform);
    const glm::vec3 localOrigin = glm::vec3(inv * glm::vec4(origin, 1.0f));
    const glm::vec3 localDirection = glm::normalize(glm::mat3(inv) * direction);
    const glm::vec3 half = size * 0.5f;
    return rayAABB(localOrigin, localDirection, -half, half, distance);
}

bool MapEditor::pickAtCursor(EditorObjectRef& out, float& outDistance) const
{
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f};
    if (!rayFromCursor(origin, direction)) return false;

    float best = std::numeric_limits<float>::max();
    EditorObjectRef bestRef;
    bool found = false;

    const auto& config = MapConfigRegistry::instance().current();
    for (const auto& entity : config.entities) {
        if (!entity.enabled) continue;
        const bool box = entity.size.x > 1.001f || entity.size.y > 1.001f ||
                         entity.size.z > 1.001f;
        float distance = 0.0f;
        bool hit = false;
        if (box) {
            const glm::vec3 half = entity.size * 0.5f;
            hit = rayAABB(origin, direction, entity.position - half,
                          entity.position + half, distance);
        } else {
            hit = raySphere(origin, direction, entity.position,
                            std::max(0.45f, entity.radius), distance);
        }
        if (hit && distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::Entity, entity.id};
            found = true;
        }
    }
    for (const BombSite& site : config.bombSites) {
        if (!site.hasPosition) continue;
        float distance = 0.0f;
        bool hit = false;
        if (std::abs(direction.z) > 1e-5f) {
            distance = (site.position.z - origin.z) / direction.z;
            if (distance >= 0.0f) {
                const glm::vec3 point = origin + direction * distance;
                const glm::vec2 offset(point.x - site.position.x,
                                       point.y - site.position.y);
                hit = glm::dot(offset, offset) <= site.radius * site.radius;
            }
        }
        if (!hit)
            hit = raySphere(origin, direction, site.position, 0.7f, distance);
        if (hit && distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::BombSite, site.id};
            found = true;
        }
    }
    for (const auto& block : config.blocks) {
        if (!block.enabled) continue;
        float distance = 0.0f;
        if (rayOBB(origin, direction, block.position, block.size,
                   block.rotationEuler, distance) && distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::Block, block.id};
            found = true;
        }
    }
    for (const auto& vehicle : config.vehicles) {
        if (!vehicle.enabled) continue;
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(vehicle.templateId);
        const glm::vec3 half = spec ? spec->halfExtents : glm::vec3(2.5f, 1.5f, 0.5f);
        float distance = 0.0f;
        if (rayOBB(origin, direction, vehicle.position, half * 2.0f,
                   glm::vec3(0.0f, 0.0f, glm::degrees(vehicle.yaw)), distance) &&
            distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::Vehicle, vehicle.id};
            found = true;
        }
    }
    for (const auto& route : config.payload.route) {
        float distance = 0.0f;
        if (raySphere(origin, direction, route.position,
                      std::max(0.35f, route.radius), distance) && distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::PayloadRoutePoint, route.id};
            found = true;
        }
    }
    if (config.payload.hasCartOrigin) {
        float distance = 0.0f;
        if (raySphere(origin, direction, config.payload.cartOrigin, 0.7f, distance) &&
            distance < best) {
            best = distance;
            bestRef = {EditorObjectKind::PayloadCartOrigin, "cart_origin"};
            found = true;
        }
    }
    if (gpWorld) {
        for (const auto& object : gpWorld->mesh.objects) {
            float distance = 0.0f;
            if (rayAABB(origin, direction, object.boundsMin, object.boundsMax,
                        distance) && distance < best) {
                best = distance;
                bestRef = {EditorObjectKind::MapGLBObject, object.id};
                found = true;
            }
        }
    }
    if (mChunkPreviewValid) {
        for (const auto& chunk : config.chunks) {
            if (chunk.id != mChunkPreviewChunkId) continue;
            for (const auto& part : chunk.parts) {
                if (!part.visible) continue;
                float distance = 0.0f;
                if (rayOBB(origin, direction,
                           mChunkPreviewOrigin + part.position, part.size,
                           part.rotationEuler, distance) && distance < best) {
                    best = distance;
                    bestRef = {EditorObjectKind::ChunkPart,
                               chunk.id + "/" + part.id};
                    found = true;
                }
            }
            for (const auto& connector : chunk.connectors) {
                float distance = 0.0f;
                if (raySphere(origin, direction,
                              mChunkPreviewOrigin + connector.position, 0.4f,
                              distance) && distance < best) {
                    best = distance;
                    bestRef = {EditorObjectKind::ChunkConnector,
                               chunk.id + "/" + connector.id};
                    found = true;
                }
            }
            for (const auto& spawn : chunk.spawnPoints) {
                if (!spawn.enabled) continue;
                float distance = 0.0f;
                if (raySphere(origin, direction,
                              mChunkPreviewOrigin + spawn.position,
                              std::max(0.4f, spawn.radius), distance) &&
                    distance < best) {
                    best = distance;
                    bestRef = {EditorObjectKind::ChunkSpawn,
                               chunk.id + "/" + spawn.id};
                    found = true;
                }
            }
        }
    }
    if (!found) return false;
    out = bestRef;
    outDistance = best;
    return true;
}

bool MapEditor::handleAtCursor(EditorObjectRef& out, int& axis) const
{
    if (mSelection.empty()) return false;
    const glm::vec3 center = selectionCenter();
    const glm::vec2 screenCursor = cursorScreen(glfwGetCurrentContext(), mMouseX, mMouseY);
    const float px = screenCursor.x;
    const float py = screenCursor.y;

    float cx = 0.0f, cy = 0.0f;
    if (!DebugVis::projectToScreen(THE_CAMERA, center, cx, cy)) return false;

    const glm::vec3 axes[3] = {
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
    };
    const glm::vec4 colors[3] = {
        {1, 0.2f, 0.2f, 1}, {0.2f, 1, 0.2f, 1}, {0.3f, 0.5f, 1, 1}
    };
    (void)colors;

    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    const bool allowScale = mSelection.size() == 1 &&
        (blockFor(mSelection.front()) || chunkPartFor(mSelection.front()));
    if (ModuleHost::instance().mapEditorLoaded()) {
        mimita_map_editor::GizmoPickV1 input{};
        input.cursor[0] = px;
        input.cursor[1] = py;
        input.center[0] = cx;
        input.center[1] = cy;
        input.allowScale = allowScale ? 1 : 0;
        input.pickPixels = cfg.handlePickPixels;
        bool projected = true;
        for (int i = 0; i < 3; ++i) {
            float ex = 0.0f, ey = 0.0f;
            if (!DebugVis::projectToScreenUnclamped(THE_CAMERA,
                    center + axes[i] * cfg.handleLength, ex, ey)) {
                projected = false;
                break;
            }
            input.axisEnd[i][0] = ex;
            input.axisEnd[i][1] = ey;
            float sx = cx, sy = cy;
            if (allowScale)
                DebugVis::projectToScreenUnclamped(THE_CAMERA,
                    center - axes[i] * cfg.handleLength, sx, sy);
            input.scaleEnd[i][0] = sx;
            input.scaleEnd[i][1] = sy;
        }
        if (projected) {
            int picked = -1;
            if (ModuleHost::instance().mapEditorPickGizmoAxis(&input, &picked)) {
                out = mSelection.front();
                axis = picked;
                return true;
            }
            return false;
        }
    }

    int bestAxis = -1;
    float bestDistance = cfg.handlePickPixels;
    for (int i = 0; i < 3; ++i) {
        float ex = 0.0f, ey = 0.0f;
        if (!DebugVis::projectToScreenUnclamped(THE_CAMERA, center + axes[i] * cfg.handleLength, ex, ey))
            continue;
        const float distance = pointSegmentDistance(px, py, cx, cy, ex, ey);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAxis = i;
        }
        if (mSelection.size() == 1 &&
            (blockFor(mSelection.front()) || chunkPartFor(mSelection.front()))) {
            float sx = 0.0f, sy = 0.0f;
            if (DebugVis::projectToScreenUnclamped(THE_CAMERA,
                center - axes[i] * cfg.handleLength, sx, sy)) {
                const float scaleDistance = glm::length(glm::vec2(px - sx, py - sy));
                if (scaleDistance < bestDistance) {
                    bestDistance = scaleDistance;
                    bestAxis = i + 3;
                }
            }
        }
    }
    if (bestAxis < 0) return false;
    out = mSelection.front();
    axis = bestAxis;
    return true;
}

glm::vec3 MapEditor::nearestWorldVertex(const glm::vec3& nearPoint, float maxDistance) const
{
    if (!gpWorld) return nearPoint;
    AABB query;
    query.min = nearPoint - glm::vec3(maxDistance);
    query.max = nearPoint + glm::vec3(maxDistance);
    static thread_local std::vector<int> candidates;
    candidates.clear();
    appendChunkTrianglesForAABB(*gpWorld, query, 0.0f, candidates, "map-editor-snap");
    float best = maxDistance;
    glm::vec3 bestVertex = nearPoint;
    for (int index : candidates) {
        if (index < 0 || index >= (int)gpWorld->collisionMesh.triangles.size()) continue;
        const CollisionTriangle& tri = gpWorld->collisionMesh.triangles[index];
        const glm::vec3 verts[3] = {tri.a, tri.b, tri.c};
        for (const glm::vec3& vertex : verts) {
            const float distance = glm::length(vertex - nearPoint);
            if (distance < best) {
                best = distance;
                bestVertex = vertex;
            }
        }
    }
    return bestVertex;
}

glm::vec3 MapEditor::applySnap(const glm::vec3& position)
{
    mSnapTargetValid = false;
    if (mVertexSnap) {
        const glm::vec3 vertex = nearestWorldVertex(position, kVertexSnapRadius);
        if (glm::length(vertex - position) < kVertexSnapRadius) {
            mSnapTarget = vertex;
            mSnapTargetValid = true;
            return vertex;
        }
    }
    GLFWwindow* window = glfwGetCurrentContext();
    const bool ctrl = window &&
        (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
    if (mGridSnap || ctrl) {
        const float grid = std::max(0.01f, mSnapGrid);
        const glm::vec3 snapped = glm::round(position / grid) * grid;
        mSnapTarget = snapped;
        mSnapTargetValid = true;
        return snapped;
    }
    return position;
}

void MapEditor::beginDrag(const EditorObjectRef& primary, DragMode mode,
                          const glm::vec3& startPoint)
{
    mDrag = DragState{};
    mDrag.mode = mode;
    mDrag.primary = primary;
    mDrag.startPoint = startPoint;
    mDrag.startCenter = selectionCenter();
    mDrag.startMouse = cursorScreen(glfwGetCurrentContext(), mMouseX, mMouseY);
    mDrag.refs = mSelection;
    if (mDrag.refs.empty()) mDrag.refs.push_back(primary);
    for (const auto& ref : mDrag.refs) {
        mDrag.originalPositions.push_back(objectPosition(ref));
        if (const auto* block = blockFor(ref)) mDrag.originalSizes.push_back(block->size);
        else if (const auto* part = chunkPartFor(ref)) mDrag.originalSizes.push_back(part->size);
        else mDrag.originalSizes.push_back(glm::vec3(0.0f));
        if (const auto* block = blockFor(ref))
            mDrag.originalRotations.push_back(block->rotationEuler);
        else
            mDrag.originalRotations.push_back(glm::vec3(0.0f));
    }
    if (primary.kind == EditorObjectKind::MapGLBObject)
        syncGLBOverridesFromWorld();
    mDrag.pendingUndo = MapConfigRegistry::instance().snapshotAuthored();
    mDrag.hasPendingUndo = true;
    mDrag.moved = false;
}

void MapEditor::updateDrag()
{
    if (mDrag.mode == DragMode::None) return;
    const glm::vec2 cursor = cursorScreen(glfwGetCurrentContext(), mMouseX, mMouseY);
    const glm::vec2 mouseDelta = cursor - mDrag.startMouse;
    const float viewDepth = std::max(0.1f,
        glm::dot(mDrag.startCenter - THE_CAMERA.pos, THE_CAMERA.front));
    const float halfFov = glm::radians(std::max(1.0f, THE_CAMERA.fov) * 0.5f);
    const float unitsPerPixel = 2.0f * viewDepth * std::tan(halfFov) /
        std::max(1.0f, screenHeight());

    glm::vec3 delta(0.0f);
    bool moduleHandled = false;
    if (ModuleHost::instance().mapEditorLoaded()) {
        mimita_map_editor::DragMathV1 input{};
        input.mode = mDrag.mode == DragMode::ObjectPlane
            ? 0 : static_cast<int>(mDrag.mode) - 1;
        input.mouseDelta[0] = mouseDelta.x;
        input.mouseDelta[1] = mouseDelta.y;
        input.cameraRight[0] = THE_CAMERA.right.x;
        input.cameraRight[1] = THE_CAMERA.right.y;
        input.cameraRight[2] = THE_CAMERA.right.z;
        input.cameraUp[0] = THE_CAMERA.up.x;
        input.cameraUp[1] = THE_CAMERA.up.y;
        input.cameraUp[2] = THE_CAMERA.up.z;
        input.unitsPerPixel = unitsPerPixel;
        input.handleLength = MapEditorConfig::instance().data().handleLength;

        bool haveAxisScreen = mDrag.mode == DragMode::ObjectPlane;
        if (!haveAxisScreen) {
            const glm::vec3 axis =
                mDrag.mode == DragMode::AxisX || mDrag.mode == DragMode::ScaleX
                    ? glm::vec3(1, 0, 0)
                    : mDrag.mode == DragMode::AxisY ||
                              mDrag.mode == DragMode::ScaleY
                          ? glm::vec3(0, 1, 0)
                          : glm::vec3(0, 0, 1);
            float cx = 0.0f, cy = 0.0f, ex = 0.0f, ey = 0.0f;
            const float sign =
                mDrag.mode == DragMode::ScaleX ||
                        mDrag.mode == DragMode::ScaleY ||
                        mDrag.mode == DragMode::ScaleZ
                    ? -1.0f : 1.0f;
            if (DebugVis::projectToScreen(THE_CAMERA, mDrag.startCenter, cx, cy) &&
                DebugVis::projectToScreenUnclamped(THE_CAMERA,
                    mDrag.startCenter + axis * input.handleLength * sign, ex, ey)) {
                input.axisScreen[0] = ex - cx;
                input.axisScreen[1] = ey - cy;
                haveAxisScreen = true;
            }
        }
        if (haveAxisScreen) {
            float moduleDelta[3] = {0.0f, 0.0f, 0.0f};
            moduleHandled = ModuleHost::instance().mapEditorComputeDragDelta(
                &input, moduleDelta);
            if (moduleHandled)
                delta = glm::vec3(moduleDelta[0], moduleDelta[1], moduleDelta[2]);
        }
    }
    if (!moduleHandled) {
        if (mDrag.mode == DragMode::ObjectPlane) {
            delta = THE_CAMERA.right * (mouseDelta.x * unitsPerPixel) +
                    THE_CAMERA.up * (-mouseDelta.y * unitsPerPixel);
        } else {
            const glm::vec3 axis = mDrag.mode == DragMode::AxisX || mDrag.mode == DragMode::ScaleX ? glm::vec3(1, 0, 0) :
                                   mDrag.mode == DragMode::AxisY || mDrag.mode == DragMode::ScaleY ? glm::vec3(0, 1, 0) :
                                                                   glm::vec3(0, 0, 1);
            float cx = 0.0f, cy = 0.0f, ex = 0.0f, ey = 0.0f;
            const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
            if (DebugVis::projectToScreen(THE_CAMERA, mDrag.startCenter, cx, cy) &&
                DebugVis::projectToScreenUnclamped(THE_CAMERA,
                    mDrag.startCenter + axis * cfg.handleLength *
                        (mDrag.mode == DragMode::ScaleX || mDrag.mode == DragMode::ScaleY ||
                         mDrag.mode == DragMode::ScaleZ ? -1.0f : 1.0f), ex, ey)) {
                const glm::vec2 axisScreen(ex - cx, ey - cy);
                const float axisScreenLength = glm::length(axisScreen);
                if (axisScreenLength > 1.0f) {
                    const float alongPixels = glm::dot(mouseDelta, axisScreen / axisScreenLength);
                    delta = axis * (alongPixels * cfg.handleLength / axisScreenLength);
                }
            }
        }
    }

    const bool scaling = mDrag.mode == DragMode::ScaleX ||
        mDrag.mode == DragMode::ScaleY || mDrag.mode == DragMode::ScaleZ;
    if (scaling) {
        const int axis = mDrag.mode == DragMode::ScaleX ? 0 :
            mDrag.mode == DragMode::ScaleY ? 1 : 2;
        if (std::abs(delta[axis]) > 0.0005f) mDrag.moved = true;
        for (size_t i = 0; i < mDrag.refs.size(); ++i) {
            glm::vec3 size = mDrag.originalSizes[i];
            size[axis] = std::max(0.05f, size[axis] + delta[axis] * 2.0f);
            const EditorObjectRef& ref = mDrag.refs[i];
            if (ref.kind == EditorObjectKind::Block) {
                if (auto* block = MapConfigRegistry::instance().findBlockMutable(ref.id))
                    block->size = size;
            } else if (ChunkPart* part = chunkPartFor(ref)) {
                part->size = size;
                refreshPreviewChunk();
            }
        }
        MapConfigRegistry::instance().markChanged();
        return;
    }

    glm::vec3 targetCenter = mDrag.startCenter + delta;
    targetCenter = applySnap(targetCenter);
    delta = targetCenter - mDrag.startCenter;
    if (glm::length(delta) > 0.0005f) mDrag.moved = true;

    for (size_t i = 0; i < mDrag.refs.size(); ++i) {
        const EditorObjectRef& ref = mDrag.refs[i];
        const glm::vec3 original = i < mDrag.originalPositions.size()
            ? mDrag.originalPositions[i] : objectPosition(ref);
        setObjectPosition(ref, original + delta);
    }
    MapConfigRegistry::instance().markChanged();
}

void MapEditor::endDrag(bool commit)
{
    if (mDrag.mode == DragMode::None) return;
    const bool moved = mDrag.moved;
    if (commit && moved && mDrag.primary.kind == EditorObjectKind::BombSite) {
        const glm::vec3 finalPosition = objectPosition(mDrag.primary);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.bombsite-drag.committed", mDrag.primary.id,
            "bomb site moved through viewport drag", 0,
            {{"site", mDrag.primary.id},
             {"from", {mDrag.originalPositions.front().x,
                         mDrag.originalPositions.front().y,
                         mDrag.originalPositions.front().z}},
             {"to", {finalPosition.x, finalPosition.y, finalPosition.z}}},
            __FILE__, __LINE__, __func__);
    }
    if (commit && moved && mDrag.hasPendingUndo) {
        mUndo.push_back(std::move(mDrag.pendingUndo));
        mUndoLabels.push_back("transform " + describeSelection());
        if (mUndo.size() > kMaxHistory) {
            mUndo.erase(mUndo.begin());
            mUndoLabels.erase(mUndoLabels.begin());
        }
        mRedo.clear();
        mRedoLabels.clear();
    } else if (!commit && mDrag.hasPendingUndo) {
        MapConfigRegistry::instance().restoreAuthored(mDrag.pendingUndo);
        if (selectionHasGLBObjects())
            applyGLBOverridesToWorld();
        if (selectionHasBlocks()) {
            rebuildAuthoredRenderGeometry();
            mCollisionDirty = true;
        }
        refreshPreviewChunk();
    }

    AABB movedOldBounds;
    AABB movedNewBounds;
    bool movedGLB = false;
    movedOldBounds.min = movedNewBounds.min = glm::vec3(std::numeric_limits<float>::max());
    movedOldBounds.max = movedNewBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    if (moved && commit && gpWorld) {
        for (size_t i = 0; i < mDrag.refs.size() && i < mDrag.originalPositions.size(); ++i) {
            const EditorObjectRef& ref = mDrag.refs[i];
            if (ref.kind != EditorObjectKind::MapGLBObject) continue;
            const MapGLBObject* object = mapObjectFor(ref);
            if (!object) continue;
            const glm::vec3 delta = glm::vec3(object->transform[3]) - mDrag.originalPositions[i];
            movedOldBounds.min = glm::min(movedOldBounds.min, object->boundsMin - delta);
            movedOldBounds.max = glm::max(movedOldBounds.max, object->boundsMax - delta);
            movedNewBounds.min = glm::min(movedNewBounds.min, object->boundsMin);
            movedNewBounds.max = glm::max(movedNewBounds.max, object->boundsMax);
            movedGLB = true;
        }
    }

    mDrag = DragState{};
    if (moved && commit) {
        if (movedGLB)
            updateCollisionForMovedObject(*gpWorld, movedOldBounds, movedNewBounds);
        if (selectionHasBlocks()) {
            rebuildAuthoredRenderGeometry();
            mCollisionDirty = true;
        }
        mLastEditTime = glfwGetTime();
        notify("transformed " + std::to_string(mSelection.size()) + " object(s)");
    }
}

void MapEditor::pushUndo(const std::string& label)
{
    mLastEditTime = glfwGetTime();
    mUndo.push_back(MapConfigRegistry::instance().snapshotAuthored());
    mUndoLabels.push_back(label);
    if (mUndo.size() > kMaxHistory) {
        mUndo.erase(mUndo.begin());
        mUndoLabels.erase(mUndoLabels.begin());
    }
    mRedo.clear();
    mRedoLabels.clear();
}

void MapEditor::undo()
{
    if (mUndo.empty()) {
        notify("undo: no history");
        return;
    }
    const std::string label = mUndoLabels.back();
    mRedo.push_back(MapConfigRegistry::instance().snapshotAuthored());
    mRedoLabels.push_back(label);
    MapConfigRegistry::instance().restoreAuthored(mUndo.back());
    mUndo.pop_back();
    mUndoLabels.pop_back();
    mSelection.erase(std::remove_if(mSelection.begin(), mSelection.end(),
        [&](const EditorObjectRef& ref) {
            if (ref.kind == EditorObjectKind::Entity)
                return MapConfigRegistry::instance().findEntity(ref.id) == nullptr;
            if (ref.kind == EditorObjectKind::Block)
                return MapConfigRegistry::instance().findBlock(ref.id) == nullptr;
            if (ref.kind == EditorObjectKind::PayloadRoutePoint)
                return MapConfigRegistry::instance().findPayloadRoutePoint(ref.id) == nullptr;
            return false;
        }), mSelection.end());
    applyGLBOverridesToWorld();
    if (!MapConfigRegistry::instance().blocks().empty()) {
        rebuildAuthoredRenderGeometry();
        mCollisionDirty = true;
    }
    mLastEditTime = glfwGetTime();
    notify("undo: " + label);
}

void MapEditor::redo()
{
    if (mRedo.empty()) {
        notify("redo: no history");
        return;
    }
    const std::string label = mRedoLabels.back();
    mUndo.push_back(MapConfigRegistry::instance().snapshotAuthored());
    mUndoLabels.push_back(label);
    MapConfigRegistry::instance().restoreAuthored(mRedo.back());
    mRedo.pop_back();
    mRedoLabels.pop_back();
    applyGLBOverridesToWorld();
    if (!MapConfigRegistry::instance().blocks().empty()) {
        rebuildAuthoredRenderGeometry();
        mCollisionDirty = true;
    }
    mLastEditTime = glfwGetTime();
    notify("redo: " + label);
}

std::string MapEditor::kindName(EditorObjectKind kind)
{
    switch (kind) {
        case EditorObjectKind::Entity: return "entity";
        case EditorObjectKind::Block: return "block";
        case EditorObjectKind::Vehicle: return "vehicle";
        case EditorObjectKind::BombSite: return "bombsite";
        case EditorObjectKind::PayloadRoutePoint: return "route";
        case EditorObjectKind::PayloadCartOrigin: return "cart";
        case EditorObjectKind::MapGLBObject: return "glb";
        case EditorObjectKind::ChunkPart: return "chunk_part";
        case EditorObjectKind::ChunkConnector: return "chunk_connector";
        case EditorObjectKind::ChunkSpawn: return "chunk_spawn";
    }
    return "object";
}

std::string MapEditor::describeRef(const EditorObjectRef& ref) const
{
    return kindName(ref.kind) + " " + ref.id;
}

std::string MapEditor::describeSelection() const
{
    if (mSelection.empty()) return "nothing";
    if (mSelection.size() == 1) return describeRef(mSelection.front());
    std::string out = std::to_string(mSelection.size()) + " objects: ";
    for (size_t i = 0; i < mSelection.size(); ++i) {
        if (i > 0) out += ", ";
        out += describeRef(mSelection[i]);
    }
    return out;
}

std::string MapEditor::objectName(const EditorObjectRef& ref) const
{
    if (const auto* entity = entityFor(ref)) return entity->name;
    if (const auto* block = blockFor(ref)) return block->name;
    if (const auto* vehicle = vehicleFor(ref)) return vehicle->name;
    if (const auto* route = routeFor(ref)) return route->name;
    if (ref.kind == EditorObjectKind::BombSite) {
        if (const BombSite* site = MapConfigRegistry::instance().findSite(ref.id))
            return site->name;
    }
    if (const auto* part = chunkPartFor(ref)) return part->name;
    if (const auto* connector = chunkConnectorFor(ref)) return connector->name;
    if (const auto* spawn = chunkSpawnFor(ref)) return spawn->name;
    if (const auto* object = mapObjectFor(ref)) return object->name;
    return {};
}

bool MapEditor::setSelectedName(const std::string& name)
{
    if (mSelection.empty()) {
        notify("rename: select an object first");
        return false;
    }
    const EditorObjectRef ref = mSelection.front();
    const std::string oldName = objectName(ref);
    bool changed = false;
    if (auto* entity = MapConfigRegistry::instance().findEntityMutable(ref.id);
        entity && ref.kind == EditorObjectKind::Entity) {
        entity->name = name;
        changed = true;
    }
    if (AuthoredBlock* block = MapConfigRegistry::instance().findBlockMutable(ref.id);
        block && ref.kind == EditorObjectKind::Block) {
        block->name = name;
        changed = true;
    }
    if (MapVehicleSpawn* vehicle =
            MapConfigRegistry::instance().findVehicleMutable(ref.id);
        vehicle && ref.kind == EditorObjectKind::Vehicle) {
        vehicle->name = name;
        changed = true;
    }
    if (PayloadRoutePoint* route =
            MapConfigRegistry::instance().findPayloadRoutePointMutable(ref.id);
        route && ref.kind == EditorObjectKind::PayloadRoutePoint) {
        route->name = name;
        changed = true;
    }
    if (ref.kind == EditorObjectKind::BombSite) {
        for (BombSite& site : MapConfigRegistry::instance().bombSitesMutable()) {
            if (site.id != ref.id) continue;
            site.name = name;
            changed = true;
            break;
        }
    }
    if (ChunkPart* part = chunkPartFor(ref)) {
        part->name = name;
        changed = true;
    }
    if (AuthoredChunkConnector* connector = chunkConnectorFor(ref)) {
        connector->name = name;
        changed = true;
    }
    if (ChunkSpawnPoint* spawn = chunkSpawnFor(ref)) {
        spawn->name = name;
        changed = true;
    }
    if (ref.kind == EditorObjectKind::MapGLBObject) {
        if (MapGLBObject* object = mapObjectFor(ref)) {
            object->name = name;
            MapConfigRegistry::instance().setGLBObjectOverride(
                object->id, name, glm::vec3(object->transform[3]));
            changed = true;
        }
    }
    if (!changed) {
        notify("rename: this object type has no editable name");
        return false;
    }
    MapConfigRegistry::instance().markChanged();
    mPropertyName.value = name;
    mPropertyName.cursorPos = (int)mPropertyName.value.size();
    mPropertyName.selectionStart = -1;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.rename.apply", ref.id, "object display name changed", 0,
        {{"kind", kindName(ref.kind)}, {"id", ref.id},
         {"old_name", oldName}, {"new_name", name},
         {"revision", MapConfigRegistry::instance().revision()}},
        __FILE__, __LINE__, __func__);
    notify("renamed " + ref.id + " to \"" + name + "\"");
    return true;
}

void MapEditor::beginRenameSelection()
{
    if (mSelection.empty()) {
        notify("rename: select an object first");
        return;
    }
    const EditorObjectRef ref = mSelection.front();
    mPropertyName.value = objectName(ref);
    mPropertyName.cursorPos = (int)mPropertyName.value.size();
    mPropertyName.selectionStart = 0;
    mPropertyName.focused = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.rename.begin", ref.id, "F2 started object rename", 0,
        {{"kind", kindName(ref.kind)}, {"id", ref.id},
         {"name", mPropertyName.value}},
        __FILE__, __LINE__, __func__);
    notify("rename " + ref.id + " and press Enter");
}

void MapEditor::copySelection()
{
    if (mSelection.empty()) {
        notify("copied nothing: selection is empty");
        return;
    }
    mClipboard.clear();
    for (const auto& ref : mSelection) {
        ClipboardItem item;
        item.kind = ref.kind;
        item.id = ref.id;
        if (const auto* entity = entityFor(ref)) {
            item.entity = *entity;
            item.hasEntity = true;
        } else if (const auto* block = blockFor(ref)) {
            item.block = *block;
            item.hasBlock = true;
        } else if (const auto* vehicle = vehicleFor(ref)) {
            item.vehicle = *vehicle;
            item.hasVehicle = true;
        } else if (const auto* route = routeFor(ref)) {
            item.route = *route;
            item.hasRoute = true;
        } else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
            item.cartOrigin = MapConfigRegistry::instance().payload().cartOrigin;
            item.cartYaw = MapConfigRegistry::instance().payload().cartYaw;
        }
        mClipboard.push_back(std::move(item));
    }
    notify("copied " + describeSelection());
}

void MapEditor::deleteSelection()
{
    if (mSelection.empty()) {
        notify("deleted nothing: selection is empty");
        return;
    }
    pushUndo("delete " + describeSelection());
    const std::string description = describeSelection();
    for (const auto& ref : mSelection) {
        if (ref.kind == EditorObjectKind::Entity)
            MapConfigRegistry::instance().deleteEntity(ref.id);
        else if (ref.kind == EditorObjectKind::Block)
            MapConfigRegistry::instance().deleteBlock(ref.id);
        else if (ref.kind == EditorObjectKind::Vehicle)
            MapConfigRegistry::instance().deleteVehicle(ref.id);
        else if (ref.kind == EditorObjectKind::PayloadRoutePoint)
            MapConfigRegistry::instance().deletePayloadRoutePoint(ref.id);
        else if (ref.kind == EditorObjectKind::BombSite)
            MapConfigRegistry::instance().deleteBombSite(ref.id);
        else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
            auto& payload = MapConfigRegistry::instance().payloadMutable();
            payload.hasCartOrigin = false;
            MapConfigRegistry::instance().markChanged();
        } else if (ref.kind == EditorObjectKind::ChunkPart ||
                   ref.kind == EditorObjectKind::ChunkConnector ||
                   ref.kind == EditorObjectKind::ChunkSpawn) {
            std::string chunkId;
            std::string itemId;
            if (!splitChunkRef(ref.id, chunkId, itemId)) continue;
            for (AuthoredChunk& chunk :
                 MapConfigRegistry::instance().chunksMutable()) {
                if (chunk.id != chunkId) continue;
                if (ref.kind == EditorObjectKind::ChunkSpawn) {
                    chunk.spawnPoints.erase(
                        std::remove_if(chunk.spawnPoints.begin(),
                                       chunk.spawnPoints.end(),
                                       [&](const ChunkSpawnPoint& spawn) {
                                           return spawn.id == itemId;
                                       }),
                        chunk.spawnPoints.end());
                } else if (ref.kind == EditorObjectKind::ChunkPart) {
                    chunk.parts.erase(
                        std::remove_if(chunk.parts.begin(), chunk.parts.end(),
                                       [&](const ChunkPart& part) {
                                           return part.id == itemId;
                                       }),
                        chunk.parts.end());
                } else {
                    chunk.connectors.erase(
                        std::remove_if(chunk.connectors.begin(),
                                       chunk.connectors.end(),
                                       [&](const AuthoredChunkConnector& connector) {
                                           return connector.id == itemId;
                                       }),
                        chunk.connectors.end());
                }
                MapConfigRegistry::instance().markChanged();
                refreshPreviewChunk();
                break;
            }
        }
    }
    clearSelection();
    rebuildGeometry();
    notify("deleted " + description);
}

void MapEditor::cutSelection()
{
    if (mSelection.empty()) {
        notify("cut nothing: selection is empty");
        return;
    }
    copySelection();
    const std::string description = describeSelection();
    pushUndo("cut " + description);
    for (const auto& ref : mSelection) {
        if (ref.kind == EditorObjectKind::Entity)
            MapConfigRegistry::instance().deleteEntity(ref.id);
        else if (ref.kind == EditorObjectKind::Block)
            MapConfigRegistry::instance().deleteBlock(ref.id);
        else if (ref.kind == EditorObjectKind::Vehicle)
            MapConfigRegistry::instance().deleteVehicle(ref.id);
        else if (ref.kind == EditorObjectKind::PayloadRoutePoint)
            MapConfigRegistry::instance().deletePayloadRoutePoint(ref.id);
        else if (ref.kind == EditorObjectKind::BombSite)
            MapConfigRegistry::instance().deleteBombSite(ref.id);
        else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
            auto& payload = MapConfigRegistry::instance().payloadMutable();
            payload.hasCartOrigin = false;
            MapConfigRegistry::instance().markChanged();
        }
    }
    clearSelection();
    rebuildGeometry();
    notify("cut " + description);
}

void MapEditor::pasteClipboard()
{
    if (mClipboard.empty()) {
        notify("paste failed: clipboard is empty");
        return;
    }
    pushUndo("paste " + std::to_string(mClipboard.size()) + " object(s)");
    std::vector<EditorObjectRef> pasted;
    bool addedBlock = false;
    const glm::vec3 offset(1.0f, 0.0f, 0.0f);
    for (const auto& item : mClipboard) {
        if (item.hasEntity) {
            MapEntity entity = item.entity;
            entity.id.clear();
            entity.position += offset;
            const std::string id = MapConfigRegistry::instance().createEntity(
                entity.type, entity.position);
            if (id.empty()) continue;
            MapEntity* created = MapConfigRegistry::instance().findEntityMutable(id);
            if (created) {
                const std::string keptId = created->id;
                *created = entity;
                created->id = keptId;
            }
            pasted.push_back({EditorObjectKind::Entity, id});
        } else if (item.hasBlock) {
            AuthoredBlock block = item.block;
            block.position += offset;
            const std::string id = MapConfigRegistry::instance().createBlock(
                block.position, block.size);
            if (id.empty()) continue;
            AuthoredBlock* created = MapConfigRegistry::instance().findBlockMutable(id);
            if (created) {
                const std::string keptId = created->id;
                *created = block;
                created->id = keptId;
            }
            pasted.push_back({EditorObjectKind::Block, id});
            addedBlock = true;
        } else if (item.hasVehicle) {
            MapVehicleSpawn vehicle = item.vehicle;
            vehicle.position += offset;
            const std::string id = MapConfigRegistry::instance().createVehicle(
                vehicle.templateId, vehicle.position, vehicle.yaw);
            if (id.empty()) continue;
            MapVehicleSpawn* created =
                MapConfigRegistry::instance().findVehicleMutable(id);
            if (created) {
                const std::string keptId = created->id;
                *created = vehicle;
                created->id = keptId;
            }
            pasted.push_back({EditorObjectKind::Vehicle, id});
        } else if (item.hasRoute) {
            PayloadRoutePoint route = item.route;
            route.position += offset;
            const std::string id = MapConfigRegistry::instance().createPayloadRoutePoint(
                route.position);
            if (id.empty()) continue;
            PayloadRoutePoint* created =
                MapConfigRegistry::instance().findPayloadRoutePointMutable(id);
            if (created) {
                const std::string keptId = created->id;
                *created = route;
                created->id = keptId;
            }
            pasted.push_back({EditorObjectKind::PayloadRoutePoint, id});
        } else if (item.kind == EditorObjectKind::PayloadCartOrigin) {
            auto& payload = MapConfigRegistry::instance().payloadMutable();
            payload.present = true;
            payload.hasCartOrigin = true;
            payload.cartOrigin = item.cartOrigin + offset;
            payload.cartYaw = item.cartYaw;
            MapConfigRegistry::instance().markChanged();
            pasted.push_back({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
        }
    }
    MapConfigRegistry::instance().renumberPayloadRoute();
    mSelection = pasted;
    rebuildGeometry();
    std::string message = "pasted " + std::to_string(pasted.size()) + " objects: ";
    for (size_t i = 0; i < pasted.size(); ++i) {
        if (i > 0) message += ", ";
        message += pasted[i].id;
    }
    notify(message);
}

void MapEditor::duplicateSelection()
{
    if (mSelection.empty()) {
        notify("duplicate failed: selection is empty");
        return;
    }
    const auto savedClipboard = mClipboard;
    const auto savedSelection = mSelection;
    copySelection();
    mSelection = savedSelection;
    pasteClipboard();
    mClipboard = savedClipboard;
}

void MapEditor::collectEditorObjects(std::vector<EditorObjectRef>& out) const
{
    out.clear();
    const auto& config = MapConfigRegistry::instance().current();
    std::string filter = mOutlinerFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    auto matches = [&](const std::string& text) {
        if (filter.empty()) return true;
        std::string haystack = text;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return haystack.find(filter) != std::string::npos;
    };
    if (const auto& payload = config.payload; payload.hasCartOrigin &&
        matches("cart_origin cart"))
        out.push_back({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
    for (const auto& site : config.bombSites) {
        if (matches(site.id + " " + site.name + " bombsite site"))
            out.push_back({EditorObjectKind::BombSite, site.id});
    }
    for (const auto& route : config.payload.route) {
        if (matches(route.id + " " + route.name + " route"))
            out.push_back({EditorObjectKind::PayloadRoutePoint, route.id});
    }
    if (gpWorld) {
        for (const auto& object : gpWorld->mesh.objects) {
            if (matches(object.id + " " + object.name + " glb"))
                out.push_back({EditorObjectKind::MapGLBObject, object.id});
        }
    }
    for (const auto& entity : config.entities) {
        if (entity.enabled &&
            matches(entity.id + " " + entity.name + " " + entity.type + " " +
                    entity.tag + " entity"))
            out.push_back({EditorObjectKind::Entity, entity.id});
    }
    for (const auto& block : config.blocks) {
        if (block.enabled && matches(block.id + " " + block.name + " block"))
            out.push_back({EditorObjectKind::Block, block.id});
    }
    for (const auto& vehicle : config.vehicles) {
        if (vehicle.enabled &&
            matches(vehicle.id + " " + vehicle.name + " " + vehicle.templateId +
                    " vehicle car"))
            out.push_back({EditorObjectKind::Vehicle, vehicle.id});
    }
    for (const auto& chunk : config.chunks) {
        if (!chunk.enabled) continue;
        for (const auto& part : chunk.parts) {
            const std::string id = chunk.id + "/" + part.id;
            if (matches(id + " " + part.name + " " + part.kind + " " +
                        part.material + " chunk part primitive"))
                out.push_back({EditorObjectKind::ChunkPart, id});
        }
        for (const auto& connector : chunk.connectors) {
            const std::string id = chunk.id + "/" + connector.id;
            if (matches(id + " " + connector.name + " " + connector.side + " " +
                        connector.role + " " + connector.orientation +
                        " chunk connector"))
                out.push_back({EditorObjectKind::ChunkConnector, id});
        }
        for (const auto& spawn : chunk.spawnPoints) {
            const std::string id = chunk.id + "/" + spawn.id;
            if (matches(id + " " + spawn.name + " " + spawn.allowedActors +
                        " chunk spawn spawnpoint"))
                out.push_back({EditorObjectKind::ChunkSpawn, id});
        }
    }
}

int MapEditor::outlinerRowAt(double screenX, double screenY) const
{
    const glm::vec2 cursor((float)screenX, (float)screenY);
    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    const float x = screenWidth() - cfg.outlinerWidth;
    const float y = cfg.outlinerTop;
    const float panelHeight = screenHeight() - y - cfg.outlinerBottomPadding;
    const float listY = y + kOutlinerHeaderHeight;
    const float listH = std::max(40.0f,
        panelHeight - kOutlinerHeaderHeight - cfg.propertiesHeight - kOutlinerListGap);
    if (cursor.x < x + 4.0f || cursor.x > x + cfg.outlinerWidth - 12.0f ||
        cursor.y < listY || cursor.y > listY + listH)
        return -1;
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    const int row = (int)std::floor(
        (cursor.y - listY + mOutlinerScroll.scrollY) / cfg.rowHeight);
    return (row >= 0 && row < (int)objects.size()) ? row : -1;
}

bool MapEditor::wantsTextInput() const
{
    if (!mActive) return false;
    if (mOutlinerSearchFocused) return true;
    if (mPropertyName.focused) return true;
    for (const UITextInputState& field : mPropertyPosition)
        if (field.focused) return true;
    for (const UITextInputState& field : mPropertySize)
        if (field.focused) return true;
    for (const UITextInputState& field : mPropertyRotation)
        if (field.focused) return true;
    for (const UITextInputState& field : mPropertyScale)
        if (field.focused) return true;
    if (mPropertyMaterial.focused) return true;
    for (const UITextInputState& field : mPropertyUvScale)
        if (field.focused) return true;
    return false;
}

void MapEditor::syncChunkPropertyFields()
{
    if (!mSelection.empty()) {
        const EditorObjectRef& ref = mSelection.front();
        const bool ownerChanged =
            !mPropertyOwnerValid || !(mPropertyOwner == ref);
        if (ownerChanged) {
            mPropertyMaterial.focused = false;
            for (UITextInputState& field : mPropertyUvScale)
                field.focused = false;
        }
    }
    if (mSelection.empty() ||
        mSelection.front().kind != EditorObjectKind::ChunkPart) {
        mPropertyMaterial.focused = false;
        for (UITextInputState& field : mPropertyUvScale) field.focused = false;
        return;
    }
    ChunkPart* part = chunkPartFor(mSelection.front());
    if (!part) return;
    bool anyFocused = mPropertyMaterial.focused;
    for (const UITextInputState& field : mPropertyUvScale)
        anyFocused = anyFocused || field.focused;
    if (anyFocused) return;
    if (mPropertyMaterial.value != part->material) {
        mPropertyMaterial.value = part->material;
        mPropertyMaterial.cursorPos = (int)mPropertyMaterial.value.size();
        mPropertyMaterial.selectionStart = -1;
    }
    char value[32];
    std::snprintf(value, sizeof(value), "%.3f", part->uvScale.x);
    if (mPropertyUvScale[0].value != value) {
        mPropertyUvScale[0].value = value;
        mPropertyUvScale[0].cursorPos = (int)mPropertyUvScale[0].value.size();
        mPropertyUvScale[0].selectionStart = -1;
    }
    std::snprintf(value, sizeof(value), "%.3f", part->uvScale.y);
    if (mPropertyUvScale[1].value != value) {
        mPropertyUvScale[1].value = value;
        mPropertyUvScale[1].cursorPos = (int)mPropertyUvScale[1].value.size();
        mPropertyUvScale[1].selectionStart = -1;
    }
}

void MapEditor::applyChunkMaterial()
{
    setSelectedChunkPartMaterial(mPropertyMaterial.value);
}

void MapEditor::applyChunkUvScale()
{
    float u = 1.0f;
    float v = 1.0f;
    try {
        u = std::stof(mPropertyUvScale[0].value);
        v = std::stof(mPropertyUvScale[1].value);
    } catch (const std::exception&) {
        notify("invalid uv scale");
        return;
    }
    setSelectedChunkPartUvScale(u, v);
}

bool MapEditor::setSelectedChunkPartMaterial(const std::string& material)
{
    if (mSelection.empty()) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.chunk-part-material-rejected", "",
            "material edit rejected because selection is empty", 0,
            {{"selection", ""}}, __FILE__, __LINE__, __func__);
        return false;
    }
    ChunkPart* part = chunkPartFor(mSelection.front());
    AuthoredChunk* chunk = chunkFor(mSelection.front());
    if (!part || !chunk) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.chunk-part-material-rejected", mSelection.front().id,
            "material edit rejected because the selection is not a chunk part",
            0,
            {{"selection", mSelection.front().id}},
            __FILE__, __LINE__, __func__);
        notify("select a chunk part first");
        return false;
    }
    std::string chunkId;
    std::string partId;
    splitChunkRef(mSelection.front().id, chunkId, partId);
    StreamedWorldLayer::instance().updateMaterial(chunkId, partId, -1,
                                                  material);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-part-material", chunkId,
        "chunk part material edited in the map editor", 0,
        {{"chunk", chunkId}, {"part", partId}, {"material", material}},
        __FILE__, __LINE__, __func__);
    notify("material applied to " + partId);
    return true;
}

bool MapEditor::setSelectedChunkPartUvScale(float u, float v)
{
    if (mSelection.empty()) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.chunk-part-uv-rejected", "",
            "UV edit rejected because selection is empty", 0,
            {{"selection", ""}}, __FILE__, __LINE__, __func__);
        return false;
    }
    ChunkPart* part = chunkPartFor(mSelection.front());
    AuthoredChunk* chunk = chunkFor(mSelection.front());
    if (!part || !chunk) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.chunk-part-uv-rejected", mSelection.front().id,
            "UV edit rejected because the selection is not a chunk part", 0,
            {{"selection", mSelection.front().id}},
            __FILE__, __LINE__, __func__);
        notify("select a chunk part first");
        return false;
    }
    part->uvScale = glm::vec2(std::max(0.001f, u), std::max(0.001f, v));
    MapConfigRegistry::instance().markChanged();
    refreshPreviewChunk();
    std::string chunkId;
    std::string partId;
    splitChunkRef(mSelection.front().id, chunkId, partId);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-part-uv", chunkId,
        "chunk part UV scale edited in the map editor", 0,
        {{"chunk", chunkId}, {"part", partId}, {"u", u}, {"v", v}},
        __FILE__, __LINE__, __func__);
    notify("uv scale applied to " + partId);
    return true;
}

bool MapEditor::setSelectedSize(const glm::vec3& size)
{
    if (mSelection.size() != 1 || !std::isfinite(size.x) ||
        !std::isfinite(size.y) || !std::isfinite(size.z) ||
        glm::any(glm::lessThanEqual(size, glm::vec3(0.01f)))) {
        notify("select one block or chunk part and use positive dimensions");
        return false;
    }
    const EditorObjectRef ref = mSelection.front();
    if (ChunkPart* part = chunkPartFor(ref)) {
        pushUndo("resize chunk part");
        part->size = size;
        MapConfigRegistry::instance().markChanged();
        refreshPreviewChunk();
    } else if (ref.kind == EditorObjectKind::Block) {
        AuthoredBlock* block = MapConfigRegistry::instance().findBlockMutable(ref.id);
        if (!block) return false;
        pushUndo("resize block");
        block->size = size;
        MapConfigRegistry::instance().markChanged();
        rebuildGeometry();
    } else {
        notify("select a block or chunk part to resize");
        return false;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.object-resized", ref.id, "editor dimensions changed", 0,
        {{"object", ref.id}, {"size", {size.x, size.y, size.z}}},
        __FILE__, __LINE__, __func__);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.transform.apply", ref.id,
        "editor scale applied", 0,
        {{"kind", kindName(ref.kind)}, {"id", ref.id},
         {"field", "scale"}, {"after", {size.x, size.y, size.z}},
         {"source", "field"}},
        __FILE__, __LINE__, __func__);
    notify("resized " + ref.id);
    return true;
}

bool MapEditor::setSelectedRotation(const glm::vec3& rotation)
{
    if (mSelection.empty()) {
        notify("select an object first");
        return false;
    }
    if (!setObjectRotation(mSelection.front(), rotation)) {
        notify("rotation is not editable for this object");
        return false;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.transform.apply", mSelection.front().id,
        "editor rotation applied", 0,
        {{"kind", kindName(mSelection.front().kind)},
         {"id", mSelection.front().id}, {"field", "rotation"},
         {"after", {rotation.x, rotation.y, rotation.z}},
         {"source", "field"}},
        __FILE__, __LINE__, __func__);
    notify("rotation applied");
    return true;
}

bool MapEditor::setSelectedScale(const glm::vec3& scale)
{
    if (mSelection.empty()) {
        notify("select an object first");
        return false;
    }
    if (!setObjectScale(mSelection.front(), scale)) {
        notify("scale is not editable for this object");
        return false;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.transform.apply", mSelection.front().id,
        "editor scale applied", 0,
        {{"kind", kindName(mSelection.front().kind)},
         {"id", mSelection.front().id}, {"field", "scale"},
         {"after", {scale.x, scale.y, scale.z}}, {"source", "field"}},
        __FILE__, __LINE__, __func__);
    notify("scale applied");
    return true;
}

bool MapEditor::setObjectRotation(const EditorObjectRef& ref,
                                  const glm::vec3& rotation)
{
    if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
        !std::isfinite(rotation.z))
        return false;
    auto& reg = MapConfigRegistry::instance();
    if (AuthoredBlock* block = reg.findBlockMutable(ref.id);
        block && ref.kind == EditorObjectKind::Block) {
        block->rotationEuler = rotation;
    } else if (ChunkPart* part = chunkPartFor(ref)) {
        part->rotationEuler = rotation;
        refreshPreviewChunk();
    } else if (MapEntity* entity = reg.findEntityMutable(ref.id);
               entity && ref.kind == EditorObjectKind::Entity) {
        entity->yaw = rotation.z;
    } else if (MapVehicleSpawn* vehicle = reg.findVehicleMutable(ref.id);
               vehicle && ref.kind == EditorObjectKind::Vehicle) {
        vehicle->yaw = rotation.z;
    } else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
        reg.payloadMutable().cartYaw = rotation.z;
    } else {
        return false;
    }
    reg.markChanged();
    return true;
}

bool MapEditor::setObjectScale(const EditorObjectRef& ref,
                               const glm::vec3& scale)
{
    if (!std::isfinite(scale.x) || !std::isfinite(scale.y) ||
        !std::isfinite(scale.z) ||
        glm::any(glm::lessThanEqual(scale, glm::vec3(0.01f))))
        return false;
    auto& reg = MapConfigRegistry::instance();
    if (AuthoredBlock* block = reg.findBlockMutable(ref.id);
        block && ref.kind == EditorObjectKind::Block) {
        block->size = scale;
    } else if (ChunkPart* part = chunkPartFor(ref)) {
        part->size = scale;
        refreshPreviewChunk();
    } else if (MapEntity* entity = reg.findEntityMutable(ref.id);
               entity && ref.kind == EditorObjectKind::Entity) {
        entity->size = scale;
    } else if (AuthoredChunkConnector* connector = chunkConnectorFor(ref)) {
        connector->radius = std::max(0.01f, scale.x);
    } else if (ChunkSpawnPoint* spawn = chunkSpawnFor(ref)) {
        spawn->radius = std::max(0.01f, scale.x);
    } else if (ref.kind == EditorObjectKind::BombSite) {
        reg.setSiteRadius(ref.id, std::max(0.01f, scale.x));
        return true;
    } else {
        return false;
    }
    reg.markChanged();
    return true;
}

bool MapEditor::setSelectedChunkConnectorField(const std::string& field,
                                               const std::string& value)
{
    if (mSelection.empty()) return false;
    AuthoredChunkConnector* connector = chunkConnectorFor(mSelection.front());
    if (!connector) {
        notify("select a chunk connector first");
        return false;
    }
    if (field == "side") connector->side = value;
    else if (field == "role") connector->role = value;
    else if (field == "orientation") connector->orientation = value;
    else if (field == "tag") {
        if (std::find(connector->tags.begin(), connector->tags.end(), value) ==
            connector->tags.end())
            connector->tags.push_back(value);
    } else {
        notify("connector field expects side, role, orientation, or tag");
        return false;
    }
    MapConfigRegistry::instance().markChanged();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-connector-edit", connector->id,
        "chunk connector edited in the map editor", 0,
        {{"connector", connector->id}, {"field", field}, {"value", value}},
        __FILE__, __LINE__, __func__);
    notify("connector " + field + " set");
    return true;
}

bool MapEditor::handleChar(unsigned int codepoint)
{
    if (!mActive) return false;
    if (mOutlinerSearchFocused) {
        UITextInputOptions options;
        options.maxLength = 80;
        options.placeholder = "search objects";
        options.submitOnEnter = false;
        if (!uiTextInputHandleChar(mOutlinerSearch, codepoint, options)) return true;
        mOutlinerFilter = mOutlinerSearch.value;
        mOutlinerScroll.scrollY = 0.0f;
        return true;
    }
    if (mPropertyName.focused) {
        UITextInputOptions options;
        options.maxLength = 64;
        options.submitOnEnter = false;
        uiTextInputHandleChar(mPropertyName, codepoint, options);
        return true;
    }
    for (UITextInputState& field : mPropertyPosition) {
        if (!field.focused) continue;
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleChar(field, codepoint, options);
        return true;
    }
    for (UITextInputState& field : mPropertySize) {
        if (!field.focused) continue;
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleChar(field, codepoint, options);
        return true;
    }
    for (UITextInputState& field : mPropertyRotation) {
        if (!field.focused) continue;
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleChar(field, codepoint, options);
        return true;
    }
    for (UITextInputState& field : mPropertyScale) {
        if (!field.focused) continue;
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleChar(field, codepoint, options);
        return true;
    }
    if (mPropertyMaterial.focused) {
        UITextInputOptions options;
        options.maxLength = 64;
        options.submitOnEnter = false;
        uiTextInputHandleChar(mPropertyMaterial, codepoint, options);
        return true;
    }
    for (UITextInputState& field : mPropertyUvScale) {
        if (!field.focused) continue;
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleChar(field, codepoint, options);
        return true;
    }
    return false;
}

bool MapEditor::handleTextKey(int key, int mods, int action)
{
    if (!mActive) return false;
    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    if (mOutlinerSearchFocused) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            mOutlinerSearchFocused = false;
            mOutlinerSearch.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 80;
        options.placeholder = "search objects";
        uiTextInputHandleKey(glfwGetCurrentContext(), mOutlinerSearch, key, action, mods, options);
        mOutlinerFilter = mOutlinerSearch.value;
        mOutlinerScroll.scrollY = 0.0f;
        return true;
    }
    if (mPropertyName.focused) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            mPropertyName.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 64;
        options.submitOnEnter = false;
        uiTextInputHandleKey(glfwGetCurrentContext(), mPropertyName, key, action,
                             mods, options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            mPropertyName.focused = false;
            setSelectedName(mPropertyName.value);
        }
        return true;
    }
    for (UITextInputState& field : mPropertyPosition) {
        if (!field.focused) continue;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            field.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleKey(glfwGetCurrentContext(), field, key, action, mods, options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            for (UITextInputState& other : mPropertyPosition) other.focused = false;
            applyPropertyPosition();
        }
        return true;
    }
    for (UITextInputState& field : mPropertySize) {
        if (!field.focused) continue;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            field.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleKey(glfwGetCurrentContext(), field, key, action, mods, options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && action == GLFW_PRESS) {
            for (UITextInputState& other : mPropertySize) other.focused = false;
            applyPropertySize();
        }
        return true;
    }
    for (UITextInputState& field : mPropertyRotation) {
        if (!field.focused) continue;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            field.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleKey(glfwGetCurrentContext(), field, key, action, mods,
                             options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            for (UITextInputState& other : mPropertyRotation) other.focused = false;
            applyPropertyRotation();
        }
        return true;
    }
    for (UITextInputState& field : mPropertyScale) {
        if (!field.focused) continue;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            field.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleKey(glfwGetCurrentContext(), field, key, action, mods,
                             options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            for (UITextInputState& other : mPropertyScale) other.focused = false;
            applyPropertyScale();
        }
        return true;
    }
    if (mPropertyMaterial.focused) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            mPropertyMaterial.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 64;
        options.submitOnEnter = false;
        uiTextInputHandleKey(glfwGetCurrentContext(), mPropertyMaterial, key,
                             action, mods, options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            mPropertyMaterial.focused = false;
            applyChunkMaterial();
        }
        return true;
    }
    for (UITextInputState& field : mPropertyUvScale) {
        if (!field.focused) continue;
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && !ctrl) {
            field.focused = false;
            return true;
        }
        UITextInputOptions options;
        options.maxLength = 24;
        options.submitOnEnter = false;
        options.characterFilter = isNumberChar;
        uiTextInputHandleKey(glfwGetCurrentContext(), field, key, action, mods,
                             options);
        if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
            action == GLFW_PRESS) {
            for (UITextInputState& other : mPropertyUvScale) other.focused = false;
            applyChunkUvScale();
        }
        return true;
    }
    return false;
}

void MapEditor::syncPropertyPositionFields()
{
    if (mSelection.empty()) {
        if (mPropertyOwnerValid) {
            mPropertyOwnerValid = false;
            mPropertyName.focused = false;
            for (UITextInputState& field : mPropertyPosition) field.focused = false;
            for (UITextInputState& field : mPropertySize) field.focused = false;
            for (UITextInputState& field : mPropertyRotation) field.focused = false;
            for (UITextInputState& field : mPropertyScale) field.focused = false;
            mPropertyMaterial.focused = false;
            for (UITextInputState& field : mPropertyUvScale) field.focused = false;
        }
        return;
    }
    const EditorObjectRef& ref = mSelection.front();
    const bool ownerChanged = !mPropertyOwnerValid || !(mPropertyOwner == ref);
    if (ownerChanged) {
        mPropertyOwner = ref;
        mPropertyOwnerValid = true;
        mPropertyName.focused = false;
        for (UITextInputState& field : mPropertyPosition) field.focused = false;
        for (UITextInputState& field : mPropertySize) field.focused = false;
        for (UITextInputState& field : mPropertyRotation) field.focused = false;
        for (UITextInputState& field : mPropertyScale) field.focused = false;
    }
    if (!ownerChanged) {
        bool anyFocused = false;
        for (const UITextInputState& field : mPropertyPosition)
            anyFocused = anyFocused || field.focused;
        if (anyFocused) return;
    }
    const glm::vec3 position = objectPosition(ref);
    for (int i = 0; i < 3; ++i) {
        UITextInputState& field = mPropertyPosition[i];
        if (field.focused) continue;
        char value[32];
        std::snprintf(value, sizeof(value), "%.3f", position[i]);
        if (field.value != value) {
            field.value = value;
            field.cursorPos = (int)field.value.size();
            field.selectionStart = -1;
        }
    }
    syncPropertyTransformFields();
}

void MapEditor::syncPropertyTransformFields()
{
    if (mSelection.empty()) return;
    const EditorObjectRef& ref = mSelection.front();
    glm::vec3 rotation(0.0f);
    glm::vec3 scale(1.0f);
    bool haveRotation = false;
    bool haveScale = false;
    if (const AuthoredBlock* block = blockFor(ref)) {
        rotation = block->rotationEuler;
        scale = block->size;
        haveRotation = true;
        haveScale = true;
    } else if (const ChunkPart* part = chunkPartFor(ref)) {
        rotation = part->rotationEuler;
        scale = part->size;
        haveRotation = true;
        haveScale = true;
    } else if (const MapEntity* entity = entityFor(ref)) {
        rotation.z = entity->yaw;
        scale = entity->size;
        haveRotation = true;
        haveScale = true;
    } else if (const MapVehicleSpawn* vehicle = vehicleFor(ref)) {
        rotation.z = vehicle->yaw;
        haveRotation = true;
    } else if (const PayloadRoutePoint* route = routeFor(ref)) {
        scale = glm::vec3(std::max(0.01f, route->radius));
        haveScale = true;
    } else if (ref.kind == EditorObjectKind::BombSite) {
        if (const BombSite* site = MapConfigRegistry::instance().findSite(ref.id)) {
            scale = glm::vec3(std::max(0.01f, site->radius));
            haveScale = true;
        }
    } else if (const AuthoredChunkConnector* connector = chunkConnectorFor(ref)) {
        scale = glm::vec3(std::max(0.01f, connector->radius));
        haveScale = true;
    } else if (const ChunkSpawnPoint* spawn = chunkSpawnFor(ref)) {
        scale = glm::vec3(std::max(0.01f, spawn->radius));
        haveScale = true;
    } else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
        rotation.z = MapConfigRegistry::instance().payload().cartYaw;
        haveRotation = true;
    } else if (const MapGLBObject* object = mapObjectFor(ref)) {
        const glm::mat3 basis(object->transform);
        scale = glm::vec3(glm::length(basis[0]), glm::length(basis[1]),
                          glm::length(basis[2]));
        glm::mat3 rotationBasis = basis;
        for (int i = 0; i < 3; ++i)
            if (scale[i] > 1e-5f) rotationBasis[i] /= scale[i];
        rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(rotationBasis)));
        haveRotation = true;
        haveScale = true;
    }
    if (!mPropertyName.focused) {
        const std::string name = objectName(ref);
        if (mPropertyName.value != name) {
            mPropertyName.value = name;
            mPropertyName.cursorPos = (int)name.size();
            mPropertyName.selectionStart = -1;
        }
    }
    for (int i = 0; i < 3; ++i) {
        char value[32];
        if (!mPropertyRotation[i].focused) {
            std::snprintf(value, sizeof(value), "%.2f", rotation[i]);
            if (haveRotation && mPropertyRotation[i].value != value) {
                mPropertyRotation[i].value = value;
                mPropertyRotation[i].cursorPos = (int)mPropertyRotation[i].value.size();
                mPropertyRotation[i].selectionStart = -1;
            }
        }
        if (!mPropertyScale[i].focused) {
            std::snprintf(value, sizeof(value), "%.3f", scale[i]);
            if (haveScale && mPropertyScale[i].value != value) {
                mPropertyScale[i].value = value;
                mPropertyScale[i].cursorPos = (int)mPropertyScale[i].value.size();
                mPropertyScale[i].selectionStart = -1;
            }
        }
    }
}

void MapEditor::applyPropertyRotation()
{
    if (mSelection.empty()) return;
    glm::vec3 rotation(0.0f);
    try {
        for (int i = 0; i < 3; ++i)
            if (!mPropertyRotation[i].value.empty())
                rotation[i] = std::stof(mPropertyRotation[i].value);
    } catch (const std::exception&) {
        notify("invalid rotation value");
        return;
    }
    if (setObjectRotation(mSelection.front(), rotation))
        notify("rotation applied");
    else
        notify("rotation is not editable for this object");
    syncPropertyPositionFields();
}

void MapEditor::applyPropertyScale()
{
    if (mSelection.empty()) return;
    glm::vec3 scale(1.0f);
    try {
        for (int i = 0; i < 3; ++i)
            if (!mPropertyScale[i].value.empty())
                scale[i] = std::stof(mPropertyScale[i].value);
    } catch (const std::exception&) {
        notify("invalid scale value");
        return;
    }
    if (setObjectScale(mSelection.front(), scale))
        notify("scale applied");
    else
        notify("scale is not editable for this object");
    syncPropertyPositionFields();
}

void MapEditor::applyPropertyPosition()
{
    if (mSelection.empty() || !mPropertyOwnerValid ||
        !(mPropertyOwner == mSelection.front())) {
        notify("position edit has no owner");
        return;
    }
    glm::vec3 position = objectPosition(mSelection.front());
    for (int i = 0; i < 3; ++i) {
        const std::string& text = mPropertyPosition[i].value;
        if (text.empty()) continue;
        try {
            position[i] = std::stof(text);
        } catch (const std::exception&) {
            notify("invalid position value");
            return;
        }
    }
    setSelectedPosition(position);
    syncPropertyPositionFields();
}

void MapEditor::applyPropertySize()
{
    if (mSelection.size() != 1 || !mPropertyOwnerValid ||
        !(mPropertyOwner == mSelection.front())) return;
    const EditorObjectRef& ref = mSelection.front();
    const AuthoredBlock* block = blockFor(ref);
    const ChunkPart* part = chunkPartFor(ref);
    if (!block && !part) return;
    glm::vec3 size = block ? block->size : part->size;
    try {
        for (int i = 0; i < 3; ++i)
            if (!mPropertySize[i].value.empty()) size[i] = std::stof(mPropertySize[i].value);
    } catch (const std::exception&) {
        notify("invalid size value");
        return;
    }
    setSelectedSize(size);
    syncPropertyPositionFields();
}

void MapEditor::tick(float dt)
{
    (void)dt;
    if (!mActive) return;
    const double now = glfwGetTime();
    if (mLastEditTime <= 0.0) mLastEditTime = now;
    const int delayMs = MapEditorConfig::instance().data().autosaveDelayMs;
    if (delayMs <= 0) return;
    if (mDrag.mode != DragMode::None || mOutlinerSearchFocused) return;
    if (!MapConfigRegistry::instance().dirty()) return;
    if (now - mLastEditTime < (double)delayMs / 1000.0) return;
    if (now - mLastAutosaveAttempt < (double)delayMs / 1000.0) return;
    autosave();
}

bool MapEditor::autosave()
{
    if (!MapConfigRegistry::instance().dirty()) return true;
    mLastAutosaveAttempt = glfwGetTime();
    const auto started = std::chrono::steady_clock::now();
    const bool ok = MapConfigRegistry::instance().save(false);
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    if (ok) {
        mStatus = "autosaved " + MapConfigRegistry::instance().configPath();
        mLastMessageTime = glfwGetTime();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.autosaved", MapConfigRegistry::instance().mapId(),
            "debounced autosave wrote map JSON", 0,
            {{"map", MapConfigRegistry::instance().mapId()},
             {"path", MapConfigRegistry::instance().configPath()},
             {"duration_ms", ms},
             {"revision", MapConfigRegistry::instance().revision()}},
            __FILE__, __LINE__, __FUNCTION__);
    } else {
        notify("autosave failed: " + MapConfigRegistry::instance().configPath());
    }
    return ok;
}

void MapEditor::flushCollisionNow()
{
    if (!mCollisionDirty || !gpWorld) return;
    rebuildGeometry();
    notify("collision rebuilt");
}

float MapEditor::cameraSpeedMultiplier() const
{
    return mCameraSpeedMultiplier > 0.0f
        ? mCameraSpeedMultiplier
        : MapEditorConfig::instance().data().cameraSpeedMultiplier;
}

void MapEditor::setCameraSpeedMultiplier(float multiplier)
{
    mCameraSpeedMultiplier = std::clamp(multiplier, 0.01f, 1000.0f);
}

void MapEditor::updateInput(GLFWwindow* window, float dt)
{
    (void)dt;
    if (!window) return;
    if (!mActive) {
        if (mLooking) {
            mLooking = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        return;
    }

    const bool blocked = Terminal::instance().isOpen() || PauseMenu::isOpen() || isChatOpen();
    if (blocked) {
        if (mLooking) {
            mLooking = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            THE_CAMERA.firstMouse = true;
        }
        return;
    }

    glfwGetCursorPos(window, &mMouseX, &mMouseY);
    int windowW = 0, windowH = 0, framebufferW = 0, framebufferH = 0;
    glfwGetWindowSize(window, &windowW, &windowH);
    glfwGetFramebufferSize(window, &framebufferW, &framebufferH);
    glm::vec2 screenCursor = cursorScreen(window, mMouseX, mMouseY);
    if (ModuleHost::instance().mapEditorLoaded()) {
        mimita_map_editor::EditorInputV1 input{};
        input.rawCursor[0] = (float)mMouseX;
        input.rawCursor[1] = (float)mMouseY;
        input.windowSize[0] = windowW;
        input.windowSize[1] = windowH;
        input.framebufferSize[0] = framebufferW;
        input.framebufferSize[1] = framebufferH;
        input.sceneSize[0] = (int)screenWidth();
        input.sceneSize[1] = (int)screenHeight();
        input.uiSize[0] = (int)uiScreenW();
        input.uiSize[1] = (int)uiScreenH();
        input.stretchedWholeFrame =
            gRenderer && gRenderer->stretchesWholeFrame() ? 1 : 0;
        if (ModuleHost::instance().mapEditorMapCursor(&input)) {
            screenCursor = glm::vec2(input.mappedCursor[0],
                                     input.mappedCursor[1]);
        }
    }
    const MapEditorConfigData& editorCfg = MapEditorConfig::instance().data();
    {
        static double lastCursorMapLog = -1.0;
        const double now = glfwGetTime();
        if (lastCursorMapLog < 0.0 || now - lastCursorMapLog >= 1.0) {
            lastCursorMapLog = now;
            float gizmoX = 0.0f, gizmoY = 0.0f;
            bool hasGizmo = false;
            nlohmann::json axisScreens = nlohmann::json::array();
            if (!mSelection.empty()) {
                const glm::vec3 center = selectionCenter();
                hasGizmo = DebugVis::projectToScreen(
                    THE_CAMERA, center, gizmoX, gizmoY);
                const glm::vec3 axes[3] = {
                    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
                for (int i = 0; i < 3; ++i) {
                    float ex = 0.0f, ey = 0.0f;
                    const bool ok = DebugVis::projectToScreenUnclamped(
                        THE_CAMERA,
                        center + axes[i] * editorCfg.handleLength, ex, ey);
                    axisScreens.push_back(
                        {{"axis", i}, {"ok", ok}, {"x", ex}, {"y", ey}});
                }
            }
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map-editor.cursor.map", mapId(),
                "editor cursor mapping between window and scene coordinates", 0,
                nlohmann::json{
                    {"raw_window_cursor", {mMouseX, mMouseY}},
                    {"mapped_scene_cursor", {screenCursor.x, screenCursor.y}},
                    {"window_size", {windowW, windowH}},
                    {"framebuffer_size", {framebufferW, framebufferH}},
                    {"scene_size", {gRenderer ? gRenderer->sceneWidth : 0,
                                    gRenderer ? gRenderer->sceneHeight : 0}},
                    {"ui_size", {uiScreenW(), uiScreenH()}},
                    {"stretched_whole_frame",
                     gRenderer ? gRenderer->stretchesWholeFrame() : false},
                    {"has_gizmo", hasGizmo},
                    {"gizmo_center_screen", {gizmoX, gizmoY}},
                    {"gizmo_axis_screens", axisScreens},
                    {"selection_count", mSelection.size()},
                    {"mode", "editor_input"}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (right && !mRightPrev) {
        const int row = outlinerRowAt(screenCursor.x, screenCursor.y);
        if (row >= 0) {
            std::vector<EditorObjectRef> objects;
            collectEditorObjects(objects);
            if (row < (int)objects.size()) {
                selectOnly(objects[(size_t)row]);
                openContextMenu(objects[(size_t)row]);
                mRightPrev = right;
                return;
            }
        }
        mContextMenuOpen = false;
        mLooking = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        THE_CAMERA.firstMouse = true;
    } else if (!right && mRightPrev) {
        mLooking = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        THE_CAMERA.firstMouse = true;
    }
    if (!mLooking && glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_NORMAL)
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    mRightPrev = right;

    if (mLooking) {
        mLeftPrev = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        return;
    }

    const bool left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (left && !mLeftPrev) {
        const int row = outlinerRowAt(screenCursor.x, screenCursor.y);
        if (row >= 0) {
            std::vector<EditorObjectRef> objects;
            collectEditorObjects(objects);
            const int index = row;
            if (index >= 0 && index < (int)objects.size()) {
                const bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                                  glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
                const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                                   glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
                if (shift) {
                    int anchorIndex = -1;
                    for (size_t i = 0; i < objects.size(); ++i)
                        if (objects[i] == mOutlinerAnchor) { anchorIndex = (int)i; break; }
                    if (anchorIndex >= 0) {
                        const int lo = std::min(anchorIndex, index);
                        const int hi = std::max(anchorIndex, index);
                        mSelection.assign(objects.begin() + lo, objects.begin() + hi + 1);
                    } else {
                        selectOnly(objects[(size_t)index]);
                        mOutlinerAnchor = objects[(size_t)index];
                    }
                } else if (ctrl) {
                    toggleSelection(objects[(size_t)index]);
                    mOutlinerAnchor = objects[(size_t)index];
                } else {
                    selectOnly(objects[(size_t)index]);
                    mOutlinerAnchor = objects[(size_t)index];
                }
                notify("selected " + std::to_string(mSelection.size()) + " object(s)");
            }
            mLeftPrev = left;
            return;
        }
    }
    if (left && !mLeftPrev) {
        GuiLayout& editorLayout =
            GuiLayoutManager::instance().getLayout("config/gui/map-editor.json");
        auto pointIn = [](const glm::vec2& p, const UIRect& r) {
            return p.x >= r.x && p.x <= r.x + r.w &&
                   p.y >= r.y && p.y <= r.y + r.h;
        };
        auto sizedRect = [&](const char* id, const UIRect& fallback) {
            const GuiElement* element = editorLayout.get(id);
            if (!element || element->w <= 0.0f || element->h <= 0.0f)
                return fallback;
            return editorLayout.getRect(id, fallback);
        };
        const UIRect panelRect = sizedRect(
            "map-editor.panel", {16.0f, 16.0f, 700.0f, 104.0f});
        const UIRect toolbarRect = sizedRect(
            "map-editor.toolbar", {16.0f, 124.0f, 660.0f, 36.0f});
        const UIRect outlinerRect = sizedRect(
            "map-editor.outliner",
            {screenWidth() - editorCfg.outlinerWidth, editorCfg.outlinerTop,
             editorCfg.outlinerWidth,
             screenHeight() - editorCfg.outlinerTop -
                 editorCfg.outlinerBottomPadding});
        const UIRect helpWorldRect = sizedRect(
            "map-editor.help-world", {16.0f, screenHeight() - 42.0f, 0.0f, 0.0f});
        const UIRect helpCommandsRect = sizedRect(
            "map-editor.help-commands", {16.0f, screenHeight() - 22.0f, 0.0f, 0.0f});
        const bool overTopPanel =
            pointIn(screenCursor, panelRect) || pointIn(screenCursor, toolbarRect);
        const bool overOutliner = pointIn(screenCursor, outlinerRect);
        const bool overHelp =
            pointIn(screenCursor, helpWorldRect) ||
            pointIn(screenCursor, helpCommandsRect);
        if (overTopPanel || overOutliner || overHelp) {
            mLeftPrev = left;
            return;
        }
    }
    const glm::vec2 cursorNow = screenCursor;
    const bool shiftNow = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                          glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    const bool ctrlNow = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                         glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    if (left && !mLeftPrev && mDrag.mode == DragMode::None) {
        EditorObjectRef handleRef;
        int axis = -1;
        const bool handleHit =
            !mSelection.empty() && handleAtCursor(handleRef, axis);
        EditorObjectRef hit;
        float distance = 0.0f;
        const bool objectHit = pickAtCursor(hit, distance);
        const bool differentObject =
            objectHit && (mSelection.empty() || !isSelected(hit));

        int action = 0;
        if (ModuleHost::instance().mapEditorLoaded()) {
            mimita_map_editor::EditorClickV1 click{};
            click.cursor[0] = cursorNow.x;
            click.cursor[1] = cursorNow.y;
            click.objectHit = objectHit ? 1 : 0;
            click.objectKind = objectHit ? static_cast<int>(hit.kind) : -1;
            if (objectHit)
                std::snprintf(click.objectId, sizeof(click.objectId), "%s",
                              hit.id.c_str());
            click.objectDistance = objectHit ? distance : -1.0f;
            click.differentObject = differentObject ? 1 : 0;
            click.handleHit = handleHit ? 1 : 0;
            click.handleAxis = axis;
            click.selectionCount = (int)mSelection.size();
            click.shift = shiftNow ? 1 : 0;
            click.ctrl = ctrlNow ? 1 : 0;
            if (ModuleHost::instance().mapEditorResolveClick(&click))
                action = click.action;
        }
        if (action == 0) {
            if (handleHit) action = 3;
            else if (objectHit && (differentObject || shiftNow))
                action = shiftNow ? 2 : 1;
            else if (objectHit) action = 4;
            else action = 5;
        }

        const char* chosenAction = "box_select";
        if (action == 3) {
            beginDrag(handleRef,
                      axis == 0 ? DragMode::AxisX :
                      axis == 1 ? DragMode::AxisY :
                      axis == 2 ? DragMode::AxisZ :
                      axis == 3 ? DragMode::ScaleX :
                      axis == 4 ? DragMode::ScaleY : DragMode::ScaleZ,
                      selectionCenter());
            mClick.active = false;
            chosenAction = "begin_handle_drag";
        } else if (action == 1 || action == 2 || action == 4) {
            if (action == 2) toggleSelection(hit);
            else if (!isSelected(hit)) selectOnly(hit);
            mClick.active = true;
            mClick.ref = hit;
            mClick.pressScreen = cursorNow;
            mClick.shift = shiftNow;
            chosenAction = "select_object";
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map-editor.pick", hit.id, "viewport object picked", 0,
                {{"kind", kindName(hit.kind)}, {"id", hit.id},
                 {"distance", distance},
                 {"selection_count", mSelection.size()},
                 {"cursor", {cursorNow.x, cursorNow.y}}},
                __FILE__, __LINE__, __func__);
        } else {
            if (!shiftNow && !ctrlNow) clearSelection();
            mClick.active = false;
            mBoxSelecting = true;
            mBoxStartScreen = cursorNow;
            mBoxEndScreen = cursorNow;
            chosenAction = "box_select";
        }

        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.handle.pick", handleHit ? handleRef.id :
                (objectHit ? hit.id : "none"),
            "editor click resolved with module action", 0,
            nlohmann::json{
                {"cursor", {cursorNow.x, cursorNow.y}},
                {"object", objectHit ? hit.id : std::string()},
                {"object_kind", objectHit ? kindName(hit.kind) : std::string()},
                {"object_distance", objectHit ? distance : -1.0f},
                {"different_object", differentObject},
                {"handle_hit", handleHit},
                {"handle_axis", axis},
                {"hover_axis", mHoverAxis},
                {"module_action", action},
                {"selection_count", mSelection.size()},
                {"chosen_action", chosenAction}},
            __FILE__, __LINE__, __func__);
    } else if (left && mClick.active && mDrag.mode == DragMode::None) {
        if (glm::length(cursorNow - mClick.pressScreen) >= 4.0f &&
            !mSelection.empty()) {
            beginDrag(mSelection.front(), DragMode::ObjectPlane,
                      selectionCenter());
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map-editor.click.action", mClick.ref.id,
                "click exceeded drag threshold; transform started", 0,
                {{"kind", kindName(mClick.ref.kind)}, {"id", mClick.ref.id},
                 {"delta_pixels", glm::length(cursorNow - mClick.pressScreen)}},
                __FILE__, __LINE__, __func__);
            mClick.active = false;
        }
    } else if (left && mBoxSelecting) {
        mBoxEndScreen = cursorNow;
        updateBoxSelection();
    }
    if (!left && mLeftPrev) {
        if (mDrag.mode != DragMode::None) {
            endDrag(true);
        } else if (mBoxSelecting) {
            finishBoxSelection();
        }
        mClick.active = false;
    }

    if (left && mDrag.mode != DragMode::None)
        updateDrag();

    if (!left && mDrag.mode == DragMode::None) {
        EditorObjectRef hit;
        float distance = 0.0f;
        mHoverValid = pickAtCursor(hit, distance);
        if (mHoverValid) mHover = hit;
        int axis = -1;
        EditorObjectRef handleRef;
        const int previousHoverAxis = mHoverAxis;
        const int nextHoverAxis =
            handleAtCursor(handleRef, axis) ? axis : -1;
        if (nextHoverAxis != previousHoverAxis) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map-editor.gizmo.hover", handleRef.id,
                "editor gizmo hover axis changed", 0,
                nlohmann::json{{"cursor", {screenCursor.x, screenCursor.y}},
                               {"axis", nextHoverAxis},
                               {"previous_axis", previousHoverAxis},
                               {"selection_count", mSelection.size()}},
                __FILE__, __LINE__, __FUNCTION__);
        }
        mHoverAxis = nextHoverAxis;
    }
    mLeftPrev = left;
}

void MapEditor::updateBoxSelection()
{
    const float minX = std::min(mBoxStartScreen.x, mBoxEndScreen.x);
    const float maxX = std::max(mBoxStartScreen.x, mBoxEndScreen.x);
    const float minY = std::min(mBoxStartScreen.y, mBoxEndScreen.y);
    const float maxY = std::max(mBoxStartScreen.y, mBoxEndScreen.y);
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    mSelection.clear();
    for (const EditorObjectRef& ref : objects) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (!DebugVis::projectToScreen(THE_CAMERA, objectPosition(ref), sx, sy))
            continue;
        if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY)
            mSelection.push_back(ref);
    }
}

void MapEditor::finishBoxSelection()
{
    updateBoxSelection();
    mBoxSelecting = false;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.box_select", mapId(), "viewport box selection completed", 0,
        {{"start", {mBoxStartScreen.x, mBoxStartScreen.y}},
         {"end", {mBoxEndScreen.x, mBoxEndScreen.y}},
         {"selected_count", mSelection.size()},
         {"selected_ids", nlohmann::json::array()}},
        __FILE__, __LINE__, __func__);
    notify("box selected " + std::to_string(mSelection.size()) + " object(s)");
}

void MapEditor::selectAllEdit()
{
    mSelection.clear();
    for (const auto& entity : MapConfigRegistry::instance().current().entities)
        if (entity.enabled) mSelection.push_back({EditorObjectKind::Entity, entity.id});
    for (const auto& block : MapConfigRegistry::instance().current().blocks)
        if (block.enabled) mSelection.push_back({EditorObjectKind::Block, block.id});
    for (const auto& site : MapConfigRegistry::instance().current().bombSites)
        mSelection.push_back({EditorObjectKind::BombSite, site.id});
    for (const auto& route : MapConfigRegistry::instance().current().payload.route)
        mSelection.push_back({EditorObjectKind::PayloadRoutePoint, route.id});
    if (gpWorld)
        for (const auto& object : gpWorld->mesh.objects)
            mSelection.push_back({EditorObjectKind::MapGLBObject, object.id});
    notify("selected " + std::to_string(mSelection.size()) + " objects");
}

bool MapEditor::handleKey(int key, int mods, int action)
{
    if (!mActive || action != GLFW_PRESS) return false;
    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    if (ctrl && key == GLFW_KEY_S) { save(); return true; }
    if (ctrl && key == GLFW_KEY_C) { copySelection(); return true; }
    if (ctrl && key == GLFW_KEY_V) { pasteClipboard(); return true; }
    if (ctrl && key == GLFW_KEY_X) { cutSelection(); return true; }
    if (ctrl && key == GLFW_KEY_D) { duplicateSelection(); return true; }
    if (ctrl && key == GLFW_KEY_Z) { shift ? redo() : undo(); return true; }
    if (ctrl && key == GLFW_KEY_Y) { redo(); return true; }
    if (ctrl && key == GLFW_KEY_A) { selectAllEdit(); return true; }
    if (key == GLFW_KEY_F2) { beginRenameSelection(); return true; }
    if (key == GLFW_KEY_DELETE || key == GLFW_KEY_X) { deleteSelection(); return true; }
    if (key == GLFW_KEY_V && !ctrl) {
        mVertexSnap = !mVertexSnap;
        notify(std::string("vertex snap ") + (mVertexSnap ? "on" : "off"));
        return true;
    }
    if (key == GLFW_KEY_G && !ctrl) {
        mGridSnap = !mGridSnap;
        notify(std::string("grid snap ") + (mGridSnap ? "on" : "off"));
        return true;
    }
    if (key == GLFW_KEY_F && !ctrl) {
        focusSelection();
        return true;
    }
    if (key == GLFW_KEY_ESCAPE) { handleEscape(); return true; }
    if (key == GLFW_KEY_PAGE_UP) {
        mOutlinerScroll.scrollY = std::max(0.0f,
            mOutlinerScroll.scrollY - (float)MapEditorConfig::instance().data().rowHeight * 10.0f);
        return true;
    }
    if (key == GLFW_KEY_PAGE_DOWN) {
        mOutlinerScroll.scrollY +=
            (float)MapEditorConfig::instance().data().rowHeight * 10.0f;
        return true;
    }
    return false;
}

void MapEditor::focusSelection()
{
    if (mSelection.empty()) return;
    const glm::vec3 center = selectionCenter();
    float radius = 1.0f;
    const EditorObjectRef& ref = mSelection.front();
    if (ref.kind == EditorObjectKind::MapGLBObject) {
        if (const MapGLBObject* object = mapObjectFor(ref)) {
            const glm::vec3 extent = (object->boundsMax - object->boundsMin) * 0.5f;
            radius = std::max({extent.x, extent.y, extent.z, 0.5f});
        }
    } else if (const AuthoredBlock* block = blockFor(ref)) {
        radius = std::max({block->size.x, block->size.y, block->size.z, 0.5f}) * 0.5f;
    } else if (const MapEntity* entity = entityFor(ref)) {
        radius = std::max({entity->size.x, entity->size.y, entity->size.z, entity->radius, 0.5f});
    } else if (ref.kind == EditorObjectKind::BombSite) {
        if (const BombSite* site = MapConfigRegistry::instance().findSite(ref.id))
            radius = std::max(site->radius, 0.5f);
    } else if (const PayloadRoutePoint* route = routeFor(ref)) {
        radius = std::max(route->radius, 0.5f);
    }
    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    const float halfFov = glm::radians(std::max(1.0f, THE_CAMERA.fov) * 0.5f);
    const float distance = std::max(cfg.focusMinDistance,
        radius / std::max(0.01f, std::tan(halfFov)) * cfg.focusPadding);
    THE_CAMERA.pos = center - THE_CAMERA.front * distance;
    THE_CAMERA.firstMouse = true;
    notify("framed " + describeSelection());
}

void MapEditor::handleEscape()
{
    if (mContextMenuOpen) {
        mContextMenuOpen = false;
        return;
    }
    if (mDrag.mode != DragMode::None) {
        endDrag(false);
        notify("drag cancelled");
        return;
    }
    if (!mSelection.empty()) {
        clearSelection();
        notify("selection cleared");
        return;
    }
    setActive(false);
}

bool MapEditor::save(bool writeBackup)
{
    const bool ok = MapConfigRegistry::instance().save(writeBackup);
    if (ok) {
        const std::string validation = validatePayloadRoute();
        notify(validation.empty()
            ? "saved " + MapConfigRegistry::instance().configPath()
            : "saved " + MapConfigRegistry::instance().configPath() + "\n" + validation);
        if (!validation.empty()) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Important,
                "map-payload-route.validated", MapConfigRegistry::instance().mapId(),
                "map editor save validation", 0,
                {{"map", MapConfigRegistry::instance().mapId()},
                 {"report", validation}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    } else {
        notify("save failed: " + MapConfigRegistry::instance().configPath());
    }
    return ok;
}

bool MapEditor::reload()
{
    const bool ok = MapConfigRegistry::instance().load(mapId());
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.reload.result", mapId(),
        ok ? "map JSON reloaded" : "map JSON reload failed; previous state retained", 0,
        nlohmann::json{{"map", mapId()},
                       {"success", ok},
                       {"revision", MapConfigRegistry::instance().revision()},
                       {"path", MapConfigRegistry::instance().configPath()}},
        __FILE__, __LINE__, __FUNCTION__);
    if (ok) {
        mUndo.clear();
        mRedo.clear();
        mUndoLabels.clear();
        mRedoLabels.clear();
        mSelection.clear();
        rebuildGeometry();
        notify("reloaded " + mapId());
    } else {
        notify("reload failed; previous map state retained");
    }
    return ok;
}

void MapEditor::rebuildGeometry()
{
    if (!gpWorld) return;
    AuthoredGeometryOptions options;
    options.rebuildCollision = true;
    options.staticFaceRemoval = true;
    applyAuthoredMapGeometry(*gpWorld, mapId(), options);
    mGeometryDirty = false;
    mCollisionDirty = false;
}

void MapEditor::rebuildAuthoredRenderGeometry()
{
    if (!gpWorld) return;
    AuthoredGeometryOptions options;
    options.rebuildCollision = false;
    options.staticFaceRemoval = false;
    applyAuthoredMapGeometry(*gpWorld, mapId(), options);
    mGeometryDirty = false;
}

std::string MapEditor::addEntity(const std::string& type, const std::string& id)
{
    pushUndo("add entity");
    const glm::vec3 position = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    const std::string created = MapConfigRegistry::instance().createEntity(type, position, id);
    if (created.empty()) {
        notify("add entity failed: id exists or is invalid");
        return {};
    }
    selectOnly({EditorObjectKind::Entity, created});
    notify("added entity " + created);
    return created;
}

std::string MapEditor::addBlock(const std::string& id)
{
    pushUndo("add block");
    const glm::vec3 position = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    const std::string created = MapConfigRegistry::instance().createBlock(
        position, glm::vec3(2.0f), id);
    if (created.empty()) {
        notify("add block failed: id exists or is invalid");
        return {};
    }
    selectOnly({EditorObjectKind::Block, created});
    rebuildGeometry();
    notify("added block " + created);
    return created;
}

std::string MapEditor::addChunk(const std::string& id)
{
    return addChunkWithFootprint(id, glm::vec3(10.0f, 10.0f, 4.0f));
}

std::string MapEditor::addChunkWithFootprint(const std::string& id,
                                             const glm::vec3& footprint)
{
    MapConfigRegistry& registry = MapConfigRegistry::instance();
    std::string created = id;
    if (created.empty()) {
        size_t next = registry.chunks().size() + 1;
        do {
            created = "chunk" + std::to_string(next++);
        } while (std::any_of(registry.chunks().begin(), registry.chunks().end(),
                             [&](const AuthoredChunk& chunk) {
                                 return chunk.id == created;
                             }));
    }
    for (const AuthoredChunk& existing : registry.chunks())
        if (existing.id == created) {
            notify("chunk id already exists: " + created);
            return {};
        }

    const float width = std::max(0.5f, footprint.x);
    const float depth = std::max(0.5f, footprint.y);
    const float height = std::max(0.5f, footprint.z);
    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;

    pushUndo("add chunk");
    AuthoredChunk chunk;
    chunk.id = created;
    chunk.boundsMin = {-halfW, -halfD, -0.5f};
    chunk.boundsMax = {halfW, halfD, height};
    ChunkPart floor;
    floor.id = "floor";
    floor.kind = "floor";
    floor.position = {0.0f, 0.0f, -0.25f};
    floor.size = {width, depth, 0.5f};
    chunk.parts.push_back(floor);
    for (const char* side : {"east", "west", "north", "south"}) {
        AuthoredChunkConnector connector;
        connector.id = std::string(side) + "_" + created;
        connector.side = side;
        connector.role = "bidirectional";
        connector.orientation = "doorway";
        connector.position =
            side == std::string("east") ? glm::vec3(halfW, 0, 0) :
            side == std::string("west") ? glm::vec3(-halfW, 0, 0) :
            side == std::string("north") ? glm::vec3(0, halfD, 0) :
                                           glm::vec3(0, -halfD, 0);
        chunk.connectors.push_back(connector);
    }
    registry.chunksMutable().push_back(std::move(chunk));
    registry.markChanged();
    previewChunk(created);
    selectOnly({EditorObjectKind::ChunkPart, created + "/floor"});
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk.added", created, "chunk added through map editor", 0,
        {{"chunk", created}, {"parts", 1}, {"connectors", 4}, {"spawns", 0},
         {"footprint", {width, depth, height}}},
        __FILE__, __LINE__, __func__);
    notify("added chunk " + created);
    return created;
}

std::string MapEditor::addChunkPart(const std::string& kind)
{
    if (!mChunkPreviewValid) {
        notify("preview or select a chunk first");
        return {};
    }
    AuthoredChunk* chunk = nullptr;
    for (AuthoredChunk& candidate : MapConfigRegistry::instance().chunksMutable())
        if (candidate.id == mChunkPreviewChunkId) { chunk = &candidate; break; }
    if (!chunk) return {};
    std::string id;
    size_t next = chunk->parts.size() + 1;
    do {
        id = kind + std::to_string(next++);
    } while (std::any_of(chunk->parts.begin(), chunk->parts.end(),
                         [&](const ChunkPart& part) { return part.id == id; }));
    pushUndo("add chunk part");
    ChunkPart part;
    part.id = id;
    part.kind = kind;
    part.position = {0.0f, 0.0f, kind == "wall" ? 1.0f : 0.5f};
    part.size = kind == "wall" ? glm::vec3(2.0f, 0.25f, 2.0f)
                                 : glm::vec3(1.0f);
    chunk->parts.push_back(part);
    MapConfigRegistry::instance().markChanged();
    selectOnly({EditorObjectKind::ChunkPart, chunk->id + "/" + id});
    refreshPreviewChunk();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-part.added", chunk->id,
        "chunk part added through map editor", 0,
        {{"chunk", chunk->id}, {"part", id}, {"kind", kind},
         {"position", {part.position.x, part.position.y, part.position.z}},
         {"size", {part.size.x, part.size.y, part.size.z}}},
        __FILE__, __LINE__, __func__);
    notify("added " + id + " to " + chunk->id);
    return id;
}

bool MapEditor::addConnector(const std::string& side)
{
    if (!mChunkPreviewValid) {
        notify("preview or select a chunk first");
        return false;
    }
    AuthoredChunk* chunk = nullptr;
    for (AuthoredChunk& candidate : MapConfigRegistry::instance().chunksMutable())
        if (candidate.id == mChunkPreviewChunkId) { chunk = &candidate; break; }
    if (!chunk) return false;
    if (side != "east" && side != "west" && side != "north" && side != "south") {
        notify("connector side must be east, west, north, or south");
        return false;
    }
    pushUndo("add connector");
    AuthoredChunkConnector connector;
    size_t next = chunk->connectors.size() + 1;
    do {
        connector.id = side + std::to_string(next++);
    } while (std::any_of(chunk->connectors.begin(), chunk->connectors.end(),
                         [&](const AuthoredChunkConnector& existing) {
                             return existing.id == connector.id;
                         }));
    connector.side = side;
    connector.role = "bidirectional";
    connector.orientation = "doorway";
    connector.position = side == "east" ? glm::vec3(5, 0, 0) :
        side == "west" ? glm::vec3(-5, 0, 0) :
        side == "north" ? glm::vec3(0, 5, 0) : glm::vec3(0, -5, 0);
    chunk->connectors.push_back(connector);
    MapConfigRegistry::instance().markChanged();
    selectOnly({EditorObjectKind::ChunkConnector, chunk->id + "/" + connector.id});
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.connector.added", chunk->id,
        "chunk connector added through map editor", 0,
        {{"chunk", chunk->id}, {"connector", connector.id},
         {"side", connector.side}, {"role", connector.role},
         {"position", {connector.position.x, connector.position.y,
                       connector.position.z}},
         {"radius", connector.radius}},
        __FILE__, __LINE__, __func__);
    notify("added connector " + connector.id);
    return true;
}

std::string MapEditor::addChunkSpawn(const std::string& allowedActors)
{
    if (!mChunkPreviewValid) {
        notify("preview or select a chunk first");
        return {};
    }
    AuthoredChunk* chunk = nullptr;
    for (AuthoredChunk& candidate : MapConfigRegistry::instance().chunksMutable())
        if (candidate.id == mChunkPreviewChunkId) { chunk = &candidate; break; }
    if (!chunk) return {};
    std::string id;
    size_t next = chunk->spawnPoints.size() + 1;
    do {
        id = "spawn" + std::to_string(next++);
    } while (std::any_of(chunk->spawnPoints.begin(), chunk->spawnPoints.end(),
                         [&](const ChunkSpawnPoint& spawn) {
                             return spawn.id == id;
                         }));
    pushUndo("add chunk spawn");
    ChunkSpawnPoint spawn;
    spawn.id = id;
    spawn.position = glm::vec3(0.0f, 0.0f, 0.5f);
    spawn.radius = 0.5f;
    spawn.allowedActors =
        (allowedActors == "human" || allowedActors == "npc") ? allowedActors
                                                             : "any";
    chunk->spawnPoints.push_back(spawn);
    MapConfigRegistry::instance().markChanged();
    selectOnly({EditorObjectKind::ChunkSpawn, chunk->id + "/" + id});
    refreshPreviewChunk();
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.chunk-spawn-added", chunk->id,
        "chunk spawn anchor added", 0,
        {{"chunk", chunk->id}, {"spawn", id},
         {"allowed_actors", spawn.allowedActors},
         {"position", {spawn.position.x, spawn.position.y, spawn.position.z}},
         {"radius", spawn.radius}},
        __FILE__, __LINE__, __func__);
    notify("added spawn " + id + " to " + chunk->id);
    return id;
}

void MapEditor::openContextMenu(const EditorObjectRef& target)
{
    mContextMenuTarget = target;
    mContextMenuOpen = true;
    mContextMenuPos = cursorScreen(glfwGetCurrentContext(), mMouseX, mMouseY);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.context_menu", target.id, "object context menu opened", 0,
        {{"kind", kindName(target.kind)}, {"id", target.id},
         {"cursor", {mContextMenuPos.x, mContextMenuPos.y}}},
        __FILE__, __LINE__, __func__);
}

std::string MapEditor::createAttachment()
{
    if (mSelection.empty()) {
        notify("attachment: select a parent object first");
        return {};
    }
    const EditorObjectRef parent = mSelection.front();
    const glm::vec3 parentPosition = objectPosition(parent);
    const std::string childId = MapConfigRegistry::instance().createBlock(
        parentPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
    if (childId.empty()) {
        notify("attachment failed: could not create child block");
        return {};
    }
    MapAttachment attachment;
    attachment.id = "attachment" +
        std::to_string(MapConfigRegistry::instance().attachments().size() + 1);
    attachment.name = "Attachment";
    attachment.parentKind = kindName(parent.kind);
    attachment.parentId = parent.id;
    attachment.childKind = "block";
    attachment.childId = childId;
    attachment.anchored = true;
    MapConfigRegistry::instance().attachmentsMutable().push_back(attachment);
    MapConfigRegistry::instance().markChanged();
    selectOnly({EditorObjectKind::Block, childId});
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.attachment.added", attachment.id,
        "anchored attachment added through context menu", 0,
        {{"attachment", attachment.id},
         {"parent_kind", attachment.parentKind},
         {"parent_id", attachment.parentId},
         {"child_kind", attachment.childKind},
         {"child_id", attachment.childId},
         {"anchored", attachment.anchored},
         {"mass", attachment.mass},
         {"density", attachment.density},
         {"material_strength", attachment.materialStrength}},
        __FILE__, __LINE__, __func__);
    notify("added attachment " + childId + " under " + parent.id);
    return childId;
}

bool MapEditor::setAttachmentAnchored(bool anchored)
{
    if (mSelection.empty()) return false;
    const EditorObjectRef ref = mSelection.front();
    bool changed = false;
    for (MapAttachment& attachment :
         MapConfigRegistry::instance().attachmentsMutable()) {
        if (attachment.parentId != ref.id &&
            attachment.childId != ref.id)
            continue;
        attachment.anchored = anchored;
        changed = true;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.attachment.updated", attachment.id,
            anchored ? "attachment anchored" : "attachment unanchored", 0,
            {{"attachment", attachment.id},
             {"parent_id", attachment.parentId},
             {"child_id", attachment.childId},
             {"anchored", anchored}},
            __FILE__, __LINE__, __func__);
    }
    if (changed) MapConfigRegistry::instance().markChanged();
    notify(changed ? (anchored ? "attached" : "detached")
                   : "no attachment for this object");
    return changed;
}

bool MapEditor::removeAttachmentForSelection()
{
    if (mSelection.empty()) return false;
    const EditorObjectRef ref = mSelection.front();
    auto& attachments = MapConfigRegistry::instance().attachmentsMutable();
    const size_t before = attachments.size();
    attachments.erase(
        std::remove_if(attachments.begin(), attachments.end(),
                       [&](const MapAttachment& attachment) {
                           return attachment.parentId == ref.id ||
                                  attachment.childId == ref.id;
                       }),
        attachments.end());
    const bool changed = attachments.size() != before;
    if (changed) {
        MapConfigRegistry::instance().markChanged();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.attachment.removed", ref.id,
            "attachment removed through context menu", 0,
            {{"object", ref.id}, {"removed", before - attachments.size()}},
            __FILE__, __LINE__, __func__);
        notify("removed attachment");
    }
    return changed;
}

bool MapEditor::setSelectedAttachmentProperties(float mass, float density,
                                                float strength)
{
    if (mSelection.empty()) return false;
    const EditorObjectRef ref = mSelection.front();
    bool changed = false;
    for (MapAttachment& attachment :
         MapConfigRegistry::instance().attachmentsMutable()) {
        if (attachment.childId != ref.id &&
            attachment.parentId != ref.id)
            continue;
        attachment.mass = std::max(0.001f, mass);
        attachment.density = std::max(0.001f, density);
        attachment.materialStrength = std::max(0.001f, strength);
        changed = true;
    }
    if (changed) MapConfigRegistry::instance().markChanged();
    return changed;
}

std::string MapEditor::previewConnectionsReport() const
{
    std::string report;
    if (!mChunkPreviewValid)
        return "preview a chunk first";
    const AuthoredChunk* chunk = nullptr;
    for (const AuthoredChunk& candidate : MapConfigRegistry::instance().chunks())
        if (candidate.id == mChunkPreviewChunkId) { chunk = &candidate; break; }
    if (!chunk)
        return "chunk not found";

    report += "chunk " + chunk->id + " bounds min(" +
        std::to_string(chunk->boundsMin.x) + "," +
        std::to_string(chunk->boundsMin.y) + "," +
        std::to_string(chunk->boundsMin.z) + ") max(" +
        std::to_string(chunk->boundsMax.x) + "," +
        std::to_string(chunk->boundsMax.y) + "," +
        std::to_string(chunk->boundsMax.z) + ")\n";
    size_t missing = 0;
    for (const auto& connector : chunk->connectors) {
        std::string error;
        const auto matches = previewCompatibleConnectors(chunk->id,
            connector.id, &error);
        if (matches.empty()) {
            ++missing;
            report += chunk->id + "/" + connector.id +
                " missing: " + error + "\n";
            continue;
        }
        report += chunk->id + "/" + connector.id + " matches=" +
            std::to_string(matches.size());
        float transform[16] = {};
        char reason[128] = {};
        if (ModuleHost::instance().mapEditorEvaluateConnectorPair(
                chunk->id.c_str(), connector.id.c_str(),
                matches.front().chunkId.c_str(),
                matches.front().connectorId.c_str(), transform, reason,
                sizeof(reason))) {
            const float yaw = std::atan2(transform[1], transform[0]) *
                180.0f / 3.14159265358979323846f;
            report += " -> " + matches.front().chunkId + "/" +
                matches.front().connectorId + " pos(" +
                std::to_string(transform[12]) + "," +
                std::to_string(transform[13]) + "," +
                std::to_string(transform[14]) + ") yaw=" +
                std::to_string(yaw);
        } else if (reason[0]) {
            report += " incompatible: " + std::string(reason);
        }
        report += "\n";
    }
    report += "connections: " +
        std::to_string(chunk->connectors.size() - missing) +
        " compatible, " + std::to_string(missing) + " missing";
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.connection.preview", chunk->id,
        "chunk bounds and connector transforms previewed", 0,
        {{"chunk", chunk->id}, {"report", report},
         {"bounds_min", {chunk->boundsMin.x, chunk->boundsMin.y,
                         chunk->boundsMin.z}},
         {"bounds_max", {chunk->boundsMax.x, chunk->boundsMax.y,
                         chunk->boundsMax.z}},
         {"connectors", chunk->connectors.size()},
         {"missing", missing}},
        __FILE__, __LINE__, __func__);
    return report;
}

void MapEditor::previewConnections()
{
    const std::string report = previewConnectionsReport();
    Terminal::instance().addLog("[MAPEDIT] " + report);
    notify(report);
}

std::string MapEditor::addVehicle(const std::string& templateId,
                                  const std::string& id)
{
    if (!VehicleConfigRegistry::instance().find(templateId)) {
        notify("add vehicle failed: unknown template " + templateId);
        return {};
    }
    pushUndo("add vehicle");
    const glm::vec3 position = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    float yaw = 0.0f;
    if (glm::dot(THE_CAMERA.front, THE_CAMERA.front) > 1e-6f)
        yaw = std::atan2(THE_CAMERA.front.y, THE_CAMERA.front.x);
    const std::string created = MapConfigRegistry::instance().createVehicle(
        templateId, position, yaw, id);
    if (created.empty()) {
        notify("add vehicle failed: id exists or is invalid");
        return {};
    }
    selectOnly({EditorObjectKind::Vehicle, created});
    notify("added vehicle " + created + " template=" + templateId);
    return created;
}

bool MapEditor::spawnSelectedVehicle()
{
    if (mSelection.empty()) {
        notify("spawn_vehicle: select a vehicle first");
        return false;
    }
    const MapVehicleSpawn* vehicle = vehicleFor(mSelection.front());
    if (!vehicle) {
        notify("spawn_vehicle: selection is not a vehicle placement");
        return false;
    }
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(vehicle->templateId);
    if (!spec) {
        notify("spawn_vehicle: unknown template " + vehicle->templateId);
        return false;
    }
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f), vehicle->position) *
        glm::mat4_cast(glm::angleAxis(vehicle->yaw, glm::vec3(0.0f, 0.0f, 1.0f)));
    const uint32_t spawned =
        VehicleSystem::instance().spawn(vehicle->templateId, transform);
    if (spawned == 0) {
        notify("spawn_vehicle: spawn failed");
        return false;
    }
    notify("spawned vehicle " + std::to_string(spawned) + " from " +
           vehicle->id + " template=" + vehicle->templateId);
    return true;
}

bool MapEditor::editSelectedVehicleWheel(int wheelIndex,
                                         const std::string& field,
                                         const glm::vec3& hub)
{
    if (mSelection.empty()) {
        notify("vehicle_wheel: select a vehicle first");
        return false;
    }
    const MapVehicleSpawn* placement = vehicleFor(mSelection.front());
    if (!placement) {
        notify("vehicle_wheel: selection is not a vehicle placement");
        return false;
    }
    VehicleSpec* spec =
        VehicleConfigRegistry::instance().findMutable(placement->templateId);
    if (!spec) {
        notify("vehicle_wheel: unknown template " + placement->templateId);
        return false;
    }
    if (wheelIndex < 0 || wheelIndex >= static_cast<int>(spec->wheels.size())) {
        notify("vehicle_wheel: wheel index out of range");
        return false;
    }
    if (field != "hub") {
        notify("vehicle_wheel: expected hub <x> <y> <z>");
        return false;
    }
    spec->wheels[wheelIndex].hub = hub;
    std::string error;
    if (!VehicleConfigRegistry::instance().saveTemplate(spec->id, &error)) {
        notify("vehicle_wheel save failed: " + error);
        return false;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.vehicle-wheel", placement->id,
        "vehicle template wheel hub edited in the map editor", 0,
        {{"vehicle_id", placement->id},
         {"template", spec->id},
         {"wheel_index", wheelIndex},
         {"field", "hub"},
         {"hub", {hub.x, hub.y, hub.z}}},
        __FILE__, __LINE__, __FUNCTION__);
    notify("wheel " + std::to_string(wheelIndex) + " hub set and saved");
    return true;
}

bool MapEditor::editSelectedVehicleWheel(int wheelIndex,
                                         const std::string& field,
                                         float value)
{
    if (mSelection.empty()) {
        notify("vehicle_wheel: select a vehicle first");
        return false;
    }
    const MapVehicleSpawn* placement = vehicleFor(mSelection.front());
    if (!placement) {
        notify("vehicle_wheel: selection is not a vehicle placement");
        return false;
    }
    VehicleSpec* spec =
        VehicleConfigRegistry::instance().findMutable(placement->templateId);
    if (!spec) {
        notify("vehicle_wheel: unknown template " + placement->templateId);
        return false;
    }
    if (wheelIndex < 0 || wheelIndex >= static_cast<int>(spec->wheels.size())) {
        notify("vehicle_wheel: wheel index out of range");
        return false;
    }
    if (!std::isfinite(value)) {
        notify("vehicle_wheel: value must be finite");
        return false;
    }
    if (field == "radius")
        spec->wheels[wheelIndex].radius = std::clamp(value, 0.05f, 2.0f);
    else if (field == "width")
        spec->wheels[wheelIndex].width = std::clamp(value, 0.02f, 1.5f);
    else {
        notify("vehicle_wheel: expected radius or width");
        return false;
    }
    std::string error;
    if (!VehicleConfigRegistry::instance().saveTemplate(spec->id, &error)) {
        notify("vehicle_wheel save failed: " + error);
        return false;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.vehicle-wheel", placement->id,
        "vehicle template wheel value edited in the map editor", 0,
        {{"vehicle_id", placement->id},
         {"template", spec->id},
         {"wheel_index", wheelIndex},
         {"field", field},
         {"value", field == "radius" ? spec->wheels[wheelIndex].radius
                                     : spec->wheels[wheelIndex].width}},
        __FILE__, __LINE__, __FUNCTION__);
    notify("wheel " + std::to_string(wheelIndex) + " " + field +
           " set and saved");
    return true;
}

std::string MapEditor::addBombSite(const std::string& id, float radius)
{
    pushUndo("add bombsite");
    const glm::vec3 position = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    const std::string created = MapConfigRegistry::instance().createBombSite(
        position, radius, id);
    if (created.empty()) {
        notify("add bombsite failed: id exists or is invalid");
        return {};
    }
    selectOnly({EditorObjectKind::BombSite, created});
    notify("added bombsite " + created);
    return created;
}

void MapEditor::setSelectedBombSiteRadius(float radius)
{
    if (mSelection.empty()) {
        notify("set_radius: no bombsite selected");
        return;
    }
    pushUndo("bombsite radius");
    bool changed = false;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::BombSite) continue;
        if (MapConfigRegistry::instance().setSiteRadius(
                ref.id, std::max(0.1f, radius)))
            changed = true;
    }
    if (!changed) {
        notify("set_radius: select a bombsite first");
        return;
    }
    notify("bombsite radius=" + std::to_string(std::max(0.1f, radius)));
}

std::string MapEditor::addRoutePoint(const std::string& id)
{
    pushUndo("add route point");
    const glm::vec3 position = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    const std::string created = MapConfigRegistry::instance().createPayloadRoutePoint(
        position, id);
    if (created.empty()) {
        notify("add route point failed: id exists or is invalid");
        return {};
    }
    selectOnly({EditorObjectKind::PayloadRoutePoint, created});
    rebuildGeometry();
    notify("added route point " + created);
    return created;
}

void MapEditor::addCartOrigin()
{
    pushUndo("add cart origin");
    auto& payload = MapConfigRegistry::instance().payloadMutable();
    payload.present = true;
    payload.hasCartOrigin = true;
    payload.cartOrigin = THE_CAMERA.pos + THE_CAMERA.front * 8.0f;
    MapConfigRegistry::instance().markChanged();
    selectOnly({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
    notify("added cart origin");
}

bool MapEditor::setSelectedRouteCheckpoint(bool checkpoint)
{
    auto& reg = MapConfigRegistry::instance();
    std::vector<PayloadRoutePoint*> points;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id))
            points.push_back(point);
    }
    if (points.empty()) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route checkpoint");
    for (PayloadRoutePoint* point : points)
        point->checkpoint = checkpoint;
    reg.markChanged();
    notify(std::string("route checkpoint=") + (checkpoint ? "1" : "0"));
    return true;
}

bool MapEditor::setSelectedRouteFinal(bool final)
{
    auto& reg = MapConfigRegistry::instance();
    std::vector<PayloadRoutePoint*> points;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id))
            points.push_back(point);
    }
    if (points.empty()) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route final");
    if (final) {
        for (PayloadRoutePoint& point : reg.payloadMutable().route)
            point.final = false;
        // A route has exactly one final point. Multiple selected points still
        // resolve to the first selection so authoring cannot create two finals.
        points.front()->final = true;
    } else {
        for (PayloadRoutePoint* point : points)
            point->final = false;
    }
    reg.markChanged();
    notify(std::string("route final=") + (final ? "1" : "0"));
    return true;
}

bool MapEditor::setSelectedRouteEnabled(bool enabled)
{
    auto& reg = MapConfigRegistry::instance();
    bool changed = false;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id)) {
            point->enabled = enabled;
            changed = true;
        }
    }
    if (!changed) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route enabled");
    reg.markChanged();
    notify(std::string("route enabled=") + (enabled ? "1" : "0"));
    return true;
}

bool MapEditor::setSelectedRouteRadius(float radius)
{
    auto& reg = MapConfigRegistry::instance();
    bool changed = false;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id)) {
            point->radius = std::max(0.1f, radius);
            changed = true;
        }
    }
    if (!changed) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route radius");
    reg.markChanged();
    notify("route radius=" + std::to_string(std::max(0.1f, radius)));
    return true;
}

bool MapEditor::setSelectedRouteTimeBonus(float seconds)
{
    auto& reg = MapConfigRegistry::instance();
    bool changed = false;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id)) {
            point->timeBonusSeconds = std::max(0.0f, seconds);
            changed = true;
        }
    }
    if (!changed) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route time bonus");
    reg.markChanged();
    notify("route time_bonus_seconds=" + std::to_string(std::max(0.0f, seconds)));
    return true;
}

bool MapEditor::setSelectedRouteWaitTicks(int ticks)
{
    auto& reg = MapConfigRegistry::instance();
    bool changed = false;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        if (PayloadRoutePoint* point = reg.findPayloadRoutePointMutable(ref.id)) {
            point->waitTicks = std::max(0, ticks);
            changed = true;
        }
    }
    if (!changed) {
        notify("select a route point first");
        return false;
    }
    pushUndo("route wait");
    reg.markChanged();
    notify("route waitTicks=" + std::to_string(std::max(0, ticks)));
    return true;
}

bool MapEditor::moveSelectedRoute(int direction)
{
    auto& reg = MapConfigRegistry::instance();
    PayloadRoutePoint* point = nullptr;
    for (const EditorObjectRef& ref : mSelection) {
        if (ref.kind != EditorObjectKind::PayloadRoutePoint) continue;
        point = reg.findPayloadRoutePointMutable(ref.id);
        if (point) break;
    }
    if (!point) {
        notify("select one route point first");
        return false;
    }
    auto& route = reg.payloadMutable().route;
    auto it = std::find_if(route.begin(), route.end(),
        [&](const PayloadRoutePoint& candidate) { return candidate.id == point->id; });
    if (it == route.end()) return false;
    const int index = (int)std::distance(route.begin(), it);
    const int target = std::clamp(index + (direction < 0 ? -1 : 1), 0,
                                  (int)route.size() - 1);
    if (target == index) {
        notify("route point is already at the edge");
        return false;
    }
    pushUndo("route order");
    std::swap(route[(size_t)index].order, route[(size_t)target].order);
    reg.renumberPayloadRoute();
    reg.markChanged();
    notify("route moved " + std::string(direction < 0 ? "up" : "down"));
    return true;
}

std::string MapEditor::validatePayloadRoute() const
{
    const PayloadConfig& payload = MapConfigRegistry::instance().payload();
    if (!payload.present && payload.route.empty() && !payload.hasCartOrigin)
        return {};
    std::vector<PayloadAuthoringIssue> issues;
    const bool valid = validatePayloadAuthoring(payload, issues);
    std::string report = valid ? "payload authoring valid" : "payload authoring invalid";
    for (const PayloadAuthoringIssue& issue : issues)
        report += "\n  " + issue.severity + " " + issue.code + ": " + issue.message;
    return report;
}

bool MapEditor::selectObject(const std::string& id)
{
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    for (const EditorObjectRef& ref : objects) {
        if (ref.id == id) {
            selectOnly(ref);
            notify("selected " + describeRef(ref));
            return true;
        }
    }
    if (id.find('/') != std::string::npos) {
        const EditorObjectRef partRef{EditorObjectKind::ChunkPart, id};
        if (chunkPartFor(partRef)) {
            selectOnly(partRef);
            notify("selected " + describeRef(partRef));
            return true;
        }
        const EditorObjectRef connectorRef{EditorObjectKind::ChunkConnector, id};
        if (chunkConnectorFor(connectorRef)) {
            selectOnly(connectorRef);
            notify("selected " + describeRef(connectorRef));
            return true;
        }
    }
    size_t chunkCount = 0;
    size_t partCount = 0;
    for (const AuthoredChunk& chunk : MapConfigRegistry::instance().chunks()) {
        ++chunkCount;
        partCount += chunk.parts.size();
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.select-missing", id,
        "editor selection id was not present in the object list", 0,
        {{"id", id}, {"objects", objects.size()}, {"chunks", chunkCount},
         {"parts", partCount}},
        __FILE__, __LINE__, __func__);
    notify("no object with id " + id);
    return false;
}

bool MapEditor::selectKind(const std::string& kind)
{
    std::string normalized = kind;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    if (normalized == "all") {
        selectAllEdit();
        return true;
    }
    EditorObjectKind want;
    if (normalized == "entity") want = EditorObjectKind::Entity;
    else if (normalized == "block") want = EditorObjectKind::Block;
    else if (normalized == "vehicle" || normalized == "car")
        want = EditorObjectKind::Vehicle;
    else if (normalized == "bombsite" || normalized == "site")
        want = EditorObjectKind::BombSite;
    else if (normalized == "route" || normalized == "payload")
        want = EditorObjectKind::PayloadRoutePoint;
    else if (normalized == "cart") want = EditorObjectKind::PayloadCartOrigin;
    else if (normalized == "glb") want = EditorObjectKind::MapGLBObject;
    else if (normalized == "chunk_part" || normalized == "part")
        want = EditorObjectKind::ChunkPart;
    else if (normalized == "chunk_connector" || normalized == "connector")
        want = EditorObjectKind::ChunkConnector;
    else if (normalized == "chunk_spawn" || normalized == "spawn")
        want = EditorObjectKind::ChunkSpawn;
    else {
        notify("select_kind expects entity, block, vehicle, bombsite, route, cart, glb, part, connector, or all");
        return false;
    }
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    mSelection.clear();
    for (const EditorObjectRef& ref : objects)
        if (ref.kind == want) mSelection.push_back(ref);
    if (!mSelection.empty() &&
        (want == EditorObjectKind::ChunkPart ||
         want == EditorObjectKind::ChunkConnector ||
         want == EditorObjectKind::ChunkSpawn)) {
        std::string chunkId;
        std::string itemId;
        if (splitChunkRef(mSelection.front().id, chunkId, itemId))
            ensureChunkPreview(chunkId);
    }
    if (mSelection.empty()) {
        notify("no " + kind + " objects to select");
        return false;
    }
    notify("selected " + std::to_string(mSelection.size()) + " " + kind + " object(s)");
    return true;
}

bool MapEditor::modulePrimarySelection(char* id, unsigned int idCapacity, int* kind,
                                       glm::vec3& position) const
{
    if (mSelection.empty())
        return false;
    const EditorObjectRef& primary = mSelection.front();
    if (id && idCapacity > 0) {
        std::strncpy(id, primary.id.c_str(), idCapacity - 1);
        id[idCapacity - 1] = '\0';
    }
    if (kind)
        *kind = static_cast<int>(primary.kind);
    position = objectPosition(primary);
    return true;
}

bool MapEditor::moduleSelectionAt(unsigned int index, int* kind, char* id,
                                  unsigned int idCapacity) const
{
    if (index >= mSelection.size())
        return false;
    const EditorObjectRef& ref = mSelection[index];
    if (id && idCapacity > 0) {
        std::strncpy(id, ref.id.c_str(), idCapacity - 1);
        id[idCapacity - 1] = '\0';
    }
    if (kind)
        *kind = static_cast<int>(ref.kind);
    return true;
}

bool MapEditor::moduleApplyPrimaryPosition(const glm::vec3& position)
{
    if (mSelection.empty())
        return false;
    const glm::vec3 delta = position - objectPosition(mSelection.front());
    if (glm::length(delta) <= 0.0001f)
        return true;
    nudgeSelection(delta);
    return true;
}

bool MapEditor::setSelectedPosition(const glm::vec3& position)
{
    if (mSelection.empty()) {
        notify("select an object first");
        return false;
    }
    const EditorObjectRef& primary = mSelection.front();
    const glm::vec3 before = objectPosition(primary);
    const glm::vec3 delta = position - before;
    if (glm::length(delta) <= 0.0001f) return true;

    bool handledByModule = false;
    if (ModuleHost::instance().mapEditorLoaded()) {
        const float target[3] = {position.x, position.y, position.z};
        handledByModule = ModuleHost::instance().mapEditorSetSelectedPosition(target);
    }
    if (!handledByModule)
        nudgeSelection(delta);

    StructuredLogger::instance().writeEvent(
        StructuredCategory::World, StructuredLevel::Important,
        "map-editor.transform.apply", primary.id,
        "property panel position edit applied", 0,
        nlohmann::json{{"map", mapId()},
                       {"object", primary.id},
                       {"kind", kindName(primary.kind)},
                       {"field", "position"},
                       {"before", {before.x, before.y, before.z}},
                       {"after", {position.x, position.y, position.z}},
                       {"source", "property_panel"},
                       {"handled_by_module", handledByModule},
                       {"revision", MapConfigRegistry::instance().revision()}},
        __FILE__, __LINE__, __FUNCTION__);
    notify("position set to " + std::to_string(position.x) + " " +
        std::to_string(position.y) + " " + std::to_string(position.z));
    return true;
}

void MapEditor::nudgeSelection(const glm::vec3& delta)
{
    if (mSelection.empty() || !gpWorld) {
        notify("select an object first");
        return;
    }
    pushUndo("nudge selection");
    AABB oldBounds;
    AABB newBounds;
    oldBounds.min = newBounds.min = glm::vec3(std::numeric_limits<float>::max());
    oldBounds.max = newBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    bool movedGLB = false;
    bool movedBlocks = false;
    for (const EditorObjectRef& ref : mSelection) {
        const glm::vec3 before = objectPosition(ref);
        if (ref.kind == EditorObjectKind::MapGLBObject) {
            const MapGLBObject* object = mapObjectFor(ref);
            if (!object) continue;
            oldBounds.min = glm::min(oldBounds.min, object->boundsMin);
            oldBounds.max = glm::max(oldBounds.max, object->boundsMax);
            movedGLB = true;
        }
        setObjectPosition(ref, before + delta);
        if (ref.kind == EditorObjectKind::MapGLBObject) {
            const MapGLBObject* object = mapObjectFor(ref);
            if (object) {
                newBounds.min = glm::min(newBounds.min, object->boundsMin);
                newBounds.max = glm::max(newBounds.max, object->boundsMax);
            }
        }
        if (ref.kind == EditorObjectKind::Block) movedBlocks = true;
    }
    if (movedGLB) {
        const bool collisionUpdated =
            updateCollisionForMovedObject(*gpWorld, oldBounds, newBounds);
        mCollisionDirty = !collisionUpdated;
    }
    if (movedBlocks) {
        rebuildAuthoredRenderGeometry();
        mCollisionDirty = true;
    }
    mLastEditTime = glfwGetTime();
    notify("nudged selection");
}
bool MapEditor::cartOriginToRouteStart()
{
    auto& reg = MapConfigRegistry::instance();
    const PayloadConfig& payload = reg.payload();
    const PayloadRoutePoint* first = nullptr;
    for (const PayloadRoutePoint& point : payload.route) {
        if (!point.enabled) continue;
        first = &point;
        break;
    }
    if (!first || !payload.hasCartOrigin) {
        notify("needs a cart origin and an enabled route point");
        return false;
    }
    pushUndo("cart origin to route start");
    auto& mutablePayload = reg.payloadMutable();
    mutablePayload.cartOrigin = first->position;
    reg.markChanged();
    notify("cart origin moved to route start");
    return true;
}

bool MapEditor::routeStartToCartOrigin()
{
    auto& reg = MapConfigRegistry::instance();
    const PayloadConfig& payload = reg.payload();
    const PayloadRoutePoint* first = nullptr;
    for (const PayloadRoutePoint& point : payload.route) {
        if (!point.enabled) continue;
        first = &point;
        break;
    }
    if (!first || !payload.hasCartOrigin) {
        notify("needs a cart origin and an enabled route point");
        return false;
    }
    const glm::vec3 delta = payload.cartOrigin - first->position;
    if (glm::length(delta) <= 0.0001f) {
        notify("route start already matches the cart origin");
        return false;
    }
    pushUndo("route start to cart origin");
    auto& mutablePayload = reg.payloadMutable();
    for (PayloadRoutePoint& point : mutablePayload.route)
        point.position += delta;
    reg.markChanged();
    notify("route translated to the cart origin");
    return true;
}
void MapEditor::renderWorld(Camera& camera)
{
    if (!mActive) return;
    auto& reg = MapConfigRegistry::instance();
    const auto& config = reg.current();
    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    auto showLabel = [&](const EditorObjectRef& ref, bool hovered) {
        if (cfg.labelMode == "all") return true;
        if (cfg.labelMode == "off") return false;
        if (cfg.labelMode == "hovered") return hovered;
        return isSelected(ref) || hovered;
    };
    auto withAlpha = [](glm::vec4 color, float alpha) {
        color.a *= alpha;
        return color;
    };

    for (const BombSite& site : config.bombSites) {
        if (!site.hasPosition) continue;
        const bool selected = isSelected({EditorObjectKind::BombSite, site.id});
        const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                                         : glm::vec4(1, 0.35f, 0.35f, 1);
        // The usable area is a horizontal circular zone. Draw a thin filled
        // disc plus an outline ring so its radius is readable from any angle.
        DebugVis::drawFilledCylinder(camera, site.position, glm::vec3(0, 0, 1),
            site.radius, 0.02f,
            glm::vec4(color.r, color.g, color.b,
                (selected ? 0.24f : 0.12f) * cfg.bombSiteAlpha));
        constexpr int kSegments = 48;
        glm::vec3 previous{0.0f};
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = (float)i / (float)kSegments * 6.2831853f;
            const glm::vec3 point = site.position + glm::vec3(
                std::cos(angle) * site.radius, std::sin(angle) * site.radius, 0.0f);
            if (i > 0) DebugVis::drawLine(camera, previous, point, color);
            previous = point;
        }
        DebugVis::drawPointCross(camera, site.position, 0.5f, color);
        char label[128];
        std::snprintf(label, sizeof(label), "BOMB SITE %s r%.1f",
            site.id.c_str(), site.radius);
        if (showLabel({EditorObjectKind::BombSite, site.id}, false))
            DebugVis::drawWorldLabel(site.position + glm::vec3(0, 0, 0.8f),
                label, color);
    }

    if (config.payload.hasCartOrigin) {
        const bool selected = isSelected({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
        DebugVis::drawWireSphere(camera, config.payload.cartOrigin, 0.65f,
            selected ? glm::vec4(1, 0.9f, 0.2f, 1) : glm::vec4(1, 0.55f, 0.1f, 1));
        if (showLabel({EditorObjectKind::PayloadCartOrigin, "cart_origin"}, false))
            DebugVis::drawWorldLabel(config.payload.cartOrigin + glm::vec3(0, 0, 1),
                "CART ORIGIN", selected ? glm::vec4(1, 0.9f, 0.2f, 1) : glm::vec4(1, 0.55f, 0.1f, 1));
    }

    if (!config.payload.route.empty()) {
        for (size_t i = 0; i < config.payload.route.size(); ++i) {
            const auto& point = config.payload.route[i];
            const bool selected = isSelected({EditorObjectKind::PayloadRoutePoint, point.id});
            const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                : (!point.enabled ? glm::vec4(0.45f, 0.45f, 0.45f, 1)
                : (point.final ? glm::vec4(1.0f, 0.25f, 0.2f, 1)
                : (point.checkpoint ? glm::vec4(1.0f, 0.85f, 0.25f, 1)
                                    : glm::vec4(1, 0.45f, 0.1f, 1))));
            DebugVis::drawWireSphere(camera, point.position,
                std::max(0.3f, point.radius * 0.25f), color);
            DebugVis::drawPointCross(camera, point.position, 0.6f, color);
            char label[192];
            std::snprintf(label, sizeof(label),
                "ROUTE %d %s%s%s r%.1f w%d%s%.0fs",
                point.order, point.id.c_str(),
                point.enabled ? "" : " OFF",
                point.checkpoint ? " CP" : "",
                point.radius, point.waitTicks,
                point.final ? " FINAL" : "",
                point.timeBonusSeconds);
            if (showLabel({EditorObjectKind::PayloadRoutePoint, point.id}, false))
                DebugVis::drawWorldLabel(point.position + glm::vec3(0, 0, 0.7f), label, color);
            if (i + 1 < config.payload.route.size())
                DebugVis::drawLine(camera, point.position,
                    config.payload.route[i + 1].position, color);
        }
    }

    if (gpWorld) {
        for (const auto& object : gpWorld->mesh.objects) {
            const bool selected = isSelected({EditorObjectKind::MapGLBObject, object.id});
            const bool hovered = mHoverValid && mHover.kind == EditorObjectKind::MapGLBObject &&
                                 mHover.id == object.id;
            const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                : hovered ? glm::vec4(0.4f, 1, 1, 1)
                          : withAlpha(glm::vec4(0.45f, 0.8f, 0.45f, 1),
                                      cfg.glbAlpha);
            const glm::vec3 center = (object.boundsMin + object.boundsMax) * 0.5f;
            const glm::vec3 half = glm::max(
                (object.boundsMax - object.boundsMin) * 0.5f, glm::vec3(0.05f));
            DebugVis::drawWireBox(camera, center, half, color);
            DebugVis::drawWorldLabel(object.boundsMax + glm::vec3(0, 0, 0.3f),
                ("GLB " + object.name).c_str(), color);
        }
    }

    for (const auto& block : config.blocks) {
        if (!block.enabled) continue;
        const bool selected = isSelected({EditorObjectKind::Block, block.id});
        const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
            : withAlpha(glm::vec4(0.3f, 0.85f, 1, cfg.blockWireAlpha),
                        cfg.blockAlpha);
        drawOrientedBox(camera, block.position, block.size, block.rotationEuler, color);
        DebugVis::drawWorldLabel(block.position + glm::vec3(0, 0, block.size.z * 0.5f + 0.4f),
            block.id.c_str(), color);
    }

    for (const auto& vehicle : config.vehicles) {
        if (!vehicle.enabled) continue;
        const bool selected = isSelected({EditorObjectKind::Vehicle, vehicle.id});
        const bool hovered = mHoverValid && mHover.kind == EditorObjectKind::Vehicle &&
                             mHover.id == vehicle.id;
        const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
            : hovered ? glm::vec4(0.4f, 1, 1, 1)
                      : glm::vec4(0.9f, 0.55f, 0.2f, 1);
        const VehicleSpec* spec =
            VehicleConfigRegistry::instance().find(vehicle.templateId);
        const glm::vec3 half = spec ? spec->halfExtents : glm::vec3(2.5f, 1.5f, 0.5f);
        const glm::quat yawRotation =
            glm::angleAxis(vehicle.yaw, glm::vec3(0.0f, 0.0f, 1.0f));
        DebugVis::drawWireBox(camera, vehicle.position, half, color);
        if (spec) {
            for (size_t i = 0; i < spec->wheels.size(); ++i) {
                const VehicleWheelSpec& wheel = spec->wheels[i];
                const glm::vec3 hub = vehicle.position + yawRotation * wheel.hub;
                const glm::vec3 axle = yawRotation * wheel.spinAxis;
                DebugVis::drawFilledCylinder(
                    camera, hub, axle, wheel.radius, wheel.width,
                    glm::vec4(0.75f, 0.78f, 0.82f, selected ? 0.9f : 0.55f));
                DebugVis::drawWireSphere(camera, hub,
                    std::max(0.08f, wheel.radius * 0.2f), color);
            }
        }
        if (showLabel({EditorObjectKind::Vehicle, vehicle.id}, hovered))
            DebugVis::drawWorldLabel(
                vehicle.position + glm::vec3(0, 0, half.z + 0.6f),
                (vehicle.id + " " + vehicle.templateId).c_str(), color);
    }

    for (const auto& entity : config.entities) {
        if (!entity.enabled) continue;
        const bool selected = isSelected({EditorObjectKind::Entity, entity.id});
        const bool hovered = mHoverValid && mHover.kind == EditorObjectKind::Entity &&
                             mHover.id == entity.id;
        glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
            : hovered ? glm::vec4(0.4f, 1, 1, 1)
                      : withAlpha(glm::vec4(0.2f, 0.8f, 1, 1), cfg.entityAlpha);
        const glm::vec3 half = glm::max(entity.size * 0.5f, glm::vec3(0.25f));
        DebugVis::drawWireBox(camera, entity.position, half, color);
        DebugVis::drawWorldLabel(entity.position + glm::vec3(0, 0, half.z + 0.4f),
            (entity.type + " " + entity.id).c_str(), color);
    }

    if (mChunkPreviewValid) {
        for (const auto& chunk : config.chunks) {
            if (chunk.id != mChunkPreviewChunkId) continue;
            for (const auto& part : chunk.parts) {
                const EditorObjectRef ref{EditorObjectKind::ChunkPart,
                                          chunk.id + "/" + part.id};
                const bool selected = isSelected(ref);
                const bool hovered = mHoverValid && mHover.kind == ref.kind &&
                                     mHover.id == ref.id;
                const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                    : hovered ? glm::vec4(0.4f, 1, 1, 1)
                              : withAlpha(glm::vec4(0.55f, 0.85f, 1.0f, 0.8f),
                                          cfg.chunkPartAlpha);
                drawOrientedBox(camera, mChunkPreviewOrigin + part.position,
                                part.size, part.rotationEuler, color);
                if (showLabel(ref, hovered))
                    DebugVis::drawWorldLabel(
                        mChunkPreviewOrigin + part.position +
                            glm::vec3(0, 0, part.size.z * 0.5f + 0.4f),
                        ("PART " + part.id + " " + part.kind).c_str(), color);
            }
            for (const auto& connector : chunk.connectors) {
                const EditorObjectRef ref{EditorObjectKind::ChunkConnector,
                                          chunk.id + "/" + connector.id};
                const bool selected = isSelected(ref);
                const bool hovered = mHoverValid && mHover.kind == ref.kind &&
                                     mHover.id == ref.id;
                const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                    : hovered ? glm::vec4(0.4f, 1, 1, 1)
                              : withAlpha(glm::vec4(1.0f, 0.55f, 0.2f, 1),
                                          cfg.connectorAlpha);
                const glm::vec3 center = mChunkPreviewOrigin + connector.position;
                glm::vec3 direction(1, 0, 0);
                if (connector.side == "west") direction = {-1, 0, 0};
                else if (connector.side == "north") direction = {0, 1, 0};
                else if (connector.side == "south") direction = {0, -1, 0};
                else if (connector.side == "up") direction = {0, 0, 1};
                else if (connector.side == "down") direction = {0, 0, -1};
                DebugVis::drawWireSphere(camera, center, 0.35f, color);
                DebugVis::drawLine(camera, center, center + direction * 1.2f,
                                   color);
                if (showLabel(ref, hovered))
                    DebugVis::drawWorldLabel(
                        center + glm::vec3(0, 0, 0.5f),
                        ("CONNECTOR " + connector.id + " " + connector.side)
                            .c_str(),
                        color);
            }
            for (const auto& spawn : chunk.spawnPoints) {
                if (!spawn.enabled) continue;
                const EditorObjectRef ref{EditorObjectKind::ChunkSpawn,
                                          chunk.id + "/" + spawn.id};
                const bool selected = isSelected(ref);
                const bool hovered = mHoverValid && mHover.kind == ref.kind &&
                                     mHover.id == ref.id;
                const glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                    : hovered ? glm::vec4(0.4f, 1, 1, 1)
                              : withAlpha(glm::vec4(0.35f, 1.0f, 0.55f, 1),
                                          cfg.spawnAlpha);
                const glm::vec3 center = mChunkPreviewOrigin + spawn.position;
                DebugVis::drawWireSphere(camera, center, spawn.radius, color);
                DebugVis::drawLine(camera, center,
                                   center + glm::vec3(0, 0, 1.2f), color);
                if (showLabel(ref, hovered))
                    DebugVis::drawWorldLabel(
                        center + glm::vec3(0, 0, spawn.radius + 0.4f),
                        ("SPAWN " + spawn.id + " " + spawn.allowedActors).c_str(),
                        color);
            }
        }
    }

    if (!mSelection.empty()) {
        const glm::vec3 center = selectionCenter();
        const glm::vec3 axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
        const glm::vec4 colors[3] = {{1,0.2f,0.2f,1},{0.2f,1,0.2f,1},{0.3f,0.5f,1,1}};
        DebugVis::drawPointCross(camera, center, 0.3f, {1, 0.9f, 0.2f, 1});
        for (int i = 0; i < 3; ++i) {
            const glm::vec4 color = (mHoverAxis == i || mDrag.mode ==
                (i == 0 ? DragMode::AxisX : i == 1 ? DragMode::AxisY : DragMode::AxisZ))
                ? glm::vec4(1, 1, 1, 1) : colors[i];
            const glm::vec3 end = center + axes[i] * cfg.handleLength;
            DebugVis::drawFilledCylinder(camera, center + axes[i] * (cfg.handleLength * 0.5f),
                axes[i], cfg.handleThickness, cfg.handleLength, color);
            DebugVis::drawFilledSphere(camera, end, cfg.handleRadius, color);
            if (mSelection.size() == 1 &&
                (blockFor(mSelection.front()) || chunkPartFor(mSelection.front()))) {
                const glm::vec3 scaleEnd = center - axes[i] * cfg.handleLength;
                DebugVis::drawFilledSphere(camera, scaleEnd,
                    cfg.handleRadius * 1.25f,
                    mHoverAxis == i + 3 ? glm::vec4(1, 1, 1, 1) : colors[i]);
            }
        }
    }

    if (mSnapTargetValid)
        DebugVis::drawPointCross(camera, mSnapTarget, 0.45f, {1, 1, 0.2f, 1});
}

void MapEditor::renderUI(GLFWwindow* window)
{
    if (!mActive) return;
    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    const float w = uiScreenW();
    const float h = uiScreenH();
    GuiLayout& layout =
        GuiLayoutManager::instance().getLayout("config/gui/map-editor.json");
    auto element = [&](const char* id) { return layout.get(id); };
    auto rectOf = [&](const char* id, const UIRect& fallback) {
        return layout.getRect(id, fallback);
    };
    auto bgOf = [&](const char* id, const glm::vec4& fallback) {
        const GuiElement* el = element(id);
        return el ? el->getBackgroundColorVec() : fallback;
    };
    auto textOf = [&](const char* id, const glm::vec4& fallback) {
        const GuiElement* el = element(id);
        return el ? el->getTextColorVec() : fallback;
    };
    auto outlineOf = [&](const char* id, const glm::vec4& fallback) {
        const GuiElement* el = element(id);
        return el && el->hasOutlineColor() ? el->getOutlineColorVec() : fallback;
    };
    auto fontOf = [&](const char* id, float fallback) {
        const GuiElement* el = element(id);
        return el && el->fontSize > 0.0f ? el->fontSize : fallback;
    };

    const glm::vec4 panel = bgOf("map-editor.panel", {0.04f, 0.05f, 0.08f, 0.82f});
    const glm::vec4 text = textOf("map-editor.title", {0.9f, 0.95f, 1.0f, 1.0f});
    const glm::vec4 dim = textOf("map-editor.summary", {0.65f, 0.72f, 0.82f, 1.0f});

    const UIRect panelRect = rectOf(
        "map-editor.panel", {16.0f, 16.0f, 700.0f, 104.0f});
    uiDrawRect(panelRect, panel, "map-editor-panel");
    char line[512];
    std::snprintf(line, sizeof(line), "MAP EDITOR  %s%s  freeze=%s%s",
        mapId().c_str(), dirty() ? " *" : "",
        mFreezeWorld ? "on" : "off",
        mCollisionDirty ? "  collision pending" : "");
    const UIRect titleRect = rectOf(
        "map-editor.title", {26.0f, 24.0f, 0.0f, 0.0f});
    uiDrawText(line, titleRect.x, titleRect.y,
        fontOf("map-editor.title", cfg.titleScale), text);
    std::snprintf(line, sizeof(line), "selected %zu  blocks=%zu  routes=%zu  entities=%zu  glb=%zu",
        mSelection.size(), MapConfigRegistry::instance().current().blocks.size(),
        MapConfigRegistry::instance().current().payload.route.size(),
        MapConfigRegistry::instance().current().entities.size(),
        gpWorld ? gpWorld->mesh.objects.size() : 0);
    const UIRect summaryRect = rectOf(
        "map-editor.summary", {26.0f, 52.0f, 0.0f, 0.0f});
    uiDrawText(line, summaryRect.x, summaryRect.y,
        fontOf("map-editor.summary", cfg.bodyScale), dim);
    const UIRect statusRect = rectOf(
        "map-editor.status", {26.0f, 78.0f, 0.0f, 0.0f});
    uiDrawText(mStatus.c_str(), statusRect.x, statusRect.y,
        fontOf("map-editor.status", cfg.bodyScale),
        textOf("map-editor.status", {0.55f, 1.0f, 0.7f, 1.0f}));

    const glm::vec4 actionColor = bgOf(
        "map-editor.toolbar-button", {0.16f, 0.25f, 0.34f, 0.94f});
    const float actionFont = fontOf("map-editor.toolbar-button", 0.28f);
    const char* actions[] = {"Add Block", "Add Chunk", "Add Connector",
                             "Add Spawn", "Preview Connections", "Save",
                             "Reload"};
    const char* actionIds[] = {"map-editor.add-block", "map-editor.add-chunk",
                               "map-editor.add-connector",
                               "map-editor.add-spawn",
                               "map-editor.preview-connections",
                               "map-editor.save", "map-editor.reload"};
    static bool layoutLogged = false;
    if (!layoutLogged && !layout.elementIds().empty()) {
        layoutLogged = true;
        nlohmann::json rects = nlohmann::json::array();
        for (const char* id : actionIds) {
            const UIRect r = layout.getRect(id, {0, 0, 0, 0});
            rects.push_back({{"id", id}, {"x", r.x}, {"y", r.y},
                             {"w", r.w}, {"h", r.h}});
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Gui, StructuredLevel::Important,
            "map-editor.gui.layout", "config/gui/map-editor.json",
            "map editor toolbar layout loaded from JSON", 0,
            {{"path", "config/gui/map-editor.json"},
             {"elements", layout.elementIds().size()},
             {"toolbar", rects}},
            __FILE__, __LINE__, __func__);
    }
    for (int i = 0; i < 7; ++i) {
        const UIRect fallback{16.0f + i * 96.0f, 124.0f, 90.0f, 36.0f};
        const UIRect rect = layout.getRect(actionIds[i], fallback);
        if (uiButton(window, actions[i],
                     {rect.x, rect.y, rect.w, rect.h}, actionColor,
                     actions[i], nullptr, nullptr, nullptr, nullptr,
                     actionFont).clicked) {
            switch (i) {
            case 0: if (mChunkPreviewValid) addChunkPart(); else addBlock(); break;
            case 1: addChunk(); break;
            case 2: addConnector(); break;
            case 3: addChunkSpawn(); break;
            case 4: previewConnections(); break;
            case 5: save(); break;
            case 6: reload(); break;
            }
        }
    }

    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    const float px = w - cfg.outlinerWidth;
    const float py = cfg.outlinerTop;
    const float pw = cfg.outlinerWidth;
    const float ph = h - py - cfg.outlinerBottomPadding;
    uiDrawRect({px, py, pw, ph}, bgOf("map-editor.outliner", panel),
               "map-editor-outliner");
    std::snprintf(line, sizeof(line), "OBJECTS %zu", objects.size());
    uiDrawText(line, px + 10.0f, py + 8.0f,
        fontOf("map-editor.outliner-title", cfg.bodyScale),
        textOf("map-editor.outliner-title", text));

    UIRect searchRect{px + 8.0f, py + 32.0f, pw - 20.0f, 26.0f};
    UITextInputOptions searchOptions;
    searchOptions.maxLength = 80;
    searchOptions.placeholder = "search id / name / kind";
    searchOptions.placeholderWhileFocused = true;
    searchOptions.submitOnEnter = false;
    mOutlinerSearchFocused = uiTextInputRenderScreen(
        window, "mapeditor_search", searchRect, mOutlinerSearch, searchOptions);
    mOutlinerSearch.focused = mOutlinerSearchFocused;
    mOutlinerFilter = mOutlinerSearch.value;

    syncPropertyPositionFields();
    syncChunkPropertyFields();

    const float listY = py + kOutlinerHeaderHeight;
    const float listH = std::max(40.0f,
        ph - kOutlinerHeaderHeight - cfg.propertiesHeight - kOutlinerListGap);
    const UIRect listRect{px + 4.0f, listY, pw - 8.0f, listH};
    const float scrollbarSpace = cfg.scrollbarWidth + cfg.scrollbarInset + 6.0f;
    const float contentHeight = std::max(listH,
        (float)objects.size() * cfg.rowHeight + 4.0f);
    const double now = glfwGetTime();
    if (now - mLastLayoutLogTime >= 5.0 ||
        (int)objects.size() != mLastLayoutObjectCount) {
        mLastLayoutLogTime = now;
        mLastLayoutObjectCount = (int)objects.size();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Important,
            "map-editor.outliner_layout", mapId(),
            "screen-space outliner layout and visible object count", 0,
            {{"screen_w", w}, {"screen_h", h},
             {"panel_x", px}, {"panel_y", py}, {"panel_w", pw}, {"panel_h", ph},
             {"list_x", listRect.x}, {"list_y", listRect.y},
             {"list_w", listRect.w}, {"list_h", listRect.h},
             {"object_count", objects.size()},
             {"scroll_y", mOutlinerScroll.scrollY},
             {"scrollbar_width", cfg.scrollbarWidth},
             {"scrollbar_inset", cfg.scrollbarInset},
             {"properties_height", cfg.propertiesHeight}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    uiBeginScreenScrollArea(window, listRect, contentHeight, mOutlinerScroll,
        cfg.scrollbarWidth, cfg.scrollbarInset);
    const int firstRow = std::max(0, (int)(mOutlinerScroll.scrollY / cfg.rowHeight) - 1);
    const int visibleRows = (int)(listH / cfg.rowHeight) + 3;
    for (int row = firstRow; row < firstRow + visibleRows; ++row) {
        if (row < 0 || row >= (int)objects.size()) break;
        const EditorObjectRef& ref = objects[(size_t)row];
        const bool selected = isSelected(ref);
        const float rowY = listY + row * cfg.rowHeight - mOutlinerScroll.scrollY;
        if (selected) {
            uiDrawRect({px + 8.0f, rowY, listRect.w - scrollbarSpace - 8.0f,
                cfg.rowHeight - 2.0f},
                bgOf("map-editor.row-selected", {0.24f, 0.42f, 0.68f, 0.9f}),
                "map-editor-row-selected");
            uiDrawRectOutline({px + 8.0f, rowY, listRect.w - scrollbarSpace - 8.0f,
                cfg.rowHeight - 2.0f},
                outlineOf("map-editor.row-selected", {0.65f, 0.85f, 1.0f, 1.0f}),
                "map-editor-row-selected-outline");
        }
        glm::vec4 color = selected ? glm::vec4(1, 0.95f, 0.55f, 1) : text;
        if (!selected) {
            switch (ref.kind) {
                case EditorObjectKind::MapGLBObject:
                    color = textOf("map-editor.row-glb", {0.55f, 0.9f, 0.6f, 1}); break;
                case EditorObjectKind::BombSite:
                    color = textOf("map-editor.row-bombsite", {1, 0.35f, 0.35f, 1}); break;
                case EditorObjectKind::PayloadRoutePoint:
                    color = textOf("map-editor.row-route", {1, 0.55f, 0.2f, 1}); break;
                case EditorObjectKind::PayloadCartOrigin:
                    color = textOf("map-editor.row-cart", {1, 0.75f, 0.2f, 1}); break;
                case EditorObjectKind::Block:
                    color = textOf("map-editor.row-block", {0.4f, 0.8f, 1, 1}); break;
                case EditorObjectKind::Vehicle:
                    color = textOf("map-editor.row-vehicle", {1, 0.65f, 0.25f, 1}); break;
                case EditorObjectKind::Entity:
                    color = textOf("map-editor.row-entity", {0.7f, 0.8f, 1, 1}); break;
                case EditorObjectKind::ChunkPart:
                    color = textOf("map-editor.row-chunk-part", {0.55f, 0.85f, 1, 1}); break;
                case EditorObjectKind::ChunkConnector:
                    color = textOf("map-editor.row-connector", {1.0f, 0.65f, 0.2f, 1}); break;
                case EditorObjectKind::ChunkSpawn:
                    color = textOf("map-editor.row-spawn", {0.35f, 1.0f, 0.55f, 1}); break;
            }
        }
        char rowText[256];
        if (ref.kind == EditorObjectKind::MapGLBObject) {
            const MapGLBObject* object = mapObjectFor(ref);
            std::snprintf(rowText, sizeof(rowText), "%s  %s",
                ref.id.c_str(), object ? object->name.c_str() : "");
        } else {
            const std::string name = objectName(ref);
            if (name.empty() || name == ref.id) {
                std::snprintf(rowText, sizeof(rowText), "%s  %s",
                    kindName(ref.kind).c_str(), ref.id.c_str());
            } else {
                std::snprintf(rowText, sizeof(rowText), "%s  %s (%s)",
                    kindName(ref.kind).c_str(), name.c_str(), ref.id.c_str());
            }
        }
        uiDrawText(rowText, px + 12.0f, rowY + 5.0f, cfg.rowScale, color);
    }
    uiEndScreenScrollArea(listRect, contentHeight, mOutlinerScroll,
        cfg.scrollbarWidth, cfg.scrollbarInset);

    const float propY = listY + listH + kOutlinerListGap;
    const float propH = std::max(0.0f, py + ph - propY);
    uiDrawRect({px + 4.0f, propY, pw - 8.0f, propH},
        bgOf("map-editor.properties", {0.03f, 0.04f, 0.06f, 0.9f}),
        "map-editor-properties");
    if (mSelection.empty()) {
        uiDrawText("select an object to edit properties", px + 10.0f, propY + 10.0f,
            cfg.rowScale, dim);
        uiDrawText("search filters the list; Ctrl+click multi-selects",
            px + 10.0f, propY + 32.0f, cfg.rowScale, dim);
    } else {
        const EditorObjectRef& ref = mSelection.front();
        std::snprintf(line, sizeof(line), "SELECTED  %s", describeRef(ref).c_str());
        uiDrawText(line, px + 10.0f, propY + 8.0f, cfg.rowScale, text);

        const float fieldW = std::max(54.0f, (pw - 88.0f) / 3.0f);
        const auto renderVec3Fields = [&](const char* label, const char* idPrefix,
                                          UITextInputState (&fields)[3],
                                          float rowY) {
            uiDrawText(label, px + 12.0f, rowY + 5.0f, cfg.rowScale, dim);
            for (int i = 0; i < 3; ++i) {
                char id[48];
                std::snprintf(id, sizeof(id), "%s_%d", idPrefix, i);
                UITextInputOptions options;
                options.maxLength = 24;
                options.submitOnEnter = false;
                options.characterFilter = isNumberChar;
                UIRect field{px + 46.0f + i * (fieldW + 4.0f), rowY,
                             fieldW, 26.0f};
                fields[i].focused = uiTextInputRenderScreen(
                    window, id, field, fields[i], options);
            }
        };
        uiDrawText("name", px + 12.0f, propY + 33.0f, cfg.rowScale, dim);
        {
            UITextInputOptions options;
            options.maxLength = 64;
            options.submitOnEnter = false;
            UIRect field{px + 56.0f, propY + 28.0f, pw - 70.0f, 24.0f};
            mPropertyName.focused = uiTextInputRenderScreen(
                window, "mapeditor_name", field, mPropertyName, options);
        }
        renderVec3Fields("pos", "mapeditor_pos", mPropertyPosition, propY + 56.0f);
        renderVec3Fields("rot", "mapeditor_rot", mPropertyRotation, propY + 84.0f);
        renderVec3Fields("scale", "mapeditor_scale", mPropertyScale, propY + 112.0f);

        glm::vec3 rotation(0.0f);
        glm::vec3 scale(1.0f);
        std::string details;
        if (const AuthoredBlock* block = blockFor(ref)) {
            rotation = block->rotationEuler;
            scale = block->size;
            details = block->shape + " material=" +
                (block->material.empty() ? "default" : block->material);
        } else if (const MapVehicleSpawn* vehicle = vehicleFor(ref)) {
            rotation.z = glm::degrees(vehicle->yaw);
            if (const VehicleSpec* spec =
                    VehicleConfigRegistry::instance().find(vehicle->templateId)) {
                scale = spec->halfExtents * 2.0f;
                details = "template=" + vehicle->templateId +
                    " wheels=" + std::to_string(spec->wheels.size()) +
                    " seats=" + std::to_string(spec->seats.size());
            } else {
                details = "MISSING template=" + vehicle->templateId;
            }
        } else if (const MapEntity* entity = entityFor(ref)) {
            rotation.z = entity->yaw;
            scale = entity->size;
            details = entity->type + " tag=" + entity->tag +
                " radius=" + std::to_string(entity->radius);
        } else if (ref.kind == EditorObjectKind::BombSite) {
            if (const BombSite* site = MapConfigRegistry::instance().findSite(ref.id)) {
                scale = glm::vec3(site->radius);
                details = "bombsite radius=" + std::to_string(site->radius) +
                    (site->visibleDebug ? " visible" : " hidden");
            }
        } else if (const PayloadRoutePoint* route = routeFor(ref)) {
            scale = glm::vec3(route->radius);
            details = "order=" + std::to_string(route->order) +
                (route->checkpoint ? " checkpoint" : "") +
                (route->final ? " final" : "") +
                " wait=" + std::to_string(route->waitTicks) +
                " bonus=" + std::to_string(route->timeBonusSeconds);
        } else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
            rotation.z = MapConfigRegistry::instance().payload().cartYaw;
            details = "payload cart origin";
        } else if (const MapGLBObject* object = mapObjectFor(ref)) {
            const glm::mat4& t = object->transform;
            const glm::mat3 basis(t);
            scale = glm::vec3(glm::length(basis[0]), glm::length(basis[1]),
                glm::length(basis[2]));
            glm::mat3 rotationBasis = basis;
            for (int i = 0; i < 3; ++i)
                if (scale[i] > 1e-5f) rotationBasis[i] /= scale[i];
            rotation = glm::degrees(glm::eulerAngles(glm::quat_cast(rotationBasis)));
            details = object->name + " verts=" + std::to_string(object->vertexCount);
        } else if (const ChunkPart* part = chunkPartFor(ref)) {
            rotation = part->rotationEuler;
            scale = part->size;
            details = part->kind + " material=" +
                (part->material.empty() ? "default" : part->material) +
                " collision=" + (part->collision ? "1" : "0") +
                " visible=" + (part->visible ? "1" : "0");
        } else if (const AuthoredChunkConnector* connector =
                       chunkConnectorFor(ref)) {
            std::string tags;
            for (const std::string& tag : connector->tags) tags += " " + tag;
            std::string links;
            for (const std::string& link : connector->links)
                links += " " + link;
            details = "side=" + connector->side +
                " role=" + connector->role +
                " orientation=" + connector->orientation +
                " radius=" + std::to_string(connector->radius) +
                " tags=[" + tags + " ] links=[" + links + " ]";
        } else if (const ChunkSpawnPoint* spawn = chunkSpawnFor(ref)) {
            details = "allowed=" + spawn->allowedActors +
                " radius=" + std::to_string(spawn->radius) +
                " enabled=" + (spawn->enabled ? "1" : "0") +
                " chunk=" + mChunkPreviewChunkId;
        }
        uiDrawText(details.c_str(), px + 10.0f, propY + 145.0f, cfg.rowScale, dim);
        if (ref.kind == EditorObjectKind::ChunkPart) {
            uiDrawText("material", px + 12.0f, propY + 169.0f, cfg.rowScale, dim);
            UITextInputOptions materialOptions;
            materialOptions.maxLength = 64;
            materialOptions.submitOnEnter = false;
            materialOptions.placeholder = "default";
            UIRect materialField{px + 84.0f, propY + 165.0f,
                                 pw - 100.0f, 24.0f};
            mPropertyMaterial.focused = uiTextInputRenderScreen(
                window, "mapeditor_part_material", materialField,
                mPropertyMaterial, materialOptions);
            uiDrawText("uv", px + 12.0f, propY + 197.0f, cfg.rowScale, dim);
            for (int i = 0; i < 2; ++i) {
                char id[48];
                std::snprintf(id, sizeof(id), "mapeditor_part_uv_%d", i);
                UITextInputOptions options;
                options.maxLength = 24;
                options.submitOnEnter = false;
                options.characterFilter = isNumberChar;
                UIRect field{px + 84.0f + i * 78.0f, propY + 193.0f, 70.0f,
                             24.0f};
                mPropertyUvScale[i].focused = uiTextInputRenderScreen(
                    window, id, field, mPropertyUvScale[i], options);
            }
        } else {
            uiDrawText("F2 rename | Enter applies | Ctrl+Z undo", px + 10.0f,
                propY + 172.0f, cfg.rowScale, dim);
        }
    }

    if (mContextMenuOpen) {
        const float menuW = 210.0f;
        const float rowH = 28.0f;
        const float menuH = rowH * 5.0f + 10.0f;
        const float mx = std::min(mContextMenuPos.x, w - menuW - 8.0f);
        const float my = std::min(mContextMenuPos.y, h - menuH - 8.0f);
        uiDrawRect({mx, my, menuW, menuH},
                   bgOf("map-editor.context-menu", {0.05f, 0.06f, 0.09f, 0.96f}),
                   "map-editor-context-menu");
        uiDrawRectOutline({mx, my, menuW, menuH},
                          outlineOf("map-editor.context-menu",
                                    {0.5f, 0.7f, 0.9f, 1.0f}),
                          "map-editor-context-menu-outline");
        const char* items[] = {"Add Attachment", "Anchor", "Unanchor",
                               "Remove Attachment", "Close"};
        const float contextFont = fontOf("map-editor.context-button", 0.28f);
        for (int i = 0; i < 5; ++i) {
            if (uiButton(window, items[i],
                         {mx + 5.0f, my + 5.0f + i * rowH,
                          menuW - 10.0f, rowH - 2.0f},
                         bgOf("map-editor.context-button",
                              {0.14f, 0.20f, 0.28f, 0.95f}),
                         items[i], nullptr, nullptr, nullptr, nullptr,
                         contextFont).clicked) {
                if (i == 0) createAttachment();
                else if (i == 1) setAttachmentAnchored(true);
                else if (i == 2) setAttachmentAnchored(false);
                else if (i == 3) removeAttachmentForSelection();
                mContextMenuOpen = false;
                return;
            }
        }
    }

    if (mBoxSelecting) {
        const float minX = std::min(mBoxStartScreen.x, mBoxEndScreen.x);
        const float maxX = std::max(mBoxStartScreen.x, mBoxEndScreen.x);
        const float minY = std::min(mBoxStartScreen.y, mBoxEndScreen.y);
        const float maxY = std::max(mBoxStartScreen.y, mBoxEndScreen.y);
        uiDrawRect({minX, minY, maxX - minX, maxY - minY},
                   bgOf("map-editor.box-select-fill",
                        {0.25f, 0.55f, 1.0f, 0.12f}),
                   "map-editor-box-select-fill");
        uiDrawRectOutline({minX, minY, maxX - minX, maxY - minY},
                          outlineOf("map-editor.box-select-outline",
                                    {0.4f, 0.8f, 1.0f, 1.0f}),
                          "map-editor-box-select");
    }

    {
        const MapObjectiveConfig& mapConfig = MapConfigRegistry::instance().current();
        const std::string project = currentWorldProjectId().empty()
            ? "none" : currentWorldProjectId();
        const std::string worldLine =
            "world project=" + project +
            " chunks=" + std::to_string(mapConfig.chunks.size()) +
            " zones=" + std::to_string(mapConfig.generationZones.size()) +
            " interactions=" + std::to_string(mapConfig.interactions.size()) +
            " | add chunk/zone: mapeditor add chunk <glb> / add zone <seed>";
        const UIRect helpWorld = rectOf(
            "map-editor.help-world", {16.0f, h - 42.0f, 0.0f, 0.0f});
        uiDrawText(worldLine.c_str(), helpWorld.x,
            helpWorld.y > 0.0f ? helpWorld.y : h - 42.0f,
            fontOf("map-editor.help-world", cfg.helpScale),
            textOf("map-editor.help-world", dim));
    }
    const UIRect helpCommands = rectOf(
        "map-editor.help-commands", {16.0f, h - 22.0f, 0.0f, 0.0f});
    uiDrawText("Commands are in docs/specs/entity-editor/entity-editor.md",
        helpCommands.x,
        helpCommands.y > 0.0f ? helpCommands.y : h - 22.0f,
        fontOf("map-editor.help-commands", cfg.helpScale),
        textOf("map-editor.help-commands", dim));
}
