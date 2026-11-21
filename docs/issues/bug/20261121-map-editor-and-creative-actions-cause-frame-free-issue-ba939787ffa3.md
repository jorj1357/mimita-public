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
