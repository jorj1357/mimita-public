// 2026-11-07
/* purpose
* define the versioned plain-data boundary between the map editor module and
* the host-owned editor document/input/render services
* this file DOES NOT declare MapEditor, MapConfigRegistry, GLFW, or STL types
*/
#pragma once

#include <cstdint>

#include "modules/api/module-abi.h"

namespace mimita_map_editor {

constexpr std::uint32_t ABI_VERSION = 2;

// Inputs use the same drag modes as the host editor:
// 0 plane, 1 X, 2 Y, 3 Z, 4 scale X, 5 scale Y, 6 scale Z.
struct DragMathV1
{
    std::int32_t mode;
    float mouseDelta[2];
    float cameraRight[3];
    float cameraUp[3];
    float unitsPerPixel;
    float axisScreen[2];
    float handleLength;
};

struct GizmoPickV1
{
    float cursor[2];
    float center[2];
    float axisEnd[3][2];
    float scaleEnd[3][2];
    std::int32_t allowScale;
    float pickPixels;
};

struct EditorSetPositionV1
{
    float position[3];
    int result;
};

struct EditorDragDeltaV1
{
    DragMathV1 input;
    float outDelta[3];
    int result;
};

struct EditorGizmoPickV1
{
    GizmoPickV1 input;
    int outAxis;
    int result;
};

struct EditorCommandV1
{
    const char* command;
    const char* arguments;
    int result;
};

struct EditorAddChunkV1
{
    char id[mimita_module::MAX_MODULE_NAME];
    char outId[mimita_module::MAX_MODULE_NAME];
    int result;
};

struct EditorAddChunkPartV1
{
    char kind[mimita_module::MAX_MODULE_NAME];
    char outId[mimita_module::MAX_MODULE_NAME];
    int result;
};

struct EditorAddConnectorV1
{
    char side[mimita_module::MAX_MODULE_NAME];
    int result;
};

struct EditorPreviewV1
{
    char report[2048];
    int result;
};

struct EditorSelectionRefV1
{
    int kind;
    char id[mimita_module::MAX_OBJECT_ID];
};

// Bounded world-object view for editor diagnostics. The host owns the world;
// the module formats the outliner/dump decision without receiving pointers.
struct WorldObjectInfoV1
{
    char id[mimita_module::MAX_OBJECT_ID];
    char name[mimita_module::MAX_TAG_TEXT];
    std::int32_t nodeIndex;
    std::uint32_t vertexCount;
    float basePosition[3];
    float boundsMin[3];
    float boundsMax[3];
};

struct EditorInputV1
{
    float rawCursor[2];
    int windowSize[2];
    int framebufferSize[2];
    int sceneSize[2];
    int uiSize[2];
    int stretchedWholeFrame;
    float mappedCursor[2];
    int result;
};

struct EditorClickV1
{
    float cursor[2];
    int objectHit;
    int objectKind;
    char objectId[mimita_module::MAX_OBJECT_ID];
    float objectDistance;
    int differentObject;
    int handleHit;
    int handleAxis;
    int selectionCount;
    int shift;
    int ctrl;
    int action; // 0 none, 1 select, 2 toggle, 3 handle drag, 4 click candidate, 5 box
    int result;
};

// Host services table returned for CAPABILITY_EDITOR. All document mutation,
// undo history, selection, collision refresh, and persistence stay in the host.
struct EditorServicesV1
{
    std::uint32_t version;
    std::uint32_t structSize;

    int (MIMITA_MODULE_CALL *getPrimarySelection)(
        char* id, std::uint32_t idCapacity, int* kind, float* position);
    int (MIMITA_MODULE_CALL *applyPrimaryPosition)(const float* position);
    void (MIMITA_MODULE_CALL *notify)(const char* message);
    std::uint64_t (MIMITA_MODULE_CALL *revision)();

    int (MIMITA_MODULE_CALL *selectObject)(const char* id);
    int (MIMITA_MODULE_CALL *selectKind)(const char* kind);
    int (MIMITA_MODULE_CALL *selectAll)();
    int (MIMITA_MODULE_CALL *clearSelection)();
    void (MIMITA_MODULE_CALL *logLine)(const char* message);
    int (MIMITA_MODULE_CALL *copySelection)();
    int (MIMITA_MODULE_CALL *pasteClipboard)();
    int (MIMITA_MODULE_CALL *cutSelection)();
    int (MIMITA_MODULE_CALL *duplicateSelection)();
    int (MIMITA_MODULE_CALL *deleteSelection)();
    int (MIMITA_MODULE_CALL *undo)();
    int (MIMITA_MODULE_CALL *redo)();
    int (MIMITA_MODULE_CALL *save)();
    int (MIMITA_MODULE_CALL *reload)();
    int (MIMITA_MODULE_CALL *autosave)();
    int (MIMITA_MODULE_CALL *rebuildCollision)();
    int (MIMITA_MODULE_CALL *setFreezeWorld)(int freeze);
    int (MIMITA_MODULE_CALL *focusSelection)();
    int (MIMITA_MODULE_CALL *nudge)(float dx, float dy, float dz);
    int (MIMITA_MODULE_CALL *setPosition)(float x, float y, float z);
    int (MIMITA_MODULE_CALL *setRotation)(float x, float y, float z);
    int (MIMITA_MODULE_CALL *setScale)(float x, float y, float z);
    int (MIMITA_MODULE_CALL *setSize)(float x, float y, float z);
    int (MIMITA_MODULE_CALL *setBombSiteRadius)(float radius);
    int (MIMITA_MODULE_CALL *renameSelection)(const char* name);

    // Selection snapshot used by the module-owned state blob. The host remains
    // the document/undo owner during this migration step.
    std::uint32_t (MIMITA_MODULE_CALL *getSelectionCount)();
    int (MIMITA_MODULE_CALL *getSelectionItem)(
        std::uint32_t index, int* kind, char* id, std::uint32_t idCapacity);
    int (MIMITA_MODULE_CALL *setSelection)(
        std::uint32_t count, const EditorSelectionRefV1* refs);

    // Chunk authoring. The host applies profile defaults and document changes.
    int (MIMITA_MODULE_CALL *getProfileFootprint)(float outFootprint[3]);
    int (MIMITA_MODULE_CALL *createChunk)(
        const char* requestedId, const float footprint[3], char* outId,
        std::uint32_t outCapacity);
    int (MIMITA_MODULE_CALL *createChunkPart)(
        const char* chunkId, const char* kind, char* outId,
        std::uint32_t outCapacity);
    int (MIMITA_MODULE_CALL *createConnector)(
        const char* chunkId, const char* side, char* outId,
        std::uint32_t outCapacity);
    int (MIMITA_MODULE_CALL *previewConnections)(
        char* report, std::uint32_t reportCapacity);
    // Solve a parent/child connector placement, including quarter-turn yaw.
    int (MIMITA_MODULE_CALL *evaluateConnectorPair)(
        const char* parentChunk, const char* parentConnector,
        const char* childChunk, const char* childConnector,
        float outTransform[16], char* reason, std::uint32_t reasonCapacity);

    // Bounded outliner/dump enumeration. Returns 0 when the world is not loaded.
    std::uint32_t (MIMITA_MODULE_CALL *getWorldObjectCount)();
    int (MIMITA_MODULE_CALL *getWorldObjectInfo)(
        std::uint32_t index, WorldObjectInfoV1* out);
    int (MIMITA_MODULE_CALL *getMapBounds)(
        float outMin[3], float outMax[3]);
};

} // namespace mimita_map_editor

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion);
