# Runtime quiesce plan (Pass A, 2026-11-12)

**Status:** static classification of the `RUNTIME_PROOF_REQUIRED` rows. This is a
plan, not runtime evidence. `TRANSIENT_CANDIDATE` rows still need a confirmation
read; `OWNED_PERSISTENT` rows need their owner/stop/join/release path traced.

- Machine table: `docs/handoffs/cpp-runtime-quiesce-plan-20261112.csv`
- Rows: 297

## Method

1. Read the recorded signals for each row (thread/job, callback, resource, network, known blocker).
2. Split compound signals into categories.
3. Attach the barrier rule the file must use.
4. Mark pure file-IO rows as `TRANSIENT_CANDIDATE` for a per-call handle check.
5. Everything else remains `OWNED_PERSISTENT` until its owner and stop/join/release path are traced.

## Category counts

| Category | Rows |
|---|---:|
| file-io | 158 |
| gpu-resource | 120 |
| network-context | 55 |
| network-resource | 24 |
| os-resource | 16 |
| callback | 16 |
| async-worker | 15 |
| known-blocker | 8 |
| audio-resource | 2 |

## Barrier rules

| Category | Required rule |
|---|---|
| async-worker | CAP_JOBS: submit only; cancel/drain/join before unload |
| callback | CAP_CALLBACKS: generation token; unregister before unload |
| gpu-resource | CAP_RESOURCES: release with the context current |
| network-resource | CAP_TRANSPORT: close/drain; session owner quiesce |
| network-context | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| audio-resource | resident audio owner; stop voice/engine before unload |
| file-io | static check: transient per-call handles; no retained resource |
| os-resource | CAP_RESOURCES or resident owner; close handle |
| known-blocker | explicit owner quiesce plan; may stay resident |
| process-entry | resident/bootstrap exception |
| other | manual trace |

## Full row table

