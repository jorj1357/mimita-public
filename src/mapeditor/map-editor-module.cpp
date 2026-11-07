// 2026-11-07
/* purpose
* live map-editor module generation
* owns editor interaction decisions while the host owns the map document, undo
* history, collision refresh, and persistence
* this file DOES NOT own map data, GLFW, camera, or renderer state
*/
#include "modules/api/map-editor-api.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <module-identity.h>

namespace {

using mimita_map_editor::EditorServicesV1;
using mimita_module::HostServicesV1;
using mimita_module::ModuleEventV1;

const HostServicesV1* gHost = nullptr;
const EditorServicesV1* gEditor = nullptr;

constexpr std::uint32_t kMaxSelection = 256;
mimita_map_editor::EditorSelectionRefV1 gSelection[kMaxSelection];
std::uint32_t gSelectionCount = 0;

float distanceSquared(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

void logPosition(const char* id, const float before[3], const float after[3])
{
    if (!gHost || !gHost->log)
        return;
    char fields[384];
    std::snprintf(fields, sizeof(fields),
                  "{\"object\":\"%s\",\"field\":\"position\","
                  "\"before\":[%.4f,%.4f,%.4f],"
                  "\"after\":[%.4f,%.4f,%.4f],"
                  "\"source\":\"property_panel\",\"handled_by_module\":true,"
                  "\"module_generation\":%u}",
                  id,
                  before[0], before[1], before[2],
                  after[0], after[1], after[2],
                  static_cast<unsigned>(MIMITA_MODULE_GENERATION));
    gHost->log(
        mimita_module::LOG_CATEGORY_WORLD,
        mimita_module::LOG_LEVEL_IMPORTANT,
        "map-editor.module.position",
        id,
        "position edit handled by map-editor module",
        0,
        fields,
        __FILE__,
        __LINE__,
        __FUNCTION__);
}

int initialize(const HostServicesV1* services)
{
    if (!services || services->version != mimita_module::ABI_VERSION ||
        services->structSize != sizeof(HostServicesV1) ||
        !services->getCapability)
        return 0;
    gHost = services;
    gEditor = static_cast<const EditorServicesV1*>(
        services->getCapability(mimita_module::CAPABILITY_EDITOR,
                                mimita_map_editor::ABI_VERSION));
    if (!gEditor || gEditor->version != mimita_map_editor::ABI_VERSION ||
        gEditor->structSize != sizeof(EditorServicesV1) ||
        !gEditor->getPrimarySelection || !gEditor->applyPrimaryPosition)
        return 0;
    return 1;
}

void shutdown()
{
    gHost = nullptr;
    gEditor = nullptr;
}

int handleSetPosition(mimita_map_editor::EditorSetPositionV1* payload)
{
    if (!gEditor || !payload)
        return 0;
    char id[mimita_module::MAX_OBJECT_ID] = {};
    int kind = 0;
    float current[3] = {};
    if (!gEditor->getPrimarySelection(
            id, static_cast<std::uint32_t>(sizeof(id)), &kind, current))
        return 0;
    if (distanceSquared(current, payload->position) <= 1e-8f) {
        payload->result = 1;
        return 1;
    }
    if (!gEditor->applyPrimaryPosition(payload->position))
        return 0;
    logPosition(id, current, payload->position);
    payload->result = 1;
    return 1;
}

float pointSegmentDistance2D(float px, float py, float ax, float ay,
                             float bx, float by)
{
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float lengthSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (lengthSq > 0.000001f)
        t = (apx * abx + apy * aby) / lengthSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

int handleDragDelta(mimita_map_editor::EditorDragDeltaV1* payload)
{
    if (!payload)
        return 0;
    const mimita_map_editor::DragMathV1* input = &payload->input;
    payload->outDelta[0] = payload->outDelta[1] = payload->outDelta[2] = 0.0f;
    if (input->mode == 0) {
        const float dx = input->mouseDelta[0] * input->unitsPerPixel;
        const float dy = -input->mouseDelta[1] * input->unitsPerPixel;
        for (int i = 0; i < 3; ++i)
            payload->outDelta[i] = input->cameraRight[i] * dx +
                                   input->cameraUp[i] * dy;
        payload->result = 1;
        return 1;
    }
    if (input->mode < 1 || input->mode > 6)
        return 0;
    const int axis = (input->mode - 1) % 3;
    const float axisScreenLength = std::sqrt(
        input->axisScreen[0] * input->axisScreen[0] +
        input->axisScreen[1] * input->axisScreen[1]);
    if (axisScreenLength <= 1.0f)
        return 0;
    const float alongPixels =
        (input->mouseDelta[0] * input->axisScreen[0] +
         input->mouseDelta[1] * input->axisScreen[1]) / axisScreenLength;
    payload->outDelta[axis] = alongPixels * input->handleLength / axisScreenLength;
    payload->result = 1;
    return 1;
}

int handleGizmoPick(mimita_map_editor::EditorGizmoPickV1* payload)
{
    if (!payload)
        return 0;
    const mimita_map_editor::GizmoPickV1* input = &payload->input;
    payload->outAxis = -1;
    float bestDistance = input->pickPixels;
    int bestAxis = -1;
    for (int i = 0; i < 3; ++i) {
        const float distance = pointSegmentDistance2D(
            input->cursor[0], input->cursor[1],
            input->center[0], input->center[1],
            input->axisEnd[i][0], input->axisEnd[i][1]);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestAxis = i;
        }
        if (input->allowScale) {
            const float dx = input->cursor[0] - input->scaleEnd[i][0];
            const float dy = input->cursor[1] - input->scaleEnd[i][1];
            const float scaleDistance = std::sqrt(dx * dx + dy * dy);
            if (scaleDistance < bestDistance) {
                bestDistance = scaleDistance;
                bestAxis = i + 3;
            }
        }
    }
    payload->outAxis = bestAxis;
    payload->result = bestAxis >= 0 ? 1 : 0;
    return payload->result;
}

void trimArgument(const char* input, char* out, std::uint32_t capacity)
{
    if (!out || capacity == 0)
        return;
    out[0] = '\0';
    if (!input)
        return;
    while (*input == ' ' || *input == '\t')
        ++input;
    std::uint32_t length = 0;
    while (input[length] && input[length] != ' ' && input[length] != '\t' &&
           length + 1 < capacity)
        ++length;
    if (length > 0) {
        std::memcpy(out, input, length);
        out[length] = '\0';
    }
}

int handleCommand(mimita_map_editor::EditorCommandV1* payload)
{
    if (!gEditor || !payload || !payload->command)
        return 0;
    const EditorServicesV1* services = gEditor;
    const char* command = payload->command;
    const char* arguments = payload->arguments;
    char arg[128] = {};
    trimArgument(arguments, arg, sizeof(arg));

    if (std::strcmp(command, "select") == 0) {
        if (arg[0] == '\0') {
            if (services->logLine)
                services->logLine("[MAPEDIT] select <id>");
            payload->result = 1;
            return 1;
        }
        payload->result = services->selectObject && services->selectObject(arg);
        return 1;
    }
    if (std::strcmp(command, "select_kind") == 0) {
        if (arg[0] == '\0') {
            if (services->logLine)
                services->logLine("[MAPEDIT] select_kind <kind>");
            payload->result = 1;
            return 1;
        }
        payload->result = services->selectKind && services->selectKind(arg);
        return 1;
    }
    if (std::strcmp(command, "select_route") == 0) {
        payload->result = services->selectKind && services->selectKind("route");
        return 1;
    }
    if (std::strcmp(command, "selectall") == 0 ||
        std::strcmp(command, "select_all") == 0) {
        payload->result = services->selectAll && services->selectAll();
        return 1;
    }
    if (std::strcmp(command, "clear_selection") == 0 ||
        std::strcmp(command, "select_none") == 0) {
        payload->result = services->clearSelection && services->clearSelection();
        return 1;
    }
    if (std::strcmp(command, "copy") == 0) {
        payload->result = services->copySelection &&
            (services->copySelection(), 1);
        return 1;
    }
    if (std::strcmp(command, "paste") == 0) {
        payload->result = services->pasteClipboard &&
            (services->pasteClipboard(), 1);
        return 1;
    }
    if (std::strcmp(command, "cut") == 0) {
        payload->result = services->cutSelection &&
            (services->cutSelection(), 1);
        return 1;
    }
    if (std::strcmp(command, "duplicate") == 0) {
        payload->result = services->duplicateSelection &&
            (services->duplicateSelection(), 1);
        return 1;
    }
    if (std::strcmp(command, "delete") == 0) {
        payload->result = services->deleteSelection &&
            (services->deleteSelection(), 1);
        return 1;
    }
    if (std::strcmp(command, "undo") == 0) {
        payload->result = services->undo && (services->undo(), 1);
        return 1;
    }
    if (std::strcmp(command, "redo") == 0) {
        payload->result = services->redo && (services->redo(), 1);
        return 1;
    }
    if (std::strcmp(command, "save") == 0) {
        payload->result = services->save && (services->save(), 1);
        return 1;
    }
    if (std::strcmp(command, "reload") == 0) {
        payload->result = services->reload && (services->reload(), 1);
        return 1;
    }
    if (std::strcmp(command, "autosave") == 0) {
        payload->result = services->autosave && (services->autosave(), 1);
        return 1;
    }
    if (std::strcmp(command, "rebuild_collision") == 0) {
        payload->result = services->rebuildCollision &&
            (services->rebuildCollision(), 1);
        return 1;
    }
    if (std::strcmp(command, "focus") == 0) {
        payload->result = services->focusSelection &&
            (services->focusSelection(), 1);
        return 1;
    }
    if (std::strcmp(command, "freeze") == 0) {
        if (arg[0] == '\0') {
            if (services->logLine)
                services->logLine("[MAPEDIT] freeze <0|1>");
            payload->result = 1;
            return 1;
        }
        if (services->setFreezeWorld)
            services->setFreezeWorld(std::atoi(arg) != 0 ? 1 : 0);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "rename") == 0) {
        if (arg[0] == '\0') {
            if (services->logLine)
                services->logLine("[MAPEDIT] rename <name>");
            payload->result = 1;
            return 1;
        }
        if (services->renameSelection)
            services->renameSelection(arg);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "set_radius") == 0 ||
        std::strcmp(command, "site_radius") == 0) {
        float radius = 0.0f;
        if (std::sscanf(arguments, "%f", &radius) == 1 &&
            services->setBombSiteRadius)
            services->setBombSiteRadius(radius);
        else if (services->logLine)
            services->logLine("[MAPEDIT] set_radius <m>");
        payload->result = 1;
        return 1;
    }
    float x = 0.0f, y = 0.0f, z = 0.0f;
    const bool hasThree = std::sscanf(arguments, "%f %f %f", &x, &y, &z) == 3;
    if (std::strcmp(command, "nudge") == 0) {
        if (!hasThree) {
            if (services->logLine)
                services->logLine("[MAPEDIT] nudge <dx> <dy> <dz>");
            payload->result = 1;
            return 1;
        }
        if (services->nudge)
            services->nudge(x, y, z);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "set_position") == 0 ||
        std::strcmp(command, "set_pos") == 0) {
        if (!hasThree) {
            if (services->logLine)
                services->logLine("[MAPEDIT] set_position <x> <y> <z>");
            payload->result = 1;
            return 1;
        }
        if (services->setPosition)
            services->setPosition(x, y, z);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "set_rotation") == 0 ||
        std::strcmp(command, "set_rot") == 0) {
        if (!hasThree) {
            if (services->logLine)
                services->logLine("[MAPEDIT] set_rotation <x> <y> <z>");
            payload->result = 1;
            return 1;
        }
        if (services->setRotation)
            services->setRotation(x, y, z);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "set_scale") == 0) {
        if (!hasThree) {
            if (services->logLine)
                services->logLine("[MAPEDIT] set_scale <x> <y> <z>");
            payload->result = 1;
            return 1;
        }
        if (services->setScale)
            services->setScale(x, y, z);
        payload->result = 1;
        return 1;
    }
    if (std::strcmp(command, "set_size") == 0 ||
        std::strcmp(command, "resize") == 0) {
        if (!hasThree) {
            if (services->logLine)
                services->logLine("[MAPEDIT] set_size <x> <y> <z>");
            payload->result = 1;
            return 1;
        }
        if (services->setSize)
            services->setSize(x, y, z);
        payload->result = 1;
        return 1;
    }
    return 0;
}

