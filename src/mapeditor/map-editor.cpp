// 2026-10-23
#include "mapeditor/map-editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

constexpr float kHandleLength = 1.6f;
constexpr float kHandlePickPixels = 14.0f;
constexpr float kVertexSnapRadius = 0.85f;
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

    int bestAxis = -1;
    float bestDistance = kHandlePickPixels;
    for (int i = 0; i < 3; ++i) {
        float ex = 0.0f, ey = 0.0f;
        if (!DebugVis::projectToScreen(THE_CAMERA, center + axes[i] * kHandleLength, ex, ey))
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
    mDrag.refs = mSelection;
    if (mDrag.refs.empty()) mDrag.refs.push_back(primary);
    for (const auto& ref : mDrag.refs) {
        mDrag.originalPositions.push_back(objectPosition(ref));
        if (const auto* block = blockFor(ref))
            mDrag.originalRotations.push_back(block->rotationEuler);
        else
            mDrag.originalRotations.push_back(glm::vec3(0.0f));
    }
    mDrag.pendingUndo = MapConfigRegistry::instance().snapshotAuthored();
    mDrag.hasPendingUndo = true;
    mDrag.moved = false;
}

void MapEditor::updateDrag()
{
    if (mDrag.mode == DragMode::None) return;
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f};
    if (!rayFromCursor(origin, direction)) return;

    glm::vec3 delta(0.0f);
    if (mDrag.mode == DragMode::ObjectPlane) {
        const glm::vec3 normal = THE_CAMERA.front;
        const float denom = glm::dot(normal, direction);
        if (std::abs(denom) < 1e-5f) return;
        const float t = glm::dot(mDrag.startCenter - origin, normal) / denom;
        if (t < 0.0f) return;
        const glm::vec3 point = origin + direction * t;
        delta = point - mDrag.startPoint;
    } else {
        const glm::vec3 axis = mDrag.mode == DragMode::AxisX ? glm::vec3(1, 0, 0) :
                               mDrag.mode == DragMode::AxisY ? glm::vec3(0, 1, 0) :
                                                               glm::vec3(0, 0, 1);
        const glm::vec3 point = rayAxisPoint(origin, direction, mDrag.startCenter, axis);
        delta = axis * glm::dot(point - mDrag.startPoint, axis);
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
        rebuildGeometry();
    }
    mDrag = DragState{};
    if (moved && commit) {
        if (selectionHasBlocksOrRoutes()) rebuildGeometry();
        notify("moved " + std::to_string(mSelection.size()) + " object(s)");
    }
}

