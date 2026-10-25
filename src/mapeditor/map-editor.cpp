// 2026-10-23
#include "mapeditor/map-editor.h"
#include "mapeditor/map-editor-config.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.h"
#include "config.h"
#include "debug/debug-log.h"
#include "debug/debug-visuals.h"
#include "debug/structured-log.h"
#include "devtools/terminal.h"
#include "gamemode/map-config.h"
#include "game/game-state.h"
#include "gui/hud/chat-window.h"
#include "gui/menus/pause-menu.h"
#include "gui/ui-system.h"
#include "input/mouse-lock.h"
#include "map/map-authored-geometry.h"
#include "map/map-loader-collision.h"
#include "network/multiplayer-context.h"
#include "network/server-gamemode.h"
#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "renderer/renderer.h"
#include "terminal/terminal-state.h"
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
    if (loaded != mapId || reg.mapId() != mapId) {
        reg.load(mapId);
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
}

void MapEditor::toggleSelection(const EditorObjectRef& ref)
{
    const auto it = std::find(mSelection.begin(), mSelection.end(), ref);
    if (it == mSelection.end()) mSelection.push_back(ref);
    else mSelection.erase(it);
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

glm::vec3 MapEditor::objectPosition(const EditorObjectRef& ref) const
{
    if (ref.kind == EditorObjectKind::PayloadCartOrigin)
        return MapConfigRegistry::instance().payload().cartOrigin;
    if (const auto* object = mapObjectFor(ref)) return glm::vec3(object->transform[3]);
    if (const auto* entity = entityFor(ref)) return entity->position;
    if (const auto* block = blockFor(ref)) return block->position;
    if (const auto* route = routeFor(ref)) return route->position;
    return glm::vec3(0.0f);
}

void MapEditor::setObjectPosition(const EditorObjectRef& ref, const glm::vec3& position)
{
    auto& reg = MapConfigRegistry::instance();
    if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
        auto& payload = reg.payloadMutable();
        payload.present = true;
        payload.hasCartOrigin = true;
        payload.cartOrigin = position;
        reg.markChanged();
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
            block->position = position;
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
    int bestAxis = -1;
    float bestDistance = cfg.handlePickPixels;
    for (int i = 0; i < 3; ++i) {
        float ex = 0.0f, ey = 0.0f;
        if (!DebugVis::projectToScreen(THE_CAMERA, center + axes[i] * cfg.handleLength, ex, ey))
            continue;
        const float distance = pointSegmentDistance(px, py, cx, cy, ex, ey);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAxis = i;
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
    if (mDrag.mode == DragMode::ObjectPlane) {
        delta = THE_CAMERA.right * (mouseDelta.x * unitsPerPixel) +
                THE_CAMERA.up * (-mouseDelta.y * unitsPerPixel);
    } else {
        const glm::vec3 axis = mDrag.mode == DragMode::AxisX ? glm::vec3(1, 0, 0) :
                               mDrag.mode == DragMode::AxisY ? glm::vec3(0, 1, 0) :
                                                               glm::vec3(0, 0, 1);
        float cx = 0.0f, cy = 0.0f, ex = 0.0f, ey = 0.0f;
        const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
        if (DebugVis::projectToScreen(THE_CAMERA, mDrag.startCenter, cx, cy) &&
            DebugVis::projectToScreen(THE_CAMERA,
                mDrag.startCenter + axis * cfg.handleLength, ex, ey)) {
            const glm::vec2 axisScreen(ex - cx, ey - cy);
            const float axisScreenLength = glm::length(axisScreen);
            if (axisScreenLength > 1.0f) {
                const float alongPixels = glm::dot(mouseDelta, axisScreen / axisScreenLength);
                delta = axis * (alongPixels * cfg.handleLength / axisScreenLength);
            }
        }
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
    if (commit && moved && mDrag.hasPendingUndo) {
        mUndo.push_back(std::move(mDrag.pendingUndo));
        mUndoLabels.push_back("move " + describeSelection());
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
        notify("moved " + std::to_string(mSelection.size()) + " object(s)");
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
        case EditorObjectKind::PayloadRoutePoint: return "route";
        case EditorObjectKind::PayloadCartOrigin: return "cart";
        case EditorObjectKind::MapGLBObject: return "glb";
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
        else if (ref.kind == EditorObjectKind::PayloadRoutePoint)
            MapConfigRegistry::instance().deletePayloadRoutePoint(ref.id);
        else if (ref.kind == EditorObjectKind::PayloadCartOrigin) {
            auto& payload = MapConfigRegistry::instance().payloadMutable();
            payload.hasCartOrigin = false;
            MapConfigRegistry::instance().markChanged();
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
        else if (ref.kind == EditorObjectKind::PayloadRoutePoint)
            MapConfigRegistry::instance().deletePayloadRoutePoint(ref.id);
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
    for (const auto& route : config.payload.route) {
        if (matches(route.id + " route"))
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
            matches(entity.id + " " + entity.type + " " + entity.tag + " entity"))
            out.push_back({EditorObjectKind::Entity, entity.id});
    }
    for (const auto& block : config.blocks) {
        if (block.enabled && matches(block.id + " block"))
            out.push_back({EditorObjectKind::Block, block.id});
    }
}

int MapEditor::outlinerRowAt(double screenX, double screenY) const
{
    GLFWwindow* window = glfwGetCurrentContext();
    const glm::vec2 cursor = cursorScreen(window, screenX, screenY);
    const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
    const float x = uiScreenW() - cfg.outlinerWidth;
    const float y = cfg.outlinerTop;
    const float panelHeight = uiScreenH() - y - cfg.outlinerBottomPadding;
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
    for (const UITextInputState& field : mPropertyPosition)
        if (field.focused) return true;
    return false;
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
    for (UITextInputState& field : mPropertyPosition) {
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
    return false;
}

void MapEditor::syncPropertyPositionFields()
{
    if (mSelection.empty()) {
        if (mPropertyOwnerValid) {
            mPropertyOwnerValid = false;
            for (UITextInputState& field : mPropertyPosition) field.focused = false;
        }
        return;
    }
    const EditorObjectRef& ref = mSelection.front();
    const bool ownerChanged = !mPropertyOwnerValid || !(mPropertyOwner == ref);
    if (ownerChanged) {
        mPropertyOwner = ref;
        mPropertyOwnerValid = true;
        for (UITextInputState& field : mPropertyPosition) field.focused = false;
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
    const bool right = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (right && !mRightPrev) {
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
        const int row = outlinerRowAt(mMouseX, mMouseY);
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
        const MapEditorConfigData& cfg = MapEditorConfig::instance().data();
        const bool overTopPanel = mMouseX >= 16.0 && mMouseX <= 716.0 &&
                                  mMouseY >= 16.0 && mMouseY <= 120.0;
        const bool overOutliner = mMouseX >= uiScreenW() - cfg.outlinerWidth &&
                                  mMouseY >= cfg.outlinerTop;
        const bool overHelp = mMouseY >= uiScreenH() - 30.0;
        if (overTopPanel || overOutliner || overHelp) {
            mLeftPrev = left;
            return;
        }
    }
    if (left && !mLeftPrev && mDrag.mode == DragMode::None) {
        EditorObjectRef handleRef;
        int axis = -1;
        if (handleAtCursor(handleRef, axis)) {
            beginDrag(handleRef, axis == 0 ? DragMode::AxisX :
                              axis == 1 ? DragMode::AxisY : DragMode::AxisZ,
                      selectionCenter());
        } else {
            EditorObjectRef hit;
            float distance = 0.0f;
            const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (pickAtCursor(hit, distance)) {
                if (shift) toggleSelection(hit);
                else if (!isSelected(hit)) selectOnly(hit);
                if (!mSelection.empty())
                    beginDrag(mSelection.front(), DragMode::ObjectPlane,
                              selectionCenter());
            } else if (!shift) {
                clearSelection();
            }
        }
    } else if (!left && mLeftPrev && mDrag.mode != DragMode::None) {
        endDrag(true);
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
        mHoverAxis = handleAtCursor(handleRef, axis) ? axis : -1;
    }
    mLeftPrev = left;
}

void MapEditor::selectAllEdit()
{
    mSelection.clear();
    for (const auto& entity : MapConfigRegistry::instance().current().entities)
        if (entity.enabled) mSelection.push_back({EditorObjectKind::Entity, entity.id});
    for (const auto& block : MapConfigRegistry::instance().current().blocks)
        if (block.enabled) mSelection.push_back({EditorObjectKind::Block, block.id});
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
    else if (normalized == "route" || normalized == "payload")
        want = EditorObjectKind::PayloadRoutePoint;
    else if (normalized == "cart") want = EditorObjectKind::PayloadCartOrigin;
    else if (normalized == "glb") want = EditorObjectKind::MapGLBObject;
    else {
        notify("select_kind expects entity, block, route, cart, glb, or all");
        return false;
    }
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    mSelection.clear();
    for (const EditorObjectRef& ref : objects)
        if (ref.kind == want) mSelection.push_back(ref);
    if (mSelection.empty()) {
        notify("no " + kind + " objects to select");
        return false;
    }
    notify("selected " + std::to_string(mSelection.size()) + " " + kind + " object(s)");
    return true;
}

bool MapEditor::setSelectedPosition(const glm::vec3& position)
{
    if (mSelection.empty()) {
        notify("select an object first");
        return false;
    }
    const glm::vec3 delta = position - objectPosition(mSelection.front());
    if (glm::length(delta) <= 0.0001f) return true;
    nudgeSelection(delta);
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
                                                       : glm::vec4(0.45f, 0.8f, 0.45f, 1);
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
            : glm::vec4(0.3f, 0.85f, 1, cfg.blockWireAlpha);
        drawOrientedBox(camera, block.position, block.size, block.rotationEuler, color);
        DebugVis::drawWorldLabel(block.position + glm::vec3(0, 0, block.size.z * 0.5f + 0.4f),
            block.id.c_str(), color);
    }

    for (const auto& entity : config.entities) {
        if (!entity.enabled) continue;
        const bool selected = isSelected({EditorObjectKind::Entity, entity.id});
        const bool hovered = mHoverValid && mHover.kind == EditorObjectKind::Entity &&
                             mHover.id == entity.id;
        glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1)
                                   : hovered ? glm::vec4(0.4f, 1, 1, 1)
                                             : glm::vec4(0.2f, 0.8f, 1, 1);
        const glm::vec3 half = glm::max(entity.size * 0.5f, glm::vec3(0.25f));
        DebugVis::drawWireBox(camera, entity.position, half, color);
        DebugVis::drawWorldLabel(entity.position + glm::vec3(0, 0, half.z + 0.4f),
            (entity.type + " " + entity.id).c_str(), color);
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
    const glm::vec4 panel(0.04f, 0.05f, 0.08f, 0.82f);
    const glm::vec4 text(0.9f, 0.95f, 1.0f, 1.0f);
    const glm::vec4 dim(0.65f, 0.72f, 0.82f, 1.0f);

    uiDrawRect({16.0f, 16.0f, 700.0f, 104.0f}, panel, "map-editor-panel");
    char line[512];
    std::snprintf(line, sizeof(line), "MAP EDITOR  %s%s  freeze=%s%s",
        mapId().c_str(), dirty() ? " *" : "",
        mFreezeWorld ? "on" : "off",
        mCollisionDirty ? "  collision pending" : "");
    uiDrawText(line, 26.0f, 24.0f, cfg.titleScale, text);
    std::snprintf(line, sizeof(line), "selected %zu  blocks=%zu  routes=%zu  entities=%zu  glb=%zu",
        mSelection.size(), MapConfigRegistry::instance().current().blocks.size(),
        MapConfigRegistry::instance().current().payload.route.size(),
        MapConfigRegistry::instance().current().entities.size(),
        gpWorld ? gpWorld->mesh.objects.size() : 0);
    uiDrawText(line, 26.0f, 52.0f, cfg.bodyScale, dim);
    uiDrawText(mStatus.c_str(), 26.0f, 78.0f, cfg.bodyScale,
        glm::vec4(0.55f, 1.0f, 0.7f, 1.0f));

    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    const float px = w - cfg.outlinerWidth;
    const float py = cfg.outlinerTop;
    const float pw = cfg.outlinerWidth;
    const float ph = h - py - cfg.outlinerBottomPadding;
    uiDrawRect({px, py, pw, ph}, panel, "map-editor-outliner");
    std::snprintf(line, sizeof(line), "OBJECTS %zu", objects.size());
    uiDrawText(line, px + 10.0f, py + 8.0f, cfg.bodyScale, text);

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
                {0.24f, 0.42f, 0.68f, 0.9f}, "map-editor-row-selected");
            uiDrawRectOutline({px + 8.0f, rowY, listRect.w - scrollbarSpace - 8.0f,
                cfg.rowHeight - 2.0f},
                {0.65f, 0.85f, 1.0f, 1.0f}, "map-editor-row-selected-outline");
        }
        glm::vec4 color = selected ? glm::vec4(1, 0.95f, 0.55f, 1) : text;
        if (!selected) {
            switch (ref.kind) {
                case EditorObjectKind::MapGLBObject: color = {0.55f, 0.9f, 0.6f, 1}; break;
                case EditorObjectKind::PayloadRoutePoint: color = {1, 0.55f, 0.2f, 1}; break;
                case EditorObjectKind::PayloadCartOrigin: color = {1, 0.75f, 0.2f, 1}; break;
                case EditorObjectKind::Block: color = {0.4f, 0.8f, 1, 1}; break;
                case EditorObjectKind::Entity: color = {0.7f, 0.8f, 1, 1}; break;
            }
        }
        char rowText[256];
        if (ref.kind == EditorObjectKind::MapGLBObject) {
            const MapGLBObject* object = mapObjectFor(ref);
            std::snprintf(rowText, sizeof(rowText), "%s  %s",
                ref.id.c_str(), object ? object->name.c_str() : "");
        } else {
            std::snprintf(rowText, sizeof(rowText), "%s  %s",
                kindName(ref.kind).c_str(), ref.id.c_str());
        }
        uiDrawText(rowText, px + 12.0f, rowY + 5.0f, cfg.rowScale, color);
    }
    uiEndScreenScrollArea(listRect, contentHeight, mOutlinerScroll,
        cfg.scrollbarWidth, cfg.scrollbarInset);

    const float propY = listY + listH + kOutlinerListGap;
    const float propH = std::max(0.0f, py + ph - propY);
    uiDrawRect({px + 4.0f, propY, pw - 8.0f, propH},
        {0.03f, 0.04f, 0.06f, 0.9f}, "map-editor-properties");
    if (mSelection.empty()) {
        uiDrawText("select an object to edit properties", px + 10.0f, propY + 10.0f,
            cfg.rowScale, dim);
        uiDrawText("search filters the list; Ctrl+click multi-selects",
            px + 10.0f, propY + 32.0f, cfg.rowScale, dim);
    } else {
        const EditorObjectRef& ref = mSelection.front();
        std::snprintf(line, sizeof(line), "SELECTED  %s", describeRef(ref).c_str());
        uiDrawText(line, px + 10.0f, propY + 8.0f, cfg.rowScale, text);

        const glm::vec3 position = objectPosition(ref);
        uiDrawText("pos", px + 12.0f, propY + 33.0f, cfg.rowScale, dim);
        const float fieldW = std::max(54.0f, (pw - 88.0f) / 3.0f);
        for (int i = 0; i < 3; ++i) {
            char id[48];
            std::snprintf(id, sizeof(id), "mapeditor_pos_%d", i);
            UITextInputOptions options;
            options.maxLength = 24;
            options.submitOnEnter = false;
            options.characterFilter = isNumberChar;
            UIRect field{px + 46.0f + i * (fieldW + 4.0f), propY + 28.0f, fieldW, 26.0f};
            mPropertyPosition[i].focused = uiTextInputRenderScreen(
                window, id, field, mPropertyPosition[i], options);
        }

        glm::vec3 rotation(0.0f);
        glm::vec3 scale(1.0f);
        std::string details;
        if (const AuthoredBlock* block = blockFor(ref)) {
            rotation = block->rotationEuler;
            scale = block->size;
            details = block->shape + " material=" +
                (block->material.empty() ? "default" : block->material);
        } else if (const MapEntity* entity = entityFor(ref)) {
            rotation.z = entity->yaw;
            scale = entity->size;
            details = entity->type + " tag=" + entity->tag +
                " radius=" + std::to_string(entity->radius);
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
        }
        std::snprintf(line, sizeof(line), "rot %.1f %.1f %.1f   scale %.2f %.2f %.2f",
            rotation.x, rotation.y, rotation.z, scale.x, scale.y, scale.z);
        uiDrawText(line, px + 10.0f, propY + 64.0f, cfg.rowScale, dim);
        uiDrawText(details.c_str(), px + 10.0f, propY + 88.0f, cfg.rowScale, dim);
        uiDrawText("Enter applies | Ctrl+Z undo", px + 10.0f, propY + 112.0f,
            cfg.rowScale, dim);
    }

    uiDrawText("Commands are in docs/specs/entity-editor/entity-editor.md",
        16.0f, h - 22.0f, cfg.helpScale, dim);
}