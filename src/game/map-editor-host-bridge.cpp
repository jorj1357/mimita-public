// Game-side implementations for the resident module host's editor callbacks.
//
// The resident module host owns module loading and the EditorServicesV1 table.
// The actual MapEditor document lives in the game generation, so the host
// forwards each bounded callback here. No map data or game state crosses back.

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "mapeditor/map-editor.h"
#include "modules/api/map-editor-api.h"

namespace mimita::game {
namespace {

using mimita::host::MapEditorHostCallV1;
using mimita::host::MapEditorHostOp;

int getPrimarySelection(MapEditorHostCallV1& call)
{
    auto* id = static_cast<char*>(call.ptr0);
    auto* kind = static_cast<int*>(call.ptr1);
    auto* position = static_cast<float*>(call.ptr2);
    if (!id || !kind || !position)
        return 0;
    glm::vec3 value(0.0f);
    if (!MapEditor::instance().modulePrimarySelection(id, call.u0, kind, value))
        return 0;
    position[0] = value.x;
    position[1] = value.y;
    position[2] = value.z;
    return 1;
}

int setFreezeWorld(const MapEditorHostCallV1& call)
{
    MapEditor::instance().setFreezeWorld(call.u0 != 0);
    return 1;
}

int getSelectionItem(MapEditorHostCallV1& call)
{
    auto* kind = static_cast<int*>(call.ptr0);
    auto* id = static_cast<char*>(call.ptr1);
    if (!kind || !id)
        return 0;
    return MapEditor::instance().moduleSelectionAt(
        call.u0, kind, id, call.u1) ? 1 : 0;
}

int setSelection(MapEditorHostCallV1& call)
{
    const auto* refs =
        static_cast<const mimita_map_editor::EditorSelectionRefV1*>(call.ptr0);
    if (!refs && call.u0 != 0)
        return 0;
    std::vector<EditorObjectRef> selection;
    selection.reserve(call.u0);
    for (std::uint32_t i = 0; i < call.u0; ++i) {
        EditorObjectRef ref;
        ref.kind = static_cast<EditorObjectKind>(refs[i].kind);
        ref.id = refs[i].id;
        selection.push_back(std::move(ref));
    }
    MapEditor::instance().moduleSetSelection(selection);
    return 1;
}

int getWorldObjectInfo(MapEditorHostCallV1& call)
{
    auto* out = static_cast<mimita_map_editor::WorldObjectInfoV1*>(call.ptr0);
    if (!out)
        return 0;
    MapEditorWorldObjectView view;
    if (!MapEditor::instance().moduleWorldObjectAt(call.u0, view))
        return 0;
    std::snprintf(out->id, sizeof(out->id), "%s", view.id);
    std::snprintf(out->name, sizeof(out->name), "%s", view.name);
    out->nodeIndex = view.nodeIndex;
    out->vertexCount = view.vertexCount;
    for (int i = 0; i < 3; ++i) {
        out->basePosition[i] = view.basePosition[i];
        out->boundsMin[i] = view.boundsMin[i];
        out->boundsMax[i] = view.boundsMax[i];
    }
    return 1;
}

int getMapBounds(MapEditorHostCallV1& call)
{
    auto* outMin = static_cast<float*>(call.ptr0);
    auto* outMax = static_cast<float*>(call.ptr1);
    if (!outMin || !outMax)
        return 0;
    glm::vec3 minValue(0.0f);
    glm::vec3 maxValue(0.0f);
    if (!MapEditor::instance().moduleMapBounds(minValue, maxValue))
        return 0;
    outMin[0] = minValue.x;
    outMin[1] = minValue.y;
    outMin[2] = minValue.z;
    outMax[0] = maxValue.x;
    outMax[1] = maxValue.y;
    outMax[2] = maxValue.z;
    return 1;
}

int createChunk(MapEditorHostCallV1& call)
{
    auto* outId = static_cast<char*>(call.ptr1);
    if (!outId || call.u0 == 0)
        return 0;
    const auto* footprint = static_cast<const float*>(call.ptr0);
    const glm::vec3 size = footprint
        ? glm::vec3(footprint[0], footprint[1], footprint[2])
        : glm::vec3(10.0f, 10.0f, 4.0f);
    const std::string created = MapEditor::instance().addChunkWithFootprint(
        call.s0, size);
    if (created.empty())
        return 0;
    std::snprintf(outId, call.u0, "%s", created.c_str());
    return 1;
}

int createChunkPart(MapEditorHostCallV1& call)
{
    auto* outId = static_cast<char*>(call.ptr0);
    if (!outId || call.u0 == 0)
        return 0;
    const std::string created = MapEditor::instance().addChunkPart(
        call.s1[0] ? call.s1 : "box");
    if (created.empty())
        return 0;
    std::snprintf(outId, call.u0, "%s", created.c_str());
    return 1;
}

int createConnector(MapEditorHostCallV1& call)
{
    auto* outId = static_cast<char*>(call.ptr0);
    if (!outId || call.u0 == 0)
        return 0;
    const std::string side = call.s1[0] ? call.s1 : "east";
    if (!MapEditor::instance().addConnector(side))
        return 0;
    std::snprintf(outId, call.u0, "%s", side.c_str());
    return 1;
}

int previewConnections(MapEditorHostCallV1& call)
{
    auto* report = static_cast<char*>(call.ptr0);
    if (!report || call.u0 == 0)
        return 0;
    const std::string text = MapEditor::instance().previewConnectionsReport();
    std::snprintf(report, call.u0, "%s", text.c_str());
    return 1;
}

}  // namespace

bool handleMapEditorHostCall(MapEditorHostCallV1& call)
{
    int result = 0;
    switch (call.op) {
    case MapEditorHostOp::GetPrimarySelection:
        result = getPrimarySelection(call);
        break;
    case MapEditorHostOp::ApplyPrimaryPosition: {
        const auto* position = static_cast<const float*>(call.ptr0);
        result = position && MapEditor::instance().moduleApplyPrimaryPosition(
            glm::vec3(position[0], position[1], position[2])) ? 1 : 0;
        break;
    }
    case MapEditorHostOp::Notify:
        MapEditor::instance().notify(call.s0);
        result = 1;
        break;
    case MapEditorHostOp::SelectObject:
        result = MapEditor::instance().selectObject(call.s0) ? 1 : 0;
        break;
    case MapEditorHostOp::SelectKind:
        result = MapEditor::instance().selectKind(call.s0) ? 1 : 0;
        break;
    case MapEditorHostOp::SelectAll:
        MapEditor::instance().selectAllEdit();
        result = 1;
        break;
    case MapEditorHostOp::ClearSelection:
        MapEditor::instance().moduleClearSelection();
        result = 1;
        break;
    case MapEditorHostOp::CopySelection:
        MapEditor::instance().copyEdit();
        result = 1;
        break;
    case MapEditorHostOp::PasteClipboard:
        MapEditor::instance().pasteEdit();
        result = 1;
        break;
    case MapEditorHostOp::CutSelection:
        MapEditor::instance().cutEdit();
        result = 1;
        break;
    case MapEditorHostOp::DuplicateSelection:
        MapEditor::instance().duplicateEdit();
        result = 1;
        break;
    case MapEditorHostOp::DeleteSelection:
        MapEditor::instance().deleteEdit();
        result = 1;
        break;
    case MapEditorHostOp::Undo:
        MapEditor::instance().undoEdit();
        result = 1;
        break;
    case MapEditorHostOp::Redo:
        MapEditor::instance().redoEdit();
        result = 1;
        break;
    case MapEditorHostOp::Save:
        result = MapEditor::instance().save() ? 1 : 0;
        break;
    case MapEditorHostOp::Reload:
        result = MapEditor::instance().reload() ? 1 : 0;
        break;
    case MapEditorHostOp::Autosave:
        result = MapEditor::instance().autosave() ? 1 : 0;
        break;
    case MapEditorHostOp::RebuildCollision:
        MapEditor::instance().flushCollisionNow();
        result = 1;
        break;
    case MapEditorHostOp::SetFreezeWorld:
        result = setFreezeWorld(call);
        break;
    case MapEditorHostOp::FocusSelection:
        MapEditor::instance().focusSelection();
        result = 1;
        break;
    case MapEditorHostOp::Nudge:
        MapEditor::instance().nudgeSelection(
            glm::vec3(call.f0, call.f1, call.f2));
        result = 1;
        break;
    case MapEditorHostOp::SetPosition:
        result = MapEditor::instance().setSelectedPosition(
            glm::vec3(call.f0, call.f1, call.f2)) ? 1 : 0;
        break;
    case MapEditorHostOp::SetRotation:
        result = MapEditor::instance().setSelectedRotation(
            glm::vec3(call.f0, call.f1, call.f2)) ? 1 : 0;
        break;
    case MapEditorHostOp::SetScale:
        result = MapEditor::instance().setSelectedScale(
            glm::vec3(call.f0, call.f1, call.f2)) ? 1 : 0;
        break;
    case MapEditorHostOp::SetSize:
        result = MapEditor::instance().setSelectedSize(
            glm::vec3(call.f0, call.f1, call.f2)) ? 1 : 0;
        break;
    case MapEditorHostOp::SetBombSiteRadius:
        MapEditor::instance().setSelectedBombSiteRadius(call.f0);
        result = 1;
        break;
    case MapEditorHostOp::RenameSelection:
        result = MapEditor::instance().setSelectedName(call.s0) ? 1 : 0;
        break;
    case MapEditorHostOp::GetSelectionCount:
        call.u0 = MapEditor::instance().moduleSelectionCount();
        result = 1;
        break;
    case MapEditorHostOp::GetSelectionItem:
        result = getSelectionItem(call);
        break;
    case MapEditorHostOp::SetSelection:
        result = setSelection(call);
        break;
    case MapEditorHostOp::GetWorldObjectCount:
        call.u0 = MapEditor::instance().moduleWorldObjectCount();
        result = 1;
        break;
    case MapEditorHostOp::GetWorldObjectInfo:
        result = getWorldObjectInfo(call);
        break;
    case MapEditorHostOp::GetMapBounds:
        result = getMapBounds(call);
        break;
    case MapEditorHostOp::CreateChunk:
        result = createChunk(call);
        break;
    case MapEditorHostOp::CreateChunkPart:
        result = createChunkPart(call);
        break;
    case MapEditorHostOp::CreateConnector:
        result = createConnector(call);
        break;
    case MapEditorHostOp::PreviewConnections:
        result = previewConnections(call);
        break;
    }
    call.result = result;
    return result != 0;
}

}  // namespace mimita::game
