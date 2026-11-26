# issue-ba939787ffa3: Map editor and creative actions cause frame freezes

## Human report (verbatim, immutable)

> can u add to issues that map editor reloading saving  copy pasting has like freeze  somietmes, copy pasting  paload routes freezes for like 0.5 sec, cant have freeze at all, for anything, relates to 240fps goal all the time regardless of waht is happening, especially in map ediotr/creation interfaces, if we want to get  creation to be as fun or more fun than playing, then need to treat it like comepeitive like 240fps ensured for all creative things


## User quote

### 2026-11-21T15:01:16.285Z — map-editor-freeze-issue

Via map-editor-freeze-issue; supports unspecified:

> can u alo add to map eidtor issue that we need dropdown for all materials/textures u acn pick, sorted alphabetical order, and todo later  make visual so u can pick from visual seeing the materials u can pcik for blocks, as wella sthe scale , like, uv , so u can make a texture stretched a lot or not stretche d alot&#x20;

also geneeral map editor business?&#x20;
like&#x20;
 the  its a lot goign on all at once , need to idk brainstorm not sur wht to do to make it more visually clear, it doensthelp that im on 1024x768 resolution bu ti tihnk it should be clear and visaullly make sense at any reoslution, thtss relating to accessibility, no matter what device waht inputmethod, what awareness what skill level etc,u shoudl be able to ahve fun&#x20;

also th erigth click menu for the thing  its not where i actual right clicked its just floating in the middle of the window&#x20;

and u should be able to edit can collide, anchored, alpha value of any block anything, the color of the block, all should be editable in the object viewer on the right side, like, make that the super strong source of ediitng things, like  u cna switch from 1 to 0 for collision  and any other fields&#x20;

also, movement mode has diffrent gizmo from rotation mode  from scale mode, all should be visible and doable with handles&#x20;

And need to ensure that u can click a block and drag it with LMB, like across the faces of other blocsk

and make it so the GUI is like super prettier and scrollable more scroll bars or more condensing like
where is snap to grid option, need to have things be super visible but also still simplenot overwhelmigm , simple neough that beginner can make super cool things, but goodenouhg that a profesional user can m ake supre cool things too

## Agent suggestion

### 2026-11-21T15:01:18.055Z — map-editor-freeze-issue

Scope extension from follow-up report: capture map-editor authoring and accessibility work as well as freezes. User requests an alphabetical material/texture picker with a later visual preview browser and UV scale controls; a clearer interface across resolutions including 1024x768 and different devices/input/skill levels; context menus positioned at the right-click point; a stronger right-side object inspector for collision, anchored, alpha, color, and other fields; visible handles for move/rotate/scale; LMB dragging blocks across other block faces; clearer scrollable/condensed layout and visible snap-to-grid control. The inline screenshot visibly shows dense overlapping editor/HUD/text layers, supporting a focused usability review. Treat the visual material browser as a later step as requested. These are requests, not implemented or runtime-verified.

## Triage

### 2026-11-21T15:02:29.573Z — map-editor-freeze-issue

Summary: Map editor frame freezes plus authoring and accessibility gaps: alphabetical material selection and UV scale (visual picker later), clear responsive UI, correctly placed context menus, comprehensive right-side object properties, move/rotate/scale handles, LMB block dragging, scrollable/condensed controls, and visible grid snapping. User reports ~0.5-second Payload route copy/paste freeze; all creative workflows should meet the existing 240 FPS goal.
Owner: (unchanged)
Priority: high

## Agent suggestion

### 2026-11-21T15:27:41.404Z — root-dll-status-review

Editor-freeze slice of the global 240 FPS goal (issue-549680e53b26). Treat any frame >4.167 ms during map reload, save, or Payload route copy/paste as a failure; measure per editor operation scope. First trace: bounded owner-level timing around reload/save and Payload route copy/paste to locate the reported ~500 ms stall.

## Agent suggestion

### 2026-11-21T15:33:04.297Z — opencode