int handleAddChunk(mimita_map_editor::EditorAddChunkV1* payload)
{
    if (!gEditor || !payload)
        return 0;
    float footprint[3] = {10.0f, 10.0f, 4.0f};
    if (gEditor->getProfileFootprint)
        gEditor->getProfileFootprint(footprint);
    payload->result = gEditor->createChunk &&
        gEditor->createChunk(payload->id, footprint, payload->outId,
                             sizeof(payload->outId));
    return payload->result;
}

int handleAddChunkPart(mimita_map_editor::EditorAddChunkPartV1* payload)
{
    if (!gEditor || !payload)
        return 0;
    payload->result = gEditor->createChunkPart &&
        gEditor->createChunkPart(nullptr, payload->kind, payload->outId,
                                 sizeof(payload->outId));
    return payload->result;
}

int handleAddConnector(mimita_map_editor::EditorAddConnectorV1* payload)
{
    if (!gEditor || !payload)
        return 0;
    char id[64] = {};
    payload->result = gEditor->createConnector &&
        gEditor->createConnector(nullptr, payload->side, id, sizeof(id));
    return payload->result;
}

int handlePreviewConnections(mimita_map_editor::EditorPreviewV1* payload)
{
    if (!gEditor || !payload)
        return 0;
    payload->result = gEditor->previewConnections &&
        gEditor->previewConnections(payload->report, sizeof(payload->report));
    return payload->result;
}

