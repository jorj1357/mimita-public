#include "avatar-editor-animation.h"

#include <cstdio>
#include <string>

#include "avatar.h"
#include "avatar-editor-helpers.h"
#include "avatar-editor-scroll.h"
#include "entities/player.h"
#include "entities/player-avatar-animations.h"
#include "gui/ui-system.h"

// Data-driven Animation tab. Everything below reads the current avatar's rig
// and animation set from the registry; nothing is hard-coded to the six
// canonical parts, so new roles/parts/clips appear automatically. Intended to
// grow (keyframe editing, live scrub) without changing this structure.
void drawAvatarAnimationTab(GLFWwindow* win, float px, float py, float pw, float ph)
{
    AvatarSystem& avatar = AvatarSystem::instance();
    if (!avatar.hasAvatar()) {
        uiDrawText(editorLabelText("animNoAvatar", "No avatar loaded.").c_str(),
                   uiScaleX(px), uiScaleY(py), avatarEditorHintFontSize,
                   {0.5f, 0.6f, 0.7f, 1.0f});
        return;
    }

    const std::string name = avatar.currentName();
    AvatarAnimationRegistry& reg = AvatarAnimationRegistry::instance();
    const AvatarRig* rig = reg.getRig(name);
    const AvatarAnimationSet* set = reg.getAnimationSet(name);
    Player* preview = avatarEditorPreviewPlayer();

    float contentH = 220.0f;
    if (rig && rig->present) contentH += rig->parts.size() * 20.0f;
    if (set && set->valid) contentH += set->clips.size() * 34.0f;

    static ScrollState scroll;
    beginScroll(win, {px, py, pw, ph}, contentH, scroll);
    float y = py;

    auto section = [&](const char* id, const char* fallback) {
        uiDrawText(editorLabelText(id, fallback).c_str(), uiScaleX(px), uiScaleY(y),
                   editorLabelFontSize(id, avatarEditorSectionFontSize),
                   {0.4f, 0.6f, 0.5f, 1.0f});
        y += 24.0f;
    };
    auto line = [&](const std::string& text) {
        uiDrawText(text.c_str(), uiScaleX(px + 8.0f), uiScaleY(y),
                   avatarEditorHintFontSize, {0.8f, 0.85f, 0.95f, 1.0f});
        y += 20.0f;
    };

    uiDrawText((std::string("AVATAR: ") + name).c_str(), uiScaleX(px), uiScaleY(y),
               avatarEditorSummaryFontSize, {0.95f, 0.95f, 1.0f, 1.0f});
    y += 26.0f;

    // ── Rig ────────────────────────────────────────────────────────
    section("animRigTitle", "RIG (parts + roles)");
    if (rig && rig->present) {
        char buf[256];
        snprintf(buf, sizeof(buf), "movement=%s gravity=%.2f hover=%d parts=%zu",
                 rig->movement.mode.c_str(), rig->movement.gravityScale,
                 (int)rig->movement.hover, rig->parts.size());
        line(buf);
        for (const auto& p : rig->parts) {
            snprintf(buf, sizeof(buf), "%s  ->  node '%s'  role=%s%s",
                     p.name.c_str(), p.node.c_str(),
                     p.role.empty() ? "-" : p.role.c_str(),
                     p.physical ? "  [physical]" : "  [render-only]");
            line(buf);
        }
    } else {
        line(editorLabelText("animNoRig",
             "No rig declared: using the six canonical parts."));
    }
    y += 6.0f;

    // ── Clips ──────────────────────────────────────────────────────
    section("animClipsTitle", "CLIPS");
    if (set && set->valid) {
        for (const auto& [clipName, clip] : set->clips) {
            const std::string label = "Play " + clipName;
            UIButtonState b = editorButton(win, label.c_str(),
                                           {px + 8.0f, y, pw - 16.0f, 26.0f},
                                           {0.10f, 0.16f, 0.22f, 1.0f},
                                           avatarEditorButtonFontSize);
            if (b.clicked && preview)
                preview->playOneShot(clipName);
            y += 32.0f;
        }
    } else {
        line(editorLabelText("animNoClips",
             "No animations.json for this avatar (using the global profile)."));
    }
    y += 6.0f;

    // ── States ─────────────────────────────────────────────────────
    section("animStatesTitle", "STATES");
    if (set && set->valid) {
        char buf[256];
        snprintf(buf, sizeof(buf), "default=%s walk=%s fly=%s crawl=%s swim=%s",
                 set->states.def.c_str(), set->states.walk.c_str(),
                 set->states.fly.c_str(), set->states.crawl.c_str(),
                 set->states.swim.c_str());
        line(buf);
        snprintf(buf, sizeof(buf), "attack=%s hurt=%s death=%s",
                 set->states.attack.empty() ? "-" : set->states.attack.c_str(),
                 set->states.hurt.empty() ? "-" : set->states.hurt.c_str(),
                 set->states.death.empty() ? "-" : set->states.death.c_str());
        line(buf);
    }
    y += 6.0f;

    // ── Actions ────────────────────────────────────────────────────
    section("animActionsTitle", "ACTIONS");
    UIButtonState reload = editorButton(win, "Reload animations.json",
                                        {px + 8.0f, y, pw - 16.0f, 26.0f},
                                        {0.16f, 0.20f, 0.14f, 1.0f},
                                        avatarEditorButtonFontSize);
    if (reload.clicked)
        reg.invalidate(name);
    y += 32.0f;

    endScroll({px, py, pw, ph}, contentH, scroll);
}