void MapEditor::pushUndo(const std::string& label)
{
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
    rebuildGeometry();
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
    rebuildGeometry();
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
    if (const auto& payload = config.payload; payload.hasCartOrigin)
        out.push_back({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
    for (const auto& route : config.payload.route)
        out.push_back({EditorObjectKind::PayloadRoutePoint, route.id});
    if (gpWorld)
        for (const auto& object : gpWorld->mesh.objects)
            out.push_back({EditorObjectKind::MapGLBObject, object.id});
    for (const auto& entity : config.entities)
        if (entity.enabled)
            out.push_back({EditorObjectKind::Entity, entity.id});
    for (const auto& block : config.blocks)
        if (block.enabled)
            out.push_back({EditorObjectKind::Block, block.id});
}

int MapEditor::outlinerRowAt(double screenX, double screenY) const
{
    GLFWwindow* window = glfwGetCurrentContext();
    const glm::vec2 cursor = cursorScreen(window, screenX, screenY);
    const float x = uiScreenW() - 360.0f;
    const float y = 120.0f;
    constexpr float kRowHeight = 22.0f;
    constexpr float kHeaderHeight = 44.0f;
    if (cursor.x < x || cursor.x > x + 340.0f ||
        cursor.y < y + kHeaderHeight || cursor.y > uiScreenH() - 48.0f)
        return -1;
    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    const int row = (int)std::floor((cursor.y - (y + kHeaderHeight)) / kRowHeight);
    const int index = row + mOutlinerOffset;
    return (index >= 0 && index < (int)objects.size()) ? row : -1;
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
            const int index = row + mOutlinerOffset;
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
    if (left && !mLeftPrev && mDrag.mode == DragMode::None) {
        EditorObjectRef handleRef;
        int axis = -1;
        if (handleAtCursor(handleRef, axis)) {
            glm::vec3 origin{0.0f}, direction{0.0f};
            if (rayFromCursor(origin, direction)) {
                const glm::vec3 axisDir = axis == 0 ? glm::vec3(1, 0, 0) :
                                          axis == 1 ? glm::vec3(0, 1, 0) :
                                                      glm::vec3(0, 0, 1);
                const glm::vec3 start = rayAxisPoint(origin, direction,
                                                     selectionCenter(), axisDir);
                beginDrag(handleRef, axis == 0 ? DragMode::AxisX :
                                  axis == 1 ? DragMode::AxisY : DragMode::AxisZ,
                          start);
            }
        } else {
            EditorObjectRef hit;
            float distance = 0.0f;
            const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                               glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            if (pickAtCursor(hit, distance)) {
                if (shift) toggleSelection(hit);
                else if (!isSelected(hit)) selectOnly(hit);
                if (!mSelection.empty()) {
                    glm::vec3 origin{0.0f}, direction{0.0f};
                    if (rayFromCursor(origin, direction)) {
                        const glm::vec3 normal = THE_CAMERA.front;
                        const float denom = glm::dot(normal, direction);
                        if (std::abs(denom) > 1e-5f) {
                            const float t = glm::dot(selectionCenter() - origin, normal) / denom;
                            const glm::vec3 start = t >= 0.0f
                                ? origin + direction * t : selectionCenter();
                            beginDrag(mSelection.front(), DragMode::ObjectPlane, start);
                        }
                    }
                }
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
        if (!mSelection.empty()) {
            const glm::vec3 center = selectionCenter();
            THE_CAMERA.pos = center - THE_CAMERA.front * 8.0f;
            THE_CAMERA.firstMouse = true;
            notify("framed " + describeSelection());
        }
        return true;
    }
    if (key == GLFW_KEY_ESCAPE) { handleEscape(); return true; }
    if (key == GLFW_KEY_PAGE_UP) { mOutlinerOffset = std::max(0, mOutlinerOffset - 20); return true; }
    if (key == GLFW_KEY_PAGE_DOWN) { mOutlinerOffset += 20; return true; }
    return false;
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

bool MapEditor::save()
{
    const bool ok = MapConfigRegistry::instance().save();
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
    if (gpWorld->authoredGeometryApplied)
        clearAuthoredMapGeometry(*gpWorld);
    gpWorld->authoredGeometryApplied = false;
    applyAuthoredMapGeometry(*gpWorld, mapId());
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

void MapEditor::renderWorld(Camera& camera)
{
    if (!mActive) return;
    auto& reg = MapConfigRegistry::instance();
    const auto& config = reg.current();

    if (config.payload.hasCartOrigin) {
        const bool selected = isSelected({EditorObjectKind::PayloadCartOrigin, "cart_origin"});
        DebugVis::drawWireSphere(camera, config.payload.cartOrigin, 0.65f,
            selected ? glm::vec4(1, 0.9f, 0.2f, 1) : glm::vec4(1, 0.55f, 0.1f, 1));
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
                                         : glm::vec4(0.3f, 0.85f, 1, 1);
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
            const glm::vec3 end = center + axes[i] * kHandleLength;
            DebugVis::drawLine(camera, center, end, color);
            DebugVis::drawWireSphere(camera, end, 0.09f, color);
        }
    }

    if (mSnapTargetValid)
        DebugVis::drawPointCross(camera, mSnapTarget, 0.45f, {1, 1, 0.2f, 1});
}

void MapEditor::renderUI(GLFWwindow* window)
{
    (void)window;
    if (!mActive) return;
    const float w = uiScreenW();
    const float h = uiScreenH();
    const glm::vec4 panel(0.04f, 0.05f, 0.08f, 0.78f);
    const glm::vec4 text(0.9f, 0.95f, 1.0f, 1.0f);
    const glm::vec4 dim(0.65f, 0.72f, 0.82f, 1.0f);

    uiDrawRect({16.0f, 16.0f, 640.0f, 92.0f}, panel, "map-editor-panel");
    char line[512];
    std::snprintf(line, sizeof(line), "MAP EDITOR  %s%s  freeze=%s",
        mapId().c_str(), dirty() ? " *" : "",
        mFreezeWorld ? "on" : "off");
    uiDrawText(line, 26.0f, 24.0f, 0.34f, text);
    std::snprintf(line, sizeof(line), "selected %zu  blocks=%zu  routes=%zu  entities=%zu",
        mSelection.size(), MapConfigRegistry::instance().current().blocks.size(),
        MapConfigRegistry::instance().current().payload.route.size(),
        MapConfigRegistry::instance().current().entities.size());
    uiDrawText(line, 26.0f, 48.0f, 0.26f, dim);
    uiDrawText(mStatus.c_str(), 26.0f, 70.0f, 0.24f,
        glm::vec4(0.55f, 1.0f, 0.7f, 1.0f));

    std::vector<EditorObjectRef> objects;
    collectEditorObjects(objects);
    const float px = w - 360.0f;
    const float py = 120.0f;
    const float pw = 340.0f;
    const float ph = h - 180.0f;
    uiDrawRect({px, py, pw, ph}, panel, "map-editor-outliner");
    std::snprintf(line, sizeof(line), "OBJECTS %zu   PgUp/PgDn", objects.size());
    uiDrawText(line, px + 10.0f, py + 10.0f, 0.3f, text);
    constexpr float rowH = 22.0f;
    const int maxRows = std::max(1, (int)((ph - 76.0f) / rowH));
    mOutlinerOffset = std::clamp(mOutlinerOffset, 0,
        std::max(0, (int)objects.size() - maxRows));
    for (int row = 0; row < maxRows; ++row) {
        const int index = mOutlinerOffset + row;
        if (index < 0 || index >= (int)objects.size()) break;
        const EditorObjectRef& ref = objects[(size_t)index];
        const bool selected = isSelected(ref);
        glm::vec4 color = selected ? glm::vec4(1, 0.9f, 0.2f, 1) : text;
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
            MapGLBObject* object = mapObjectFor(ref);
            std::snprintf(rowText, sizeof(rowText), "%s  %s",
                ref.id.c_str(), object ? object->name.c_str() : "");
        } else {
            std::snprintf(rowText, sizeof(rowText), "%s  %s",
                kindName(ref.kind).c_str(), ref.id.c_str());
        }
        uiDrawText(rowText, px + 10.0f, py + 36.0f + row * rowH, 0.24f, color);
    }

    if (!mSelection.empty()) {
        const EditorObjectRef& ref = mSelection.front();
        const glm::vec3 p = objectPosition(ref);
        std::snprintf(line, sizeof(line), "SELECTED %s", describeRef(ref).c_str());
        uiDrawText(line, px + 10.0f, py + ph - 66.0f, 0.26f, text);
        std::snprintf(line, sizeof(line), "pos %.2f %.2f %.2f", p.x, p.y, p.z);
        uiDrawText(line, px + 10.0f, py + ph - 44.0f, 0.24f, dim);
        if (MapGLBObject* object = mapObjectFor(ref)) {
            const glm::vec3 size = object->boundsMax - object->boundsMin;
            std::snprintf(line, sizeof(line), "glb %s  size %.2f %.2f %.2f  verts %zu",
                object->name.c_str(), size.x, size.y, size.z, object->vertexCount);
            uiDrawText(line, px + 10.0f, py + ph - 24.0f, 0.21f, dim);
        }
    }

    const char* help =
        "RMB look  LMB select/drag  Shift+click multi  Ctrl+C/V/X  Ctrl+D duplicate  "
        "Del delete  Ctrl+Z undo  Ctrl+Shift+Z redo  Ctrl+S save  V vertex snap  G grid snap  F frame  Esc back";
    uiDrawRect({16.0f, h - 38.0f, w - 32.0f, 24.0f}, panel, "map-editor-help-bg");
    uiDrawText(help, 26.0f, h - 34.0f, 0.20f, dim);
}