int handleInput(mimita_map_editor::EditorInputV1* payload)
{
    if (!payload)
        return 0;
    const int windowW = payload->windowSize[0];
    const int windowH = payload->windowSize[1];
    if (windowW <= 0 || windowH <= 0)
        return 0;
    payload->mappedCursor[0] =
        payload->rawCursor[0] * (float)payload->sceneSize[0] / (float)windowW;
    payload->mappedCursor[1] =
        payload->rawCursor[1] * (float)payload->sceneSize[1] / (float)windowH;
    payload->result = 1;
    return 1;
}

int handleClick(mimita_map_editor::EditorClickV1* payload)
{
    if (!payload)
        return 0;
    int action = 5;
    if (payload->handleHit && payload->selectionCount > 0)
        action = 3;
    else if (payload->objectHit &&
             (payload->differentObject || payload->shift))
        action = payload->shift ? 2 : 1;
    else if (payload->objectHit)
        action = 4;
    else
        action = 5;
    payload->action = action;
    payload->result = 1;
    return 1;
}

int onEvent(ModuleEventV1* event)
{
    if (!event || !event->payload)
        return 0;
    switch (event->typeId) {
    case mimita_module::EVENT_EDITOR_SET_POSITION:
        return handleSetPosition(
            static_cast<mimita_map_editor::EditorSetPositionV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_DRAG_DELTA:
        return handleDragDelta(
            static_cast<mimita_map_editor::EditorDragDeltaV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_GIZMO_PICK:
        return handleGizmoPick(
            static_cast<mimita_map_editor::EditorGizmoPickV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_COMMAND:
        return handleCommand(
            static_cast<mimita_map_editor::EditorCommandV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_ADD_CHUNK:
        return handleAddChunk(
            static_cast<mimita_map_editor::EditorAddChunkV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_ADD_CHUNK_PART:
        return handleAddChunkPart(
            static_cast<mimita_map_editor::EditorAddChunkPartV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_ADD_CONNECTOR:
        return handleAddConnector(
            static_cast<mimita_map_editor::EditorAddConnectorV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_PREVIEW_CONNECTIONS:
        return handlePreviewConnections(
            static_cast<mimita_map_editor::EditorPreviewV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_INPUT:
        return handleInput(
            static_cast<mimita_map_editor::EditorInputV1*>(event->payload));
    case mimita_module::EVENT_EDITOR_CLICK:
        return handleClick(
            static_cast<mimita_map_editor::EditorClickV1*>(event->payload));
    default:
        return 0;
    }
}

struct SelectionStateHeader
{
    std::uint32_t version;
    std::uint32_t count;
};

std::uint32_t exportState(std::uint8_t* buffer, std::uint32_t capacity)
{
    std::uint32_t count = 0;
    if (gEditor && gEditor->getSelectionCount)
        count = gEditor->getSelectionCount();
    if (count > kMaxSelection)
        count = kMaxSelection;
    if (gEditor && gEditor->getSelectionItem) {
        for (std::uint32_t i = 0; i < count; ++i) {
            gSelection[i] = {};
            gEditor->getSelectionItem(
                i, &gSelection[i].kind, gSelection[i].id,
                sizeof(gSelection[i].id));
        }
    }
    gSelectionCount = count;
    const std::uint32_t size =
        sizeof(SelectionStateHeader) +
        count * sizeof(mimita_map_editor::EditorSelectionRefV1);
    if (gHost && gHost->log) {
        char fields[160];
        std::snprintf(fields, sizeof(fields),
                      "{\"count\":%u,\"bytes\":%u,\"generation\":%u}",
                      count, size,
                      static_cast<unsigned>(MIMITA_MODULE_GENERATION));
        gHost->log(mimita_module::LOG_CATEGORY_WORLD,
                   mimita_module::LOG_LEVEL_IMPORTANT,
                   "map-editor.state.export", "map-editor",
                   "selection exported into module state blob", 0,
                   fields, __FILE__, __LINE__, __FUNCTION__);
    }
    if (!buffer)
        return size;
    if (capacity < size)
        return 0;
    SelectionStateHeader header{1, count};
    std::memcpy(buffer, &header, sizeof(header));
    if (count > 0)
        std::memcpy(buffer + sizeof(header), gSelection,
                    count * sizeof(mimita_map_editor::EditorSelectionRefV1));
    return size;
}

int importState(const std::uint8_t* buffer, std::uint32_t size)
{
    if (size == 0) {
        gSelectionCount = 0;
        return 1;
    }
    if (!buffer || size < sizeof(SelectionStateHeader))
        return 0;
    SelectionStateHeader header{};
    std::memcpy(&header, buffer, sizeof(header));
    if (header.version != 1 || header.count > kMaxSelection)
        return 0;
    const std::uint32_t expected =
        sizeof(SelectionStateHeader) +
        header.count * sizeof(mimita_map_editor::EditorSelectionRefV1);
    if (size < expected)
        return 0;
    if (header.count > 0)
        std::memcpy(gSelection, buffer + sizeof(header),
                    header.count * sizeof(mimita_map_editor::EditorSelectionRefV1));
    gSelectionCount = header.count;
    if (gEditor && gEditor->setSelection)
        gEditor->setSelection(header.count, gSelection);
    if (gHost && gHost->log) {
        char fields[160];
        std::snprintf(fields, sizeof(fields),
                      "{\"count\":%u,\"bytes\":%u,\"generation\":%u}",
                      header.count, size,
                      static_cast<unsigned>(MIMITA_MODULE_GENERATION));
        gHost->log(mimita_module::LOG_CATEGORY_WORLD,
                   mimita_module::LOG_LEVEL_IMPORTANT,
                   "map-editor.state.import", "map-editor",
                   "selection restored from module state blob", 0,
                   fields, __FILE__, __LINE__, __FUNCTION__);
    }
    return 1;
}

const mimita_module::ModuleV1 gModule = {
    {mimita_module::ABI_VERSION,
     sizeof(mimita_module::ModuleV1),
     "map-editor",
     MIMITA_MODULE_GENERATION,
     MIMITA_MODULE_SOURCE_HASH,
     MIMITA_MODULE_STATE_SCHEMA},
    initialize,
    shutdown,
    onEvent,
    exportState,
    importState,
};

} // namespace

MIMITA_MODULE_EXPORT const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaModuleAPI(std::uint32_t abiVersion)
{
    if (abiVersion != mimita_module::ABI_VERSION)
        return nullptr;
    return &gModule.base;
}
// selection state blob proof probe
