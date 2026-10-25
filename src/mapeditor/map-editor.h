// 2026-10-23
/* purpose
* own the in-game map editor mode, selection, clipboard, and undo/redo history
* expose editor input, world overlay rendering, HUD rendering, and terminal commands
* this file DOES NOT own authored map data, world geometry, or gameplay simulation
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gamemode/map-config.h"

struct GLFWwindow;
class Camera;
struct MapGLBObject;

enum class EditorObjectKind {
    Entity,
    Block,
    PayloadRoutePoint,
    PayloadCartOrigin,
    MapGLBObject
};

struct EditorObjectRef {
    EditorObjectKind kind = EditorObjectKind::Entity;
    std::string id;

    bool operator==(const EditorObjectRef& other) const {
        return kind == other.kind && id == other.id;
    }
};

class MapEditor {
public:
    static MapEditor& instance();

    bool active() const { return mActive; }
    bool freezeWorld() const { return mFreezeWorld; }
    bool wantsCursorCaptured() const { return mLooking; }
    bool setActive(bool enabled);
    void toggle();
    void setFreezeWorld(bool freeze);

    void updateInput(GLFWwindow* window, float dt);
    void renderWorld(Camera& camera);
    void renderUI(GLFWwindow* window);
    bool handleKey(int key, int mods, int action);
    void handleEscape();

    void notify(const std::string& message);
    const std::string& statusMessage() const { return mStatus; }
    const std::string& lastAction() const { return mLastAction; }

    bool save();
    bool reload();
    void rebuildGeometry();
    void undoEdit() { undo(); }
    void redoEdit() { redo(); }
    void copyEdit() { copySelection(); }
    void pasteEdit() { pasteClipboard(); }
    void cutEdit() { cutSelection(); }
    void duplicateEdit() { duplicateSelection(); }
    void deleteEdit() { deleteSelection(); }
    void selectAllEdit();

    std::string mapId() const;
    size_t selectionCount() const { return mSelection.size(); }
    bool dirty() const;

    // Commands that intentionally mutate authored map data through the editor,
    // so they share selection, history, and geometry rebuild behavior.
    std::string addEntity(const std::string& type, const std::string& id = {});
    std::string addBlock(const std::string& id = {});
    std::string addRoutePoint(const std::string& id = {});
    void addCartOrigin();

private:
    MapEditor() = default;

    struct ClipboardItem {
        EditorObjectKind kind = EditorObjectKind::Entity;
        std::string id;
        MapEntity entity;
        AuthoredBlock block;
        PayloadRoutePoint route;
        bool hasEntity = false;
        bool hasBlock = false;
        bool hasRoute = false;
        glm::vec3 cartOrigin{0.0f};
        float cartYaw = 0.0f;
    };

    enum class DragMode {
        None,
        ObjectPlane,
        AxisX,
        AxisY,
        AxisZ
    };

    struct DragState {
        DragMode mode = DragMode::None;
        EditorObjectRef primary;
        glm::vec3 startPoint{0.0f};
        glm::vec3 startCenter{0.0f};
        float startDistance = 0.0f;
        std::vector<EditorObjectRef> refs;
        std::vector<glm::vec3> originalPositions;
        std::vector<glm::vec3> originalRotations;
        MapObjectiveConfig pendingUndo;
        bool hasPendingUndo = false;
        bool moved = false;
    };

    bool mActive = false;
    bool mFreezeWorld = true;
    bool mPrevFreecam = false;
    bool mPrevMouseLock = true;
    bool mLooking = false;
    bool mVertexSnap = true;
    bool mGridSnap = false;
    float mSnapGrid = 1.0f;
    double mMouseX = 0.0;
    double mMouseY = 0.0;
    bool mLeftPrev = false;
    bool mRightPrev = false;
    bool mHoverValid = false;
    EditorObjectRef mHover;
    int mHoverAxis = -1;
    std::string mStatus;
    std::string mLastAction;
    double mLastMessageTime = 0.0;

    std::vector<EditorObjectRef> mSelection;
    std::vector<ClipboardItem> mClipboard;
    std::vector<MapObjectiveConfig> mUndo;
    std::vector<MapObjectiveConfig> mRedo;
    std::vector<std::string> mUndoLabels;
    std::vector<std::string> mRedoLabels;
    std::vector<glm::vec3> mFramePositions;
    DragState mDrag;
    glm::vec3 mSnapTarget{0.0f};
    bool mSnapTargetValid = false;
    int mOutlinerOffset = 0;
    EditorObjectRef mOutlinerAnchor;

    void collectEditorObjects(std::vector<EditorObjectRef>& out) const;
    int outlinerRowAt(double screenX, double screenY) const;

    void clearSelection();
    void selectOnly(const EditorObjectRef& ref);
    void toggleSelection(const EditorObjectRef& ref);
    bool isSelected(const EditorObjectRef& ref) const;
    const MapEntity* entityFor(const EditorObjectRef& ref) const;
    const AuthoredBlock* blockFor(const EditorObjectRef& ref) const;
    const PayloadRoutePoint* routeFor(const EditorObjectRef& ref) const;
    MapGLBObject* mapObjectFor(const EditorObjectRef& ref) const;

    glm::vec3 objectPosition(const EditorObjectRef& ref) const;
    void setObjectPosition(const EditorObjectRef& ref, const glm::vec3& position);
    glm::vec3 selectionCenter() const;
    bool selectionHasBlocksOrRoutes() const;

    bool pickAtCursor(EditorObjectRef& out, float& outDistance) const;
    bool handleAtCursor(EditorObjectRef& out, int& axis) const;
    bool rayFromCursor(glm::vec3& origin, glm::vec3& direction) const;
    bool rayAABB(glm::vec3 origin, glm::vec3 direction, const glm::vec3& mn,
                 const glm::vec3& mx, float& distance) const;
    bool raySphere(glm::vec3 origin, glm::vec3 direction, const glm::vec3& center,
                   float radius, float& distance) const;
    bool rayOBB(glm::vec3 origin, glm::vec3 direction, const glm::vec3& center,
                const glm::vec3& size, const glm::vec3& rotationEuler,
                float& distance) const;
    glm::vec3 applySnap(const glm::vec3& position);
    glm::vec3 nearestWorldVertex(const glm::vec3& nearPoint, float maxDistance) const;

    void beginDrag(const EditorObjectRef& primary, DragMode mode,
                   const glm::vec3& startPoint);
    void updateDrag();
    void endDrag(bool commit);
    void pushUndo(const std::string& label);
    void undo();
    void redo();

    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void duplicateSelection();
    void deleteSelection();

    std::string describeRef(const EditorObjectRef& ref) const;
    std::string describeSelection() const;
    static std::string kindName(EditorObjectKind kind);
};
