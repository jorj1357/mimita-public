// 09 13 2026
/* purpose
* Hot replaceable editor/inspection package. Owns creation-mode state,
* selection policy (hovered vs selected), overlap cycling, clipboard edits,
* overlay layout, and the modecreate command. The kernel provides capabilities
* (spatial query, inspection, draw, fork, input, shared state) and plain data;
* this module never touches live engine objects.
* Registered through the generic runtime package (no new EXE slot).
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"
#include "hot-reload/hot-package.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint32_t kPersistMagic = 0x45444954u;  // 'EDIT'
constexpr std::uint32_t kMaxCandidates = EDITOR_MAX_CANDIDATES;

bool hasV2(const EditorContextV1* context)
{
    return context && context->structSize >= sizeof(EditorContextV1);
}

GameSharedStateV1* asShared(EditorContextV1* context)
{
    if (!context || !context->permanentStorage ||
        context->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(context->permanentStorage);
    if (shared->magic != GAME_SHARED_MAGIC)
        return nullptr;
    return shared;
}

struct EditorPersistV1 {
    std::uint32_t magic;
    // Hover / selection.
    std::uint64_t hoveredEntity;
    std::uint32_t hoveredHitKind;
    std::uint32_t candidateCount;
    std::uint32_t candidateIndex;
    std::uint64_t candidateEntity[kMaxCandidates];
    std::uint32_t candidateHitKind[kMaxCandidates];
    std::uint32_t candidateWorldKind[kMaxCandidates];
    std::uint32_t candidateWorldIndex[kMaxCandidates];
    float candidatePoint[kMaxCandidates][3];
    // Selected object snapshot (for the outline + clipboard).
    std::uint32_t selectedHitKind;
    std::uint32_t selectedWorldKind;
    std::uint32_t selectedWorldIndex;
    float selectedCenter[3];
    float selectedSize[3];
    char selectedMaterial[48];
    // Clipboard.
    std::uint32_t hasClipboard;
    std::uint32_t clipHitKind;
    std::uint64_t clipEntity;
    std::uint32_t clipWorldKind;
    std::uint32_t clipWorldIndex;
    float clipPosition[3];
    float clipSize[3];
    // Edge tracking.
    std::uint32_t prevCopy;
    std::uint32_t prevPaste;
    std::uint32_t prevDelete;
    // Status line.
    char status[96];
    // Hot-tweakable knobs.
    float moveStep;
    float outlineThickness;
    std::uint32_t outlinePeriodTicks;
};

EditorPersistV1* persist(EditorContextV1* context)
{
    if (!context || !context->permanentStorage ||
        context->permanentStorageSize <
            sizeof(GameSharedStateV1) + sizeof(EditorPersistV1))
        return nullptr;
    // Reserve the first GameSharedStateV1 bytes for kernel shared state.
    EditorPersistV1* state = reinterpret_cast<EditorPersistV1*>(
        reinterpret_cast<std::uint8_t*>(context->permanentStorage) +
        sizeof(GameSharedStateV1));
    if (state->magic != kPersistMagic) {
        std::memset(state, 0, sizeof(EditorPersistV1));
        state->magic = kPersistMagic;
        state->moveStep = 1.0f;
        state->outlineThickness = 1.0f;
        state->outlinePeriodTicks = 60;
    }
    return state;
}

EditorPersistV1* gPersist = nullptr;
GameSharedStateV1* gShared = nullptr;

bool createModeActive()
{
    return gShared && (gShared->modeFlags & GAME_MODE_FLAG_CREATION);
}

void setStatus(EditorPersistV1* st, const char* text)
{
    if (!st)
        return;
    std::strncpy(st->status, text ? text : "", sizeof(st->status) - 1);
    st->status[sizeof(st->status) - 1] = '\0';
}

// ── command: modecreate [0|1] ───────────────────────────────────────
void MIMITA_GAME_CALL modecreateCommand(void* /*host*/, const char* args)
{
    if (!gShared) {
        std::printf("[MODE CREATE] not ready (no shared state yet)\n");
        return;
    }
    const bool wantOn = (args && args[0] == '0') ? false : true;
    if (wantOn)
        gShared->modeFlags |= GAME_MODE_FLAG_CREATION;
    else
        gShared->modeFlags &= ~GAME_MODE_FLAG_CREATION;
    // Journal-visible via stdout (kernel logging capability is future work).
    std::printf("[MODE CREATE] %s\n", wantOn ? "ON" : "OFF");
}