| Row | Lines | Categories | Review state | Barrier rule |
|---|---:|---|---|---|
| `src/analytics/analytics-config.cpp` | 89 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/analytics/analytics-consent.cpp` | 75 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/analytics/analytics-events-impl.cpp` | 187 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/analytics/analytics-manager.cpp` | 333 | gpu-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/analytics/analytics-uploader.cpp` | 157 | async-worker,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; explicit owner quiesce plan; may stay resident |
| `src/audio/hitmarker-audio.cpp` | 224 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/audio/music-manager-widget.cpp` | 220 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/auth/auth-popup.cpp` | 515 | gpu-resource,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_RESOURCES or resident owner; close handle |
| `src/auth/auth-system.cpp` | 469 | os-resource | OWNED_PERSISTENT | CAP_RESOURCES or resident owner; close handle |
| `src/auth/auth-token.cpp` | 403 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/avatar/avatar-atlas.cpp` | 1032 | async-worker,callback,gpu-resource,file-io,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; explicit owner quiesce plan; may stay resident |
| `src/avatar/avatar-autosave.cpp` | 247 | callback,file-io | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; static check: transient per-call handles; no retained resource |
| `src/avatar/avatar-commands.cpp` | 468 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/avatar/avatar-drop-target.cpp` | 185 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-animation.cpp` | 125 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-colors.cpp` | 50 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-cosmetics.cpp` | 420 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/avatar/avatar-editor-dropdown.cpp` | 110 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-face.cpp` | 366 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-helpers.cpp` | 253 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-image.cpp` | 200 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-library.cpp` | 115 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-outfits.cpp` | 103 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-parts.cpp` | 98 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-popups.cpp` | 141 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-presets.cpp` | 81 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor-scroll.cpp` | 37 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/avatar/avatar-editor.cpp` | 431 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/avatar/avatar-fileops.cpp` | 252 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/avatar/avatar.cpp` | 1462 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/avatar/character-manifest.cpp` | 108 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/avatar/character-registry.cpp` | 116 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/avatar/cosmetic-system.cpp` | 283 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/combat/death-system.cpp` | 477 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/melee-client.cpp` | 1450 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/melee-rules-config.cpp` | 231 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/projectile-render.cpp` | 483 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/combat/revolver-system.cpp` | 436 | gpu-resource,network-context | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-audio.cpp` | 117 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-collision-config.cpp` | 660 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/weapon-fire-damage.cpp` | 884 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-godball-movement.cpp` | 723 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-json-config.cpp` | 928 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/weapon-melee-config.cpp` | 676 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/weapon-model-cache.cpp` | 216 | async-worker,gpu-resource,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_RESOURCES: release with the context current; explicit owner quiesce plan; may stay resident |
| `src/combat/weapon-runtime.cpp` | 196 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/combat/weapon-spyknife.cpp` | 650 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-system.cpp` | 1502 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/combat/weapon-viewmodel.cpp` | 624 | gpu-resource,network-context | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/competitive/competitive-result.cpp` | 149 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/competitive/competitive-ui.cpp` | 103 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/competitive/competitive.cpp` | 253 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/actor-collision-config.cpp` | 402 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/camera-config.cpp` | 218 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/collision-config.cpp` | 282 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/collision-lod-config.cpp` | 111 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/gameplay-config.cpp` | 181 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/impact-decals-config.cpp` | 325 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/killfeed-config.cpp` | 122 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/material-config.cpp` | 157 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/movement-config.cpp` | 723 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/networking-config.cpp` | 802 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/player-settings.cpp` | 134 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/player-visuals-config.cpp` | 388 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/ragdoll-death-config.cpp` | 122 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/settings-backup.cpp` | 145 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/size-scaling-config.cpp` | 123 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/spatial-audio-config.cpp` | 599 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/spawn-velocity-config.cpp` | 186 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/weapon-hitfx-config.cpp` | 401 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/config/weapon-tracers-config.cpp` | 143 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/crosshair/crosshair-config.cpp` | 130 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/debug/debug-diag-commands.cpp` | 414 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/debug/debug-diag.cpp` | 184 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/debug-visuals-config.cpp` | 47 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/debug/debug-visuals-labels.cpp` | 158 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/debug-visuals-lines.cpp` | 631 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/debug-visuals-scene.cpp` | 598 | gpu-resource,network-context | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/debug/debug-visuals-tris.cpp` | 114 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/debug-visuals.cpp` | 200 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/gl-debug.cpp` | 102 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/debug/npckillfeed-log.cpp` | 71 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/debug/validate-assets.cpp` | 138 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/account-config.cpp` | 308 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/dev-config.cpp` | 146 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/dev-log-commands.cpp` | 194 | file-io,os-resource | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/devtools/dev-overlay.cpp` | 67 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/devtools/scenario-player.cpp` | 927 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/scenario-recorder.cpp` | 1115 | async-worker,file-io,network-context | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/devtools/scenario-server-playback.cpp` | 827 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/scenario-server-recorder.cpp` | 1152 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/scenario-tracks.cpp` | 697 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/terminal-input.cpp` | 322 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/devtools/terminal.cpp` | 1026 | callback,gpu-resource,file-io | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/duel/duel-history.cpp` | 112 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/duel/duel-ui.cpp` | 306 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/effects/effect-part-render.cpp` | 1018 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/effects/hit-effects-config.cpp` | 668 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/effects/muzzle-flash-config.cpp` | 79 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/engine/engine-init.cpp` | 65 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/engine/engine-tick-camera.cpp` | 1570 | callback,gpu-resource,network-context | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/engine/engine-tick-net.cpp` | 1375 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/engine/engine-tick-render.cpp` | 1184 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/engine/engine-tick-replay.cpp` | 1055 | async-worker,file-io,os-resource,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle; explicit owner quiesce plan; may stay resident |
| `src/engine/engine-tick-setup.cpp` | 310 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/engine/engine-tick-ui-replay-hud.cpp` | 423 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/engine/engine-tick.cpp` | 463 | callback,file-io,os-resource | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/entities/actor-body.cpp` | 169 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/entities/aimbody-config.cpp` | 323 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/entities/player-animation-config.cpp` | 327 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/entities/player-avatar-animations.cpp` | 491 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/entities/player-config.cpp` | 182 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/entities/player-loader.cpp` | 1520 | async-worker,callback,gpu-resource,file-io,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; explicit owner quiesce plan; may stay resident |
| `src/entities/player-render-mesh.cpp` | 512 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/entities/player-render.cpp` | 795 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/game/bomb-tag-config.cpp` | 93 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/game/duel.cpp` | 500 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/game/game-cli.cpp` | 3320 | gpu-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/game/spawn-utils.cpp` | 356 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/gamemode-map-pool.cpp` | 104 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/gamemode.cpp` | 1118 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/map-config.cpp` | 2186 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/match-roles.cpp` | 672 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/mode-pack-registry.cpp` | 315 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gamemode/monster-pools.cpp` | 83 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gui/font-stuff/font-loader.cpp` | 327 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/gui/gui-bindings.cpp` | 165 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-editor-overlay.cpp` | 185 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-editor-preview.cpp` | 462 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-editor-select.cpp` | 408 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-editor.cpp` | 83 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-element-render.cpp` | 400 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/gui-layout.cpp` | 694 | callback,file-io | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; static check: transient per-call handles; no retained resource |
| `src/gui/gui-main.cpp` | 1838 | gpu-resource,network-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_TRANSPORT: close/drain; session owner quiesce; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/gui/gui-media.cpp` | 156 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/gui/hud/chat-window.cpp` | 601 | callback,gpu-resource,network-context | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/gui/hud/healthbar-config.cpp` | 166 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gui/hud/reward-popup.cpp` | 197 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/gui/menus/account-panel.cpp` | 160 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/debug-menu.cpp` | 40 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/duel-config-menu.cpp` | 93 | callback,gpu-resource | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current |
| `src/gui/menus/help-menu.cpp` | 59 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/login-menu.cpp` | 204 | gpu-resource,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_RESOURCES or resident owner; close handle |
| `src/gui/menus/main-menu.cpp` | 234 | gpu-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/gui/menus/menu-avatar-preview.cpp` | 394 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/gui/menus/online-menu.cpp` | 562 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/pause-menu.cpp` | 282 | gpu-resource,os-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_RESOURCES or resident owner; close handle |
| `src/gui/menus/play-menu.cpp` | 89 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/practice-menu.cpp` | 39 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/sandbox-map-menu.cpp` | 141 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/server-info-menu.cpp` | 142 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/settings-menu.cpp` | 459 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/menus/sign-in-menu.cpp` | 130 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/password-popup.cpp` | 138 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/ui-system-buttons.cpp` | 366 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/ui-system-layout.cpp` | 75 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/ui-system-render.cpp` | 533 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/ui-system.cpp` | 279 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/gui/ui-text-input.cpp` | 545 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/impact/destructible-async-cut.cpp` | 206 | async-worker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload |
| `src/impact/destructible-render.cpp` | 154 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/impact/destructible-world-config.cpp` | 228 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/input/input-commands-bind.cpp` | 154 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/input/input-commands.cpp` | 473 | callback,gpu-resource | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current |
| `src/input/input-poll.cpp` | 568 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/input/mouse-lock.cpp` | 56 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/input/scenario-input.cpp` | 73 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/interaction/interaction-system.cpp` | 483 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/leaderboard/leaderboard-ui.cpp` | 140 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map-authored-geometry.cpp` | 1611 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/map/map-loader-gltf-extract.cpp` | 252 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map-loader-gltf-mesh.cpp` | 29 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map-loader-gltf-nodes.cpp` | 261 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map-loader-gltf-prims.cpp` | 258 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map-loader-material.cpp` | 297 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map_loader.cpp` | 506 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/map_render.cpp` | 67 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/texture.cpp` | 87 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/map/texture_manager.cpp` | 62 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/mapeditor/map-editor-config.cpp` | 154 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/mapeditor/map-editor-projects.cpp` | 334 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/mapeditor/map-editor.cpp` | 5146 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/network/avatar-network-client.cpp` | 464 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/badconn/badconn-config.cpp` | 410 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/network/badconn/badconn.cpp` | 458 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/client.cpp` | 497 | gpu-resource,network-resource,network-context | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/community-server-config.cpp` | 228 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/network/confirmed-damage-presentation.cpp` | 262 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/disagreement-visuals.cpp` | 573 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/network/multiplayer-chat.cpp` | 18 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-context.cpp` | 129 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-interpolation.cpp` | 2510 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-packets.cpp` | 2309 | async-worker,network-resource,network-context | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-physical-entities.cpp` | 339 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-projectiles.cpp` | 2762 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-reconcile.cpp` | 514 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-shots.cpp` | 928 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/multiplayer-tick.cpp` | 3286 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/network-weapons.cpp` | 202 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/packets.cpp` | 22 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/physical-entity-replication.cpp` | 244 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/reconnect-visuals.cpp` | 197 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/reliable-gameplay-events.cpp` | 388 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-attack.cpp` | 1594 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-avatar-network.cpp` | 333 | network-resource | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce |
| `src/network/server-body-template.cpp` | 307 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/network/server-browser.cpp` | 225 | async-worker,network-resource,network-context,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload; explicit owner quiesce plan; may stay resident |
| `src/network/server-damage.cpp` | 663 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-gamemode.cpp` | 10894 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-melee-actions.cpp` | 1398 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-npcs.cpp` | 3087 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-packet-chat.cpp` | 1628 | network-resource,file-io,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-packet-handlers.cpp` | 1857 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-packets.cpp` | 3442 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-physical-contact-actors.cpp` | 973 | network-resource,file-io,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-physical-contact.cpp` | 636 | network-resource | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce |
| `src/network/server-players.cpp` | 2280 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-presets.cpp` | 95 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/network/server-projectiles.cpp` | 2187 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/server-world.cpp` | 577 | callback,file-io | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; static check: transient per-call handles; no retained resource |
| `src/network/snapshot-chunks.cpp` | 639 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/udp-echo.cpp` | 177 | network-resource,file-io,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/vehicle-networking.cpp` | 1175 | callback,network-resource,network-context | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/network/weapon-runtime-reconciliation.cpp` | 239 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/notifications/notifications.cpp` | 1031 | callback,gpu-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/npc/npc-avatar.cpp` | 198 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/npc/npc-behavior.cpp` | 721 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/npc/npc-difficulty-config.cpp` | 411 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/perf/perf-frame.cpp` | 968 | async-worker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload |
| `src/perf/perf-gpu.cpp` | 170 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/perf/perf-visualize.cpp` | 184 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/perf/perf.cpp` | 1710 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/persistence/persistence-emit.cpp` | 179 | network-resource,network-context | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/physics/physical-entity.cpp` | 2338 | callback,gpu-resource | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; CAP_RESOURCES: release with the context current |
| `src/physics/physics-debug-movement.cpp` | 61 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/physics/physics-mini.cpp` | 564 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/procedural/infinite-world-runtime.cpp` | 1898 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/procedural/procedural-world.cpp` | 837 | network-resource,file-io | OWNED_PERSISTENT | CAP_TRANSPORT: close/drain; session owner quiesce; static check: transient per-call handles; no retained resource |
| `src/profile/local-profile-system.cpp` | 114 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/ragdoll/ragdoll-mode-config.cpp` | 369 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/render/dynamic-light-config.cpp` | 160 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/render/lighting-config.cpp` | 135 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/render/post-fx.cpp` | 671 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/render/procedural-cylinder.cpp` | 216 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/render/render-player.cpp` | 1191 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/render/render-world-mesh.cpp` | 508 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/render/render-world-shadow.cpp` | 71 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/render/render-world-sky.cpp` | 102 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/render/render-world.cpp` | 489 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/render/skybox.cpp` | 439 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/replay/replay-camera.cpp` | 200 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-capture-commands.cpp` | 311 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-commands-export.cpp` | 597 | file-io,os-resource | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/replay/replay-editor-commands.cpp` | 1728 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-editor.cpp` | 1031 | audio-resource,file-io | OWNED_PERSISTENT | resident audio owner; stop voice/engine before unload; static check: transient per-call handles; no retained resource |
| `src/replay/replay-export-ffmpeg.cpp` | 824 | async-worker,audio-resource,file-io,os-resource | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; resident audio owner; stop voice/engine before unload; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle |
| `src/replay/replay-export-json.cpp` | 511 | async-worker,file-io,os-resource,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle; explicit owner quiesce plan; may stay resident |
| `src/replay/replay-export-mf.cpp` | 1255 | async-worker,file-io | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; static check: transient per-call handles; no retained resource |
| `src/replay/replay-export-subprocess.cpp` | 472 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-export-target.cpp` | 69 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/replay/replay-export.cpp` | 715 | async-worker,gpu-resource,file-io,os-resource,known-blocker | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource; CAP_RESOURCES or resident owner; close handle; explicit owner quiesce plan; may stay resident |
| `src/replay/replay-factory-clips.cpp` | 202 | async-worker,file-io | OWNED_PERSISTENT | CAP_JOBS: submit only; cancel/drain/join before unload; static check: transient per-call handles; no retained resource |
| `src/replay/replay-factory.cpp` | 199 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-io-save.cpp` | 229 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-player-load.cpp` | 253 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-player.cpp` | 372 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/replay/replay-recorder.cpp` | 635 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/shadow/shadow-config.cpp` | 106 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/shadow/shadow-render.cpp` | 462 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/terminal/actor-commands.cpp` | 401 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/debug-commands.cpp` | 944 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/map-entity-commands.cpp` | 267 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/network-commands.cpp` | 1249 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/terminal/npc-commands.cpp` | 219 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/terminal/object-commands.cpp` | 214 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/player-commands.cpp` | 643 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/terminal/procedural-world-commands.cpp` | 142 | network-context | OWNED_PERSISTENT | CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/terminal/replay-commands.cpp` | 430 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/terminal-cursor-commands.cpp` | 40 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/terminal/weapon-bench-commands.cpp` | 420 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/terminal/weapon-commands.cpp` | 1666 | file-io,network-context | OWNED_PERSISTENT | static check: transient per-call handles; no retained resource; CAP_TRANSPORT/CAP_SESSION: bounded payloads; drain before unload |
| `src/utils/pak-file.cpp` | 188 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/utils/path_utils.cpp` | 273 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/utils/tips.cpp` | 103 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/vehicles/vehicle-config.cpp` | 724 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/video/outro-ffmpeg.cpp` | 328 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/video/outro.cpp` | 304 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/video/video-settings.cpp` | 193 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/void-death/void-death-commands.cpp` | 65 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/void-death/void-death.cpp` | 107 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/world/streamed-world-layer.cpp` | 1150 | gpu-resource | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current |
| `src/world/texture-store.cpp` | 65 | gpu-resource,file-io | OWNED_PERSISTENT | CAP_RESOURCES: release with the context current; static check: transient per-call handles; no retained resource |
| `src/world/world-gltf-loader.cpp` | 515 | callback,file-io | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload; static check: transient per-call handles; no retained resource |
| `src/worldgen/worldgen-library.cpp` | 650 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/worldproject/world-edit-session.cpp` | 488 | callback | OWNED_PERSISTENT | CAP_CALLBACKS: generation token; unregister before unload |
| `src/worldproject/world-project-sha256.cpp` | 181 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |
| `src/worldproject/world-project-store.cpp` | 1005 | file-io | TRANSIENT_CANDIDATE | static check: transient per-call handles; no retained resource |

## Exit criteria for Pass A

- Every row has a traced owner or is marked `TRANSIENT_CANDIDATE` after a read.
- Every `OWNED_PERSISTENT` row cites its stop/join/release path and the barrier rule.
- No row relies on a per-file runtime test; the barrier rule is the mechanism.
- The remaining `RUNTIME_PROOF_REQUIRED` set is only rows with a live owner that the barrier cannot yet cover.

## What this pass does not prove

- It does not prove quiescence. That is the barrier campaign.
- It does not move any resource into a capability; it only names the rule.