opencode trace hypothesis for cold handoff (not yet runtime-verified): the reported ~0.5s Payload-route copy/paste freeze aligns with synchronous work on the input/tick path, not the copy itself. Candidate owners: (1) MapEditor::pasteClipboard (src/mapeditor/map-editor.cpp:1864) calls renumberPayloadRoute() and then rebuildGeometry(); (2) MapEditor::rebuildGeometry (src/mapeditor/map-editor.cpp:4043) runs applyAuthoredMapGeometry with rebuildCollision=true and staticFaceRemoval=true inline, so each paste pays a full authored-geometry + collision rebuild on the frame thread; (3) MapEditor::reload (src/mapeditor/map-editor.cpp:3841) also calls rebuildGeometry synchronously; (4) MapEditor::autosave (src/mapeditor/map-editor.cpp:3138) calls MapConfigRegistry::save (src/gamemode/map-config.cpp:1668), which reads+parses the existing file, dumps the full authored JSON, and writes it inline; explicit save (map-editor.cpp:3798) adds saveVersion plus validatePayloadRoute. Suggested first evidence step: add bounded owner-level timing scopes to these four operations (duration_ms + entity/block/route/chunk counts) and run a pinned editor workload that reloads, saves, and pastes Payload routes, then read the exact events.jsonl to attribute the >4.167ms frames before any optimization. Do not split async until the responsible scope is measured.

## Scheduled independent review

### 2026-11-25T23:39:20.452Z — rocket-league-input

A bounded independent review was queued as wake wake-54803a3bce8c after 747 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence)

### 2026-11-25T23:47:10.239Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake wake-54803a3bce8c, 2026-11-25T23:39Z). Read-only: no source edit, build, close, or fix claim. Append-only; prior text preserved.

ISSUE STATE OBSERVED
- issue readiness: ready=true, score 8/8, missing=[], state=triaged, owner empty, tasks=0, reviews=0 before this append, 0 linked build/run/proof. Issue evidence fields build/run/journal=null, runtime_verified=false; only the user report exists.
- Doc: docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md

SOURCE OBSERVED (current checkout; prior 2026-11-21 hypothesis line numbers are stale, call chain unchanged)
- src/mapeditor/map-editor.cpp:1908 pasteClipboard -> MapConfigRegistry::renumberPayloadRoute() (1985) -> rebuildGeometry() (1987) inline on the caller/frame thread.
- MapEditor::rebuildGeometry (4262-4271) calls applyAuthoredMapGeometry(world, mapId, {rebuildCollision=true, staticFaceRemoval=true}) at 4265-4268 synchronously.
- MapEditor::reload (4060-4088) calls rebuildGeometry() at 4082 after the registry load.
- MapEditor::autosave (3357-3386) calls MapConfigRegistry::save(false) inline at 3362 and logs map-editor.autosaved with duration_ms (3368-3374).
- MapEditor::save (4017-4058) calls registry.save, optional project version save, and validatePayloadRoute inline; no duration_ms.
- Instrumentation gap: pasteClipboard, rebuildGeometry, save, and reload have no own duration_ms. reload logs map-editor.reload.result (success/path/revision only, 4067-4075). map-authored-geometry.applied (src/map/map-authored-geometry.cpp:1074-1094) logs static_union_ms and counts but not total apply time. Among the four editor operations named by the 2026-11-21 plan, only autosave got a duration_ms log; the collision sub-steps are timed but there is no paste-level or rebuild-level duration.

