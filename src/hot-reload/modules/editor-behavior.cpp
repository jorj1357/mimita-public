// 09 13 2026
/* purpose
* Hot replaceable editor/inspector module. Owns selection policy, inspector
* formatting, and the overlay layout for creation/inspection mode. The kernel
* provides capabilities (spatial query, inspection snapshot, UI primitives) and
* plain data only; this module never touches live engine objects.
* Editor state lives in GameMemory::permanentStorage so it survives reloads.
* Does NOT link into the EXE; only into the replaceable game DLL.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/game-modules.h"

#include <cstdio>

namespace {

constexpr std::uint32_t kPersistMagic = 0x45444954u;  // 'EDIT'

// Persistent across hot reloads. `preferEntity`/`bias` are editable selection
// policy knobs: change them here and hot reload to change behavior live.
struct EditorPersistV1 {
    std::uint32_t magic;
    std::uint32_t mode;
    std::uint64_t lastSelected;
    std::uint32_t preferEntity;
    float entityBias;
    std::uint32_t showComponents;
    std::uint32_t reserved;
};

EditorPersistV1* persist(EditorContextV1* context)
{
    if (!context || !context->permanentStorage ||
        context->permanentStorageSize < sizeof(EditorPersistV1))
        return nullptr;
    EditorPersistV1* state =
        reinterpret_cast<EditorPersistV1*>(context->permanentStorage);
    if (state->magic != kPersistMagic) {
        state->magic = kPersistMagic;
        state->mode = 0;
        state->lastSelected = 0;
        state->preferEntity = 1;
        state->entityBias = 0.75f;   // prefer an entity within +0.75m of world
        state->showComponents = 1;
        state->reserved = 0;
    }
    return state;
}

void MIMITA_GAME_CALL onTick(const EditorStateV1* state, EditorContextV1* context,
                             EditorResultV1* out)
{
    if (!out)
        return;
    *out = EditorResultV1{};
    if (!state || !state->enabled || !context || !context->queryRay) {
        out->handled = 0;
        return;
    }
    EditorPersistV1* st = persist(context);
    out->handled = 1;
    out->hitKind = EDITOR_HIT_NONE;

    EditorQueryV1 query{};
    context->queryRay(context->host, state->origin, state->dir, state->maxDistance,
                      EDITOR_MAX_CANDIDATES, &query);
    if (query.count == 0)
        return;

    // Selection policy (hot-editable): nearest by distance, optionally
    // preferring an entity when it is within entityBias of the nearest world.
    std::uint32_t best = 0;
    if (st && st->preferEntity) {
        float nearestWorld = 1e30f;
        std::uint32_t nearestWorldIndex = 0;
        bool haveWorld = false;
        for (std::uint32_t i = 0; i < query.count; ++i) {
            if (query.hits[i].kind == EDITOR_HIT_WORLD &&
                query.hits[i].distance < nearestWorld) {
                nearestWorld = query.hits[i].distance;
                nearestWorldIndex = i;
                haveWorld = true;
            }
        }
        best = nearestWorldIndex;
        for (std::uint32_t i = 0; i < query.count; ++i) {
            if (query.hits[i].kind != EDITOR_HIT_ENTITY)
                continue;
            if (!haveWorld ||
                query.hits[i].distance <= nearestWorld + (st ? st->entityBias : 0.0f)) {
                best = i;
                break;
            }
        }
    }

    const EditorCandidateV1& hit = query.hits[best];
    out->hitKind = hit.kind;
    out->selectedEntity = hit.entity;
    out->distance = hit.distance;
    if (st)
        st->lastSelected = hit.entity;

    if (hit.kind == EDITOR_HIT_ENTITY && context->inspect) {
        if (context->inspect(context->host, hit.entity, &out->inspection)) {
            const EditorInspectionV1& ins = out->inspection;
            out->hasInspection = ins.valid ? 1u : 0u;
        }
    }
}

void drawLine(EditorContextV1* context, const char* text, float& y,
              const float rgba[4], float scale = 0.32f)
{
    context->drawText(context->host, text, 24.0f, y, scale, rgba);
    y += 16.0f;
}

void MIMITA_GAME_CALL onDraw(const EditorStateV1* state, const EditorResultV1* result,
                             EditorContextV1* context)
{
    if (!state || !state->enabled || !context || !context->drawText ||
        !context->drawRect)
        return;
    EditorPersistV1* st = persist(context);

    const float panel[4] = {0.05f, 0.07f, 0.10f, 0.85f};
    const float green[4] = {0.4f, 1.0f, 0.6f, 1.0f};
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    const float grey[4] = {0.7f, 0.75f, 0.8f, 1.0f};

    const float x = 16.0f;
    const float w = 420.0f;
    const float top = 96.0f;
    const float lineH = 16.0f;

    // Count lines to size the panel.
    std::uint32_t lines = 3;
    if (result && result->hitKind != EDITOR_HIT_NONE) {
        lines += 2;
        if (result->hasInspection && st && st->showComponents)
            lines += 2 + result->inspection.componentCount;
        if (result->hasInspection && result->inspection.hasConstraint)
            lines += 3;
        if (result->hasInspection && result->inspection.linkedConstraintSerial)
            lines += 1;
    }
    const float h = 16.0f + (float)lines * lineH;

    context->drawRect(context->host, x, top, w, h, panel);
    char buf[160];
    float y = top + 6.0f;
    drawLine(context, "CREATE MODE", y, green, 0.40f);

    if (!result || result->hitKind == EDITOR_HIT_NONE) {
        drawLine(context, "no target under crosshair", y, white);
        return;
    }

    if (result->hitKind == EDITOR_HIT_WORLD) {
        std::snprintf(buf, sizeof(buf), "world triangle  dist=%.2f", result->distance);
        drawLine(context, buf, y, white);
        drawLine(context, "(world geometry -> authoring object)", y, grey);
        return;
    }

    std::snprintf(buf, sizeof(buf), "entity=%llu  dist=%.2f",
                  (unsigned long long)result->selectedEntity, result->distance);
    drawLine(context, buf, y, white);

    if (result->hasInspection) {
        const EditorInspectionV1& ins = result->inspection;
        std::snprintf(buf, sizeof(buf), "domain=%u legacy=%u gen=%u pos=(%.2f, %.2f, %.2f)",
                      ins.domain, ins.legacyId, ins.generation,
                      ins.position[0], ins.position[1], ins.position[2]);
        drawLine(context, buf, y, grey);

        if (st && st->showComponents) {
            drawLine(context, "components:", y, grey);
            for (std::uint32_t i = 0; i < ins.componentCount; ++i) {
                std::snprintf(buf, sizeof(buf), "  %s", ins.components[i]);
                drawLine(context, buf, y, white, 0.30f);
            }
        }
        if (ins.hasConstraint) {
            std::snprintf(buf, sizeof(buf), "constraint serial=%u type=%u active=%u",
                          ins.constraintSerial, ins.constraintType, ins.constraintActive);
            drawLine(context, buf, y, green);
            std::snprintf(buf, sizeof(buf), "  bodyA=%llu bodyB=%llu strength=%.2f",
                          (unsigned long long)ins.constraintBodyA,
                          (unsigned long long)ins.constraintBodyB,
                          ins.constraintStrength);
            drawLine(context, buf, y, white, 0.30f);
        } else if (ins.linkedConstraintSerial != 0) {
            std::snprintf(buf, sizeof(buf), "linkedConstraint=%u", ins.linkedConstraintSerial);
            drawLine(context, buf, y, green, 0.30f);
        }
    }
}

const GameEditorModuleV1 gEditorModuleV1 = {
    1u,
    sizeof(GameEditorModuleV1),
    onTick,
    onDraw,
};

} // namespace

const GameModuleDescriptor* MimitaGetEditorModule()
{
    static const GameModuleDescriptor descriptor = {
        "editor", 1u, sizeof(GameEditorModuleV1), &gEditorModuleV1};
    return &descriptor;
}

#endif