// ── tick ────────────────────────────────────────────────────────────
void MIMITA_GAME_CALL onTick(const EditorStateV1* state, EditorContextV1* context,
                             EditorResultV1* out)
{
    if (!out)
        return;
    *out = EditorResultV1{};
    gShared = asShared(context);
    gPersist = persist(context);
    EditorPersistV1* st = gPersist;

    if (!state || !state->enabled || !context || !context->queryRay ||
        !createModeActive()) {
        out->handled = 0;
        return;
    }
    out->handled = 1;
    out->hitKind = EDITOR_HIT_NONE;

    EditorQueryV1 query{};
    context->queryRay(context->host, state->origin, state->dir, state->maxDistance,
                      EDITOR_MAX_CANDIDATES, &query);

    // Store the full candidate stack for overlap cycling.
    if (st) {
        st->candidateCount = query.count;
        for (std::uint32_t i = 0; i < query.count; ++i) {
            st->candidateEntity[i] = query.hits[i].entity;
            st->candidateHitKind[i] = query.hits[i].kind;
            st->candidatePoint[i][0] = query.hits[i].point[0];
            st->candidatePoint[i][1] = query.hits[i].point[1];
            st->candidatePoint[i][2] = query.hits[i].point[2];
        }
        if (query.count == 0)
            st->candidateIndex = 0;
        else if (st->candidateIndex >= query.count)
            st->candidateIndex = 0;
    }

    // Wheel cycles the overlap candidate.
    if (st && hasV2(context) && context->input && query.count > 1) {
        if (context->input->cyclePrev)
            st->candidateIndex = (st->candidateIndex + query.count - 1) % query.count;
        if (context->input->cycleNext)
            st->candidateIndex = (st->candidateIndex + 1) % query.count;
    }

    const std::uint32_t best = (st && query.count > 0) ? st->candidateIndex : 0;
    if (query.count == 0) {
        if (st) {
            st->hoveredHitKind = EDITOR_HIT_NONE;
            st->hoveredEntity = 0;
        }
        if (gShared)
            gShared->hoveredEntity = 0;
        return;
    }

    const EditorCandidateV1& hit = query.hits[best];
    out->hitKind = hit.kind;
    out->selectedEntity = hit.entity;
    out->distance = hit.distance;
    if (st) {
        st->hoveredHitKind = hit.kind;
        st->hoveredEntity = hit.entity;
    }
    if (gShared)
        gShared->hoveredEntity = hit.entity;

    // Resolve world-object details for the hovered world hit.
    if (st && hit.kind == EDITOR_HIT_WORLD && hasV2(context) &&
        context->worldObjectInfo) {
        EditorWorldObjectV1 wo{};
        context->worldObjectInfo(context->host, hit.point, &wo);
        st->selectedWorldKind = wo.kind;
        st->selectedWorldIndex = wo.index;
        for (int i = 0; i < 3; ++i) {
            st->selectedCenter[i] = wo.center[i];
            st->selectedSize[i] = wo.size[i];
        }
        std::strncpy(st->selectedMaterial, wo.material, sizeof(st->selectedMaterial) - 1);
    } else if (st) {
        st->selectedWorldKind = 3;
        st->selectedWorldIndex = hit.worldTriangle;
        for (int i = 0; i < 3; ++i) {
            st->selectedCenter[i] = hit.point[i];
            st->selectedSize[i] = 0.0f;
        }
        st->selectedMaterial[0] = '\0';
    }
    if (st)
        st->selectedHitKind = hit.kind;

    if (hit.kind == EDITOR_HIT_ENTITY && context->inspect) {
        if (context->inspect(context->host, hit.entity, &out->inspection))
            out->hasInspection = out->inspection.valid ? 1u : 0u;
    }

    // CTRL+LMB commits the hovered candidate as the persistent selection.
    if (hasV2(context) && context->input && context->input->selectPressed) {
        if (gShared) {
            gShared->selectedEntity = hit.entity;
            gShared->modeFlags |= GAME_MODE_FLAG_CREATION;
        }
        if (st) {
            std::snprintf(st->status, sizeof(st->status),
                          "selected entity=%llu kind=%u",
                          (unsigned long long)hit.entity, hit.kind);
        }
    }

    // Clipboard edits (rising edges).
    if (st && context->input && context->fork) {
        const EditorInputV1& in = *context->input;
        const bool copyEdge = in.copyPressed && !st->prevCopy;
        const bool pasteEdge = in.pastePressed && !st->prevPaste;
        const bool deleteEdge = in.deletePressed && !st->prevDelete;
        st->prevCopy = in.copyPressed;
        st->prevPaste = in.pastePressed;
        st->prevDelete = in.deletePressed;

        if (copyEdge) {
            st->hasClipboard = 1;
            st->clipHitKind = hit.kind;
            st->clipEntity = hit.entity;
            st->clipWorldKind = st->selectedWorldKind;
            st->clipWorldIndex = st->selectedWorldIndex;
            for (int i = 0; i < 3; ++i) {
                st->clipPosition[i] = st->selectedCenter[i];
                st->clipSize[i] = st->selectedSize[i];
            }
            setStatus(st, "copied");
        }
        if (pasteEdge && st->hasClipboard) {
            EditorForkArgsV1 args{};
            args.op = EDITOR_FORK_DUPLICATE;
            args.sourceKind = st->clipHitKind;
            args.sourceWorldKind = st->clipWorldKind;
            args.sourceWorldIndex = st->clipWorldIndex;
            args.sourceEntity = st->clipEntity;
            args.position[0] = st->clipPosition[0] + st->moveStep;
            args.position[1] = st->clipPosition[1];
            args.position[2] = st->clipPosition[2];
            for (int i = 0; i < 3; ++i)
                args.size[i] = st->clipSize[i];
            context->fork(context->host, &args);
            setStatus(st, args.ok ? "pasted" : "paste failed");
        }
        if (deleteEdge) {
            EditorForkArgsV1 args{};
            args.op = EDITOR_FORK_DELETE;
            args.sourceEntity = hit.entity;
            context->fork(context->host, &args);
            setStatus(st, args.ok ? "deleted from fork" : "delete failed");
        }
    }
}