JOURNAL EVIDENCE (new, indirect)
- No editor-operation journal exists: rg across C:\mimita-v9\logs for map-editor.(autosaved|reload.result|command|scene.applied|drag-probe|object.deleted|paste|chunk-preview|entity.transform.applied|object-resized) returned zero matches. Only map-editor.config.loaded (startup config) appears. Reload/save/paste/rebuild from the editor have never been captured, so there is no direct paste proof.
- The same synchronous function is timed in non-editor runs. Pinned: run_id 20261125_204934_01; build_id 2026-11-25T20:35:21.601166Z:ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764; exe sha256 ff20ae8525ee2afc5727a0ca055796369445e697123df0bef8bb4d5b46422764; game generation 645 (swap at t=257.21); journal logs/2026-11-25/20261125_204934_01/client-01/events.jsonl.
  - t=323.482 map.collision.static-solid-union completed: map=funworld3, input_triangles=41326, union_solids=145, union_ms=1165.1213.
  - t=323.500 map.collision.static-face-removal completed: elapsed_ms=16.9262 (removed=0).
  - t=323.677 and t=324.722 performance.stall-watchdog fired: frame_heartbeat_age_ms=1681 and 2726 ("main thread still has not completed a frame").
  - t=327.140 map-authored-geometry.applied: static_union_ms=1173.7211, collision_triangles=44352, render_vertices=128124.
  - Trigger was a map load/join (network.connect.stage, reason "loaded requested join map"), NOT a map-editor action. It still proves this exact applyAuthoredMapGeometry path can block the client main thread for >1.1 s.
- Other client samples: arena1 10 blocks union_ms=6.06/7.22 idle but 55.18/48.70 in other runs; dust2cyberiav4 485 solids hits solid_count_cap fallback (union_ms=0) but static-face-removal elapsed_ms=43.75-53.07 with ~203k candidate_pairs; slaughterhouse1 removed 2301 triangles.

INFERENCE / UNCERTAINTY
- Medium confidence: the reported ~0.5 s Payload-route paste freeze is consistent with the synchronous applyAuthoredMapGeometry collision rebuild run by pasteClipboard->rebuildGeometry. The observed 1.165 s union-alone cost on a 44k-triangle authored map is the same routine the editor calls; if the user's paste yields comparable authored collision counts, the freeze is real and can exceed the reported 0.5 s.
- Low confidence on the user's exact case: no editor run, no map identity, and no paste/route/block/chunk counts. arena1-like 10-block maps show only 6-7 ms union when unloaded, so not every paste need freeze 0.5 s; cost likely scales with collision triangle/solid count and host contention (arena1 union ranged 6 ms to 55 ms).
- Unknown: whether the 0.5 s also includes MapConfigRegistry::save/toJsonString/disk write. Paste does not call autosave directly (autosave is debounced in tick, 3342-3355), so paste's synchronous cost is renumber+rebuild, but this was not measured.

CONTRARY EVIDENCE
- The >1.1 s stall observed was a map load/join, not paste; it does not prove paste-caused freeze.
- The union step can be skipped (solid_count_cap fallback, union_ms=0) on high-solid maps, shifting cost to static-face-removal (43-53 ms) or other steps; the dominant cost may be map-dependent.
- Small unloaded arena1 samples argue against a universal 0.5 s paste freeze.

NEXT DISCRIMINATING CHECK (smallest, run/read only)
1. Pinned dev-loop editor run on the user's affected map: open the map editor, do one Payload-route paste, one explicit save, and one reload on the same build/run identity.
2. Read that exact client events.jsonl for: map.collision.static-solid-union union_ms; map.collision.static-face-removal elapsed_ms; map-authored-geometry.applied static_union_ms/collision_triangles; performance.stall-watchdog frame_heartbeat_age_ms; PERFORMANCE_FRAME/performance.fps max_frame_ms and frames_over_budget.
3. Interpretation: if these fire on paste/reload, the synchronous rebuild is the owner and the fix should defer or bound it off the frame path; if they do not fire, next scope is MapConfigRegistry::renumberPayloadRoute/save (toJsonString+disk) or editor UI/layout.
4. Missing witness to add (owner-approved, bounded): a map-editor.paste / map-editor.rebuild duration_ms event so paste is directly attributable. Do not treat the map-load evidence above as paste proof.

Confidence: medium on shared-path cost; low on paste-specific causation. Acceptance unchanged: no frame >4.167 ms during the controlled editor workload, with hardware, workload, max/p99.99, and journal recorded.