// ── draw ────────────────────────────────────────────────────────────
void drawLine(EditorContextV1* context, const char* text, float& y,
              const float rgba[4], float scale = 0.32f)
{
    context->drawText(context->host, text, 24.0f, y, scale, rgba);
    y += 16.0f;
}

void outlineColor(std::uint32_t tick, std::uint32_t startTick,
                  std::uint32_t period, float out[4])
{
    if (period == 0) {
        out[0] = out[1] = out[2] = 0.0f;
        out[3] = 0.0f;
        return;
    }
    const std::uint32_t phase = (tick - startTick) % period;
    const std::uint32_t half = period / 2;
    float c = (phase < half)
        ? (float)phase / (float)half
        : (float)(period - phase) / (float)half;
    c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
    out[0] = out[1] = out[2] = c;
    out[3] = 0.15f + 0.85f * c;
}

void MIMITA_GAME_CALL onDraw(const EditorStateV1* state, const EditorResultV1* result,
                             EditorContextV1* context)
{
    if (!state || !state->enabled || !context || !context->drawText ||
        !context->drawRect)
        return;
    EditorPersistV1* st = gPersist ? gPersist : persist(context);
    const bool mode = createModeActive();

    const float panel[4] = {0.05f, 0.07f, 0.10f, 0.85f};
    const float green[4] = {0.4f, 1.0f, 0.6f, 1.0f};
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float grey[4] = {0.7f, 0.75f, 0.8f, 1.0f};
    const float yellow[4] = {1.0f, 0.9f, 0.4f, 1.0f};

    // Outline only the committed selection (wire box/capsule/sphere).
    if (mode && st && gShared && gShared->selectedEntity && hasV2(context) &&
        context->drawOutline) {
        float rgba[4];
        outlineColor(state->tick, 0, st->outlinePeriodTicks, rgba);
        context->drawOutline(context->host, st->selectedHitKind,
                             gShared->selectedEntity, st->selectedWorldKind,
                             st->selectedWorldIndex, rgba, st->outlineThickness, 0u);
    }

    // Fork visualization: every recorded edit as a colored wire box, so
    // duplicate/transform/delete are visible immediately.
    if (mode && hasV2(context) && context->drawWireBox && context->forkOpCount &&
        context->forkOp) {
        const std::uint32_t n = context->forkOpCount(context->host);
        for (std::uint32_t i = 0; i < n && i < 64; ++i) {
            EditorForkArgsV1 op{};
            if (!context->forkOp(context->host, i, &op))
                continue;
            float rgba[4] = {0.6f, 0.6f, 0.6f, 0.9f};
            if (op.op == EDITOR_FORK_DUPLICATE) {
                rgba[0] = 0.2f; rgba[1] = 1.0f; rgba[2] = 0.3f;
            } else if (op.op == EDITOR_FORK_DELETE) {
                rgba[0] = 1.0f; rgba[1] = 0.2f; rgba[2] = 0.2f;
            } else if (op.op == EDITOR_FORK_SET_TRANSFORM) {
                rgba[0] = 1.0f; rgba[1] = 0.9f; rgba[2] = 0.2f;
            }
            const float size[3] = {
                op.size[0] > 0.0f ? op.size[0] : 1.0f,
                op.size[1] > 0.0f ? op.size[1] : 1.0f,
                op.size[2] > 0.0f ? op.size[2] : 1.0f};
            context->drawWireBox(context->host, op.position, size, rgba);
        }
    }

    const float x = 16.0f;
    const float w = 480.0f;
    const float top = 96.0f;
    const float lineH = 16.0f;
    std::uint32_t lines = 6 + (st ? st->candidateCount : 0);
    const float h = 16.0f + (float)lines * lineH;
    context->drawRect(context->host, x, top, w, h, panel);

    char buf[200];
    float y = top + 6.0f;
    drawLine(context, mode ? "CREATE MODE: ON" : "CREATE MODE: OFF (type modecreate 1)",
             y, mode ? green : grey, 0.40f);
    drawLine(context, "CTRL+LMB=select  wheel=cycle  C=copy V=paste DEL=delete",
             y, grey, 0.28f);

    if (!mode)
        return;

    std::snprintf(buf, sizeof(buf), "hovered=%llu  selected=%llu  stack=%u/%u",
                  (unsigned long long)(gShared ? gShared->hoveredEntity : 0),
                  (unsigned long long)(gShared ? gShared->selectedEntity : 0),
                  st ? st->candidateIndex + 1 : 0u,
                  st ? st->candidateCount : 0u);
    drawLine(context, buf, y, white);

    if (st && st->candidateCount > 1) {
        drawLine(context, "candidates:", y, grey, 0.28f);
        for (std::uint32_t i = 0; i < st->candidateCount && i < 6; ++i) {
            const char* kind = st->candidateHitKind[i] == EDITOR_HIT_ENTITY
                ? "entity" : "world";
            std::snprintf(buf, sizeof(buf), "  [%u/%u] %s %llu%s",
                          i + 1, st->candidateCount, kind,
                          (unsigned long long)st->candidateEntity[i],
                          (i == st->candidateIndex) ? "  <" : "");
            drawLine(context, buf, y, i == st->candidateIndex ? yellow : white, 0.28f);
        }
    }

    if (st && st->status[0])
        drawLine(context, st->status, y, green, 0.28f);

    if (result && result->hasInspection) {
        const EditorInspectionV1& ins = result->inspection;
        std::snprintf(buf, sizeof(buf), "domain=%u legacy=%u pos=(%.1f, %.1f, %.1f)",
                      ins.domain, ins.legacyId, ins.position[0], ins.position[1],
                      ins.position[2]);
        drawLine(context, buf, y, grey, 0.28f);
    }
    if (st && st->hasClipboard) {
        std::snprintf(buf, sizeof(buf), "clipboard: entity=%llu kind=%u",
                      (unsigned long long)st->clipEntity, st->clipHitKind);
        drawLine(context, buf, y, grey, 0.28f);
    }
}

const GameEditorModuleV1 gEditorModuleV1 = {
    1u, sizeof(GameEditorModuleV1), onTick, onDraw};

} // namespace

// Registered as a generic runtime command (terminal dispatches hot commands).
void MimitaModecreateCommand(void* host, const char* args)
{
    modecreateCommand(host, args);
}

const MimitaHotPackage::CommandRegistrar s_modecreateCmd{
    {"modecreate", "modecreate [0|1] - creation/inspection mode", 0,
     MimitaModecreateCommand}};

// Toggle the hot movement step (movement.main takes over the built-in step).
void MimitaHotMovementCommand(void* /*host*/, const char* args)
{
    if (!gShared) {
        std::printf("[HOT MOVEMENT] not ready (no shared state yet)\n");
        return;
    }
    const bool on = (args && args[0] == '0') ? false : true;
    if (on)
        gShared->modeFlags |= GAME_MODE_FLAG_HOT_MOVEMENT;
    else
        gShared->modeFlags &= ~GAME_MODE_FLAG_HOT_MOVEMENT;
    std::printf("[HOT MOVEMENT] %s\n", on ? "ON" : "OFF");
}

const MimitaHotPackage::CommandRegistrar s_hotMovementCmd{
    {"hotmovement", "hotmovement [0|1] - use the hot movement step", 0,
     MimitaHotMovementCommand}};

// Kernel resolves the named "editor" module for the fixed-tick/overlay hooks.
const GameModuleDescriptor* MimitaGetEditorModule()
{
    static const GameModuleDescriptor descriptor = {
        "editor", 1u, sizeof(GameEditorModuleV1), &gEditorModuleV1};
    return &descriptor;
}

#endif
