2026 10 10 1800 jorj todo - need to make size limit bc we keephaving std alloc memory error 

# Implementation plan: one canonical `events.jsonl` log path

Status: planning only. This section records the intended migration before any
source or configuration changes are made. Contributors must update the
inventory and obtain review before deleting or redirecting a logging path.

## Goal and non-negotiable result

MiMITA will have one project-owned diagnostic output path:

```text
logs/<yyyy-mm-dd>/<yyyymmdd_hhmmss>/events.jsonl
```

All in-process diagnostic records from the client, dedicated server, tools,
and debug subsystems will be structured records written by `StructuredLogger`.
There will be no category `.txt` files, summary files, legacy `LogManager`
files, feature-specific debug files, `printf`/`fprintf` diagnostic sinks, or
parallel JSONL journals under `logs/`. Terminal display may remain as a
presentation of the same structured record, but it is not a second file sink.

The final storage invariant is absolute, not best-effort:

1. The complete `logs` directory tree must never exceed exactly
   `1,000,000,000` bytes of regular-file content.
2. No individual log file may exceed exactly `100,000,000` bytes.
3. The quota must hold while client and server processes write concurrently,
   not only at startup or shutdown.
4. A write that would violate either limit must be rejected, summarized, or
   cause an older completed run to be removed before the write. It must never
   write first and clean up afterward.
5. The active run and files still open by a live process must not be deleted.
6. If no deletable bytes remain, low-priority records are dropped or
   aggregated and the drop is itself reported through the surviving canonical
   stream when space becomes available. The logger must not bypass the quota
   for errors, crashes, or assertions.

The exact byte units are decimal bytes, not MiB/GiB. The implementation may
reserve internal headroom below 1,000,000,000 bytes, but the specification's
observable ceiling remains 1,000,000,000 bytes.

## Proposed ownership and migration order

### Phase 1: establish the canonical writer contract

Keep `StructuredLogger` as the only logging owner. The primary implementation
points are:

- `src/debug/structured-log.cpp:221-340` — extend the parsed logging policy
  with the file and folder byte limits, rotation state, quota state, and
  drop/summarization policy. The current parser does not read retention or
  byte-limit settings.
- `src/debug/structured-log.cpp:352-410` — keep one run directory and one
  canonical path. Resolve the shared `MIMITA_EVENTS_FILE` path before opening
  it, and make all participating processes agree on the same run identity.
- `src/debug/structured-log.cpp:654-707` — initialize the canonical writer,
  acquire the shared quota/rotation coordination object, and perform safe
  startup accounting without deleting an active run.
- `src/debug/structured-log.cpp:896-961` — make serialization, quota
  reservation, rotation, and the actual write one coordinated operation. The
  current `writeJsonLine()` appends and flushes but performs no size check.
- `src/debug/structured-log.cpp:709-730` and `1002-1010` — close/complete a
  run and publish bounded drop/rotation summaries without creating another
  file.
- `src/debug/structured-log.h:140-274` — expose only the canonical logger
  contract; do not add another logger API for individual gameplay systems.

The concurrent client/server case must use one cross-process Windows mutex or
equivalent named coordination primitive around: current-size accounting,
deletion, file rotation, reservation, write, and close/rename. A per-process
mutex is insufficient because the current shared stream is written by more
than one process.

### Phase 2: define safe rotation and folder-quota behavior

The preferred layout is a run directory containing numbered JSONL segments,
for example `events-0001.jsonl`, `events-0002.jsonl`, and a current segment.
Rotation occurs before the next complete JSON record would make the current
segment exceed 100,000,000 bytes. No record may be split or truncated.

The canonical reader and `log_open` command must understand all segments as
one chronological run. `src/devtools/dev-log-commands.cpp:30-43` currently
reports one `events.jsonl` path and must be updated only after the segment
contract is approved. `docs/specs/debug-logging/canonical-jsonl.md` and the
runtime-validation workflow must describe the same reader behavior.

Before creating a segment or accepting a write, the quota owner must:

1. enumerate regular files under `logs`;
2. exclude the current active segment and files owned by live runs;
3. delete the oldest completed eligible runs/segments until the incoming
   reservation fits below 1,000,000,000 bytes;
4. refuse or summarize the record if it still does not fit; and
5. re-check the result after the write for evidence and invariant reporting.

This is intentionally not a background watcher. A watcher can only discover a
violation after it has happened and can race with open writers. A hard ceiling
requires every writer to reserve bytes through the same owner before writing.

Crash and assertion records must use the same quota. The earlier idea of
keeping crash files forever is incompatible with an absolute folder-wide cap
and must be removed or explicitly rejected during review.

### Phase 3: remove parallel file writers

Each writer below must either be deleted when redundant or migrated to a
structured `StructuredLogger::writeEvent()` call. No replacement may open a
file under `logs` directly.

| Current path | Current owner and lines | Required disposition |
|---|---|---|
| `events.jsonl` append | `src/debug/structured-log.cpp:935-961` | Retain as the only sink, but add coordinated rotation/quota enforcement. |
| Category `.txt` files and summary | `src/debug/structured-log.cpp:433-510`, `587-649` | Delete as file sinks; category remains a JSON field. |
| Legacy stdout log | `src/debug/log-manager.cpp:60-120`, `251-346` | Remove as a file logger; route diagnostic output to the canonical logger. |
| Crosshair debug file | `src/crosshair/crosshair-config.cpp:24-36` | Convert to a GUI/crosshair structured event or delete if redundant. |
| NPC killfeed file | `src/debug/npckillfeed-log.cpp:37-70` | Convert to structured events; remove `tempdebuglogs`. |
| Spawn debug file | `src/game/spawn-utils.cpp:12-25`; `src/combat/death-system.cpp:354-359` | Consolidate into structured spawn/respawn events. |
| Weapon-spawn debug file | `src/combat/weapon-runtime.cpp:135-147` | Convert to structured weapon lifecycle events. |
| SpyKnife file | `src/combat/weapon-spyknife.cpp:61-94` | Convert to structured weapon/contact events. |
| Godball file | `src/combat/weapon-godball-movement.cpp:38-99` | Convert to structured weapon/damage events. |
| Performance text files | `src/perf/perf.cpp:562-566`, `639-654`; `src/perf/perf-commands.cpp:230` | Remove deprecated file paths; retain bounded performance JSONL summaries. |
| Weapon benchmark files | `src/terminal/weapon-bench-commands.cpp:25-36`, `79-120`, `166` | Write benchmark results as structured events or place them outside `logs` as explicit user artifacts. They must not silently count as diagnostic logs. |
| Legacy test/shared journals | `devscripts/dev-loop.py:155-165`, `1103-1129`, `1226-1232` and test helpers | Keep one canonical run path; remove any alternate journal creation. |

#### Complete direct-file and process-output sink inventory

The table above names the main `logs` writers. These additional diagnostic or
process-output sinks are also outside the canonical `events.jsonl` path and
must receive an explicit disposition. Ordinary configuration, save-game,
replay, asset, and user-export files are not logging sinks merely because they
use `fopen` or `ofstream`; classify them rather than silently sweeping them
into this migration.

| Exact location | Current output | Required disposition |
|---|---|---|
| `src/debug/debug-diag-commands.cpp:240-253` | `diag-log.txt` snapshot of the diagnostic ring | Replace with a bounded `events.jsonl` export/event or remove the file command. |
| `src/replay/replay-commands-export.cpp:214-233`, `236-306` | `replays/exports/export-debug.log` | Keep only if classified as a user export artifact; otherwise migrate to `events.jsonl`. |
| `src/replay/replay-export-ffmpeg.cpp:618-629`, `706-708` | `replays/exports/_tmp/ffmpeg-export.log` | Classify as an external-tool artifact or redirect its diagnostic summary to `events.jsonl`. |
| `src/debug/crash-handler.cpp:313-327`, `375-426`, `429-478`, `756-764` | Crash reports and minidumps under `%LOCALAPPDATA%\MiMITA\crashes` or the `crashes` fallback | Decide whether crash artifacts are allowed outside `logs`; their summary and identity must still reach `events.jsonl`. |
| `src/debug/log-manager.cpp:67-76`, `192-204`, `251-291`, `295-346` | Legacy log file, stdout pipe capture, console mirroring, and `latest-log-path.txt` | Remove as a second logger; console presentation must consume canonical records. |
| `src/debug/structured-log.cpp:433-529`, `587-649` | Per-category `.txt` files and `Summary_*.txt` | Delete these sinks; retain category/summary information as JSON events. |
| `src/debug/structured-log.cpp:985-995` | Throttled lines sent to `LogManager::write()`/`writeConsole()` | Route the same record through the canonical writer; console output is presentation only. |
| `devscripts/dev-loop.py:801-813` | Build subprocess stdout/stderr pipe printed by the launcher | Classify as build-console UI or capture bounded build events; it is not game logging. |
| `devscripts/dev-loop.py:1153-1169` | Server launched with `CREATE_NEW_CONSOLE` | Remove the separate server console after startup/readiness is observable in `events.jsonl` and launcher UI. |
| `devscripts/dev-loop.py:1242-1258` | Client launched with `CREATE_NEW_CONSOLE` | Remove the separate logging console after readiness/status is observable canonically. |
| `src/gui/gui-main.cpp:939-950` | GUI-launched server with visible CMD window and raw launch messages | Replace raw launch prints with structured launch events and remove the visible CMD window when proven safe. |
| `src/replay/replay-export-ffmpeg.cpp:560`; `src/replay/replay-commands-export.cpp:62-107`, `138-208` | Explicit visible FFmpeg/CMD debug windows | Keep only as intentional external-tool debug mode; record mode/process identity in `events.jsonl`. |
| `src/terminal/weapon-bench-commands.cpp:25-36`, `79-120`, `164-166` | Benchmark text files and `latest-weapon-bench-path.txt` | Convert results to structured events or explicitly classify them as user artifacts outside `logs`. |

The exact search ledger below covers call sites that produce terminal or
stdout/stderr output. Every entry must be classified, even if currently
considered harmless. “It only prints to a terminal” is not an automatic
exception: the target is canonical events plus intentional UI text, not an
independent diagnostic stream.

This inventory is a starting ledger, not permission to delete. Before each
migration slice, search the whole repository for `logs`, `events.jsonl`,
`fopen`, `ofstream`, `CreateFile`, `printf`, `fprintf`, `fputs`, `vfprintf`,
`std::cout`, `LogManager`, and `StructuredLogger`. Record every result with
the exact file and current line number, classify it as canonical logging,
user-facing terminal output, non-log artifact output, third-party code, or
obsolete diagnostic output, and give it a disposition. Re-run the search after
each slice so no direct writer is silently left behind.

### Phase 4: eliminate raw diagnostic output

Raw `printf`, `fprintf`, `fputs`, and direct stdout/stderr writes must not be
used as a parallel diagnostic system. Migrate diagnostic messages to
`StructuredLogger` with category, level, event name, reason, source location,
and relevant fields. Keep deliberate command-line/user-facing text only when
it is proven not to be a diagnostic log; record those exceptions in the
inventory. Do not replace a raw print with another hidden file writer.

Known raw-output families requiring review include:

- `src/perf/perf.cpp:624-635`, `1238-1293`;
- `src/replay/replay-commands-playback.cpp:37-49`, `150-269`;
- `src/replay/replay-capture-commands.cpp:50-82`, `148-187`, `274-304`;
- `src/replay/replay-commands-export.cpp:336-384`;
- `src/replay/replay-export-ffmpeg.cpp:61`, `138-255`, `485-543`,
  `678-735`;
- all remaining repository matches found by the required search ledger.

### Raw C/C++/Python-style print-family output

Inventory snapshot: 2026-10-10. Each value is an exact matching source line; ranges contain only consecutive matches.

- `src\analytics\analytics-manager.cpp`: `243,256,272`
- `src\analytics\analytics-uploader.cpp`: `39,143`
- `src\audio\audio.cpp`: `165,172,181,213,233,238,279,660`
- `src\auth\auth-popup.cpp`: `54,88,148,159,170,181,302,307,331,479,484,508`
- `src\auth\auth-token.cpp`: `98,103,123,137,141,177,197,205,209,249,327,331,334,391,393,402`
- `src\avatar\avatar-atlas.cpp`: `417,425,432,452,513,524,527,536,539,570,600,608`
- `src\avatar\avatar-commands.cpp`: `246-247,278,292,297,303,305,310,316,319,326,342-345,350,355,369,376,381,389,395,397,416,421`
- `src\avatar\avatar-drop-target.cpp`: `155,162,164`
- `src\avatar\avatar-fileops.cpp`: `81,84`
- `src\avatar\avatar.cpp`: `509-510,518,541,546,564,586,590,597,601,620,639,642,644,648,654,656-657,666,673,701,1040-1041,1043,1045-1046,1059,1066,1077,1166,1177,1179,1184,1189,1194,1200,1202,1204,1213,1224,1227`
- `src\avatar\character-manifest.cpp`: `33,89`
- `src\avatar\character-registry.cpp`: `22,39,46,53,60,66`
- `src\avatar\cosmetic-system.cpp`: `74,82`
- `src\combat\death-system.cpp`: `180,356`
- `src\combat\projectile-render.cpp`: `312`
- `src\combat\revolver-system.cpp`: `120,152`
- `src\combat\shot-profiler.cpp`: `11-13,55-59,61,65,74,84,89,101-107,112-114,117,119,122-124,126,131,149`
- `src\combat\weapon-audio.cpp`: `41,47,53,59,80,100`
- `src\combat\weapon-config.cpp`: `65,86,89,101,143`
- `src\combat\weapon-data.cpp`: `41,56,81,129,140,155,198`
- `src\combat\weapon-fire-damage.cpp`: `270,804`
- `src\combat\weapon-fire-hit.cpp`: `135,151,403,470`
- `src\combat\weapon-fire-raycast.cpp`: `173`
- `src\combat\weapon-godball-movement.cpp`: `66-70,79-81,98,232,393,453,467`
- `src\combat\weapon-godball.cpp`: `102,113,150`
- `src\combat\weapon-hafs.cpp`: `295`
- `src\combat\weapon-registry.cpp`: `25,29,32`
- `src\combat\weapon-rocket-launcher.cpp`: `513`
- `src\combat\weapon-runtime.cpp`: `50,93,111,118,128,143,173-174,176`
- `src\combat\weapon-spyknife.cpp`: `92`
- `src\combat\weapon-swordsword.cpp`: `208,253,356,646`
- `src\combat\weapon-system-equip.cpp`: `109`
- `src\combat\weapon-system.cpp`: `80,160,170,578,584,587,1303`
- `src\combat\weapon-viewmodel.cpp`: `113,398,414,516`
- `src\competitive\competitive-match.cpp`: `23,59,65`
- `src\competitive\competitive-ui.cpp`: `95`
- `src\competitive\competitive.cpp`: `130,170,233,241-250,252`
- `src\config\collision-lod-config.cpp`: `38,61,67`
- `src\config\player-settings.cpp`: `76,130`
- `src\crosshair\crosshair-config.cpp`: `33-34`
- `src\debug\debug-diag-commands.cpp`: `248`
- `src\debug\debug-diag.cpp`: `84,159,161,164,168,177`
- `src\debug\debug-visuals-config.cpp`: `16,25,29,45`
- `src\debug\debug-visuals.cpp`: `61,73,75`
- `src\debug\gl-debug.cpp`: `41,47,56,64,74`
- `src\debug\log-manager.cpp`: `72,113,115,125,134,141,158,224,239,254,291,301,341-346`
- `src\debug\npckillfeed-log.cpp`: `48,64,69`
- `src\debug\structured-log.cpp`: `385,393,404,408,514-518,525,537-541,574,580,594-598,631-635,663,982,984`
- `src\debug\transform-debug.cpp`: `34,43`
- `src\debug\validate-assets.cpp`: `98-100,110,120,123,125-126,131-134`
- `src\devtools\account-config.cpp`: `126,143,163,167,172,203,208,237,262,266,302,305`
- `src\devtools\dev-commands.cpp`: `11,14,20,25,31`
- `src\devtools\dev-config.cpp`: `78,92,111,121,124`
- `src\devtools\dev-log-commands.cpp`: `24-25,141,162,169`
- `src\devtools\dev-npc-selection.cpp`: `15,25,42,48`
- `src\devtools\dev-overlay.cpp`: `15,24`
- `src\devtools\dev-teleport.cpp`: `24,43,51,56,65,71,83,94,99,111`
- `src\devtools\npc-spawn-commands.cpp`: `86,156`
- `src\devtools\terminal.cpp`: `516,518,839`
- `src\effects\effect-part-blood.cpp`: `382`
- `src\effects\effect-part-render.cpp`: `728,739`
- `src\effects\effect-part-system.cpp`: `42`
- `src\engine\engine-init.cpp`: `31,35-36,42`
- `src\engine\engine-tick-camera.cpp`: `786,818,849,864,1081`
- `src\engine\engine-tick-combat.cpp`: `479`
- `src\engine\engine-tick-net.cpp`: `203,469,480,487,519,557,563,770,780,832,848,940,952`
- `src\engine\engine-tick-render.cpp`: `197,611,629,635`
- `src\engine\engine-tick-replay.cpp`: `927,932`
- `src\engine\engine-tick-setup.cpp`: `80,195`
- `src\engine\engine-tick-state.cpp`: `105,124,130,156,166,172,185,189,191,194,205,213`
- `src\engine\engine-tick-ui-game-hud.cpp`: `487,517`
- `src\engine\engine-tick.cpp`: `193,198,203`
- `src\entities\player-animation-config.cpp`: `129,145,182`
- `src\entities\player-animation.cpp`: `256,265,639,643,651,665,668,671,682-683,685,698,702,706`
- `src\entities\player-config.cpp`: `142,147,180`
- `src\entities\player-loader.cpp`: `243,287,570,583-584,587,614,617,667,671,677,681,697,709,786-787,789,933`
- `src\entities\player-render-mesh.cpp`: `274`
- `src\entities\player-render.cpp`: `141,144,211,322-332,335-336,343-345,347`
- `src\entities\player.cpp`: `274,340,344,353,357`
- `src\game\game-cli.cpp`: `125-133,144,162,168,182,193,224,274-275,282,305,308,319-320,328,330,340,345,350,353,407,414,423,445-446,469,563-564,611,652,692-693,888-889,896-897,904-905,994-995,1115-1116,1123-1124,1261-1262,1358-1359,1398-1399,1494-1495,1617-1618,1730-1731,1832-1833,1921-1922,1929-1930,1937-1938,1945-1946,1953-1954,1961-1962,1969-1970,1977-1978,1985-1986,1993-1994,2001-2002,2009-2010,2017,2046,2063-2064,2081,2084,2091-2092,2101-2102,2110-2111,2113,2116,2118,2127,2130,2151,2155,2164,2167,2169,2172-2173,2175,2185,2205,2210,2217-2218,2225-2226,2233-2234,2241-2242,2249-2250,2257-2258,2265-2266,2273-2274,2294-2295,2302-2303,2310-2311,2317,2322,2328-2329,2336-2337,2341,2345-2346,2376-2381,2384,2388,2390,2392,2401,2406,2411-2412,2423,2430,2434,2436,2438,2442,2452-2453,2460,2464,2468,2476,2486-2487,2491,2506-2507,2512,2514,2517,2525,2530,2535,2540-2541,2551,2558,2563,2567,2570,2575,2579,2587,2597-2598,2606,2609,2613,2628-2629,2631,2683,2689,2695-2696,2699,2708,2730,2739,2745,2751-2752,2756,2807,2810,2814,2858,2868,2915,2950,2958,2964,2967,2995,2998,3018,3021,3029,3036,3039,3045,3053,3056-3057,3060,3062,3067,3070,3073,3077,3082,3085,3089,3092,3098,3101,3105,3109,3111,3113,3116,3118,3123,3125,3127,3129`
- `src\game\spawn-utils.cpp`: `25,37,41,69,82,89`
- `src\gui\font-stuff\font-loader.cpp`: `79,86,122,128,133,166,176-177,182,203,244,261,282-283,290,296,302,324`
- `src\gui\gui-editor-commands.cpp`: `22,129,144`
- `src\gui\gui-editor-overlay.cpp`: `92`
- `src\gui\gui-editor-select.cpp`: `112,165,179,300,329,356,372`
- `src\gui\gui-editor-tools.cpp`: `98`
- `src\gui\gui-layout.cpp`: `104,278,297,316,321,328,425,432,584,596,611,618,628,631,644,654`
- `src\gui\gui-main.cpp`: `227,232,251,299,304,313,318,328,332,345,362,369,499,521,536,574,582,595,688,694,702,711,716,721,741,945,948,957,1238`
- `src\gui\gui-media.cpp`: `57,94,104,129`
- `src\gui\menus\account-panel.cpp`: `140,152`
- `src\gui\menus\debug-menu.cpp`: `9,22,38`
- `src\gui\menus\main-menu.cpp`: `37,40,118,148,160`
- `src\gui\menus\menu-avatar-preview-debug.cpp`: `143,148,153,159,212-213`
- `src\gui\menus\menu-avatar-preview.cpp`: `89,99,120,125-126,142,150,298`
- `src\gui\menus\online-menu.cpp`: `230,300,359,392,453,466,475,512,521`
- `src\gui\menus\play-menu.cpp`: `50,59,64,69,74,79`
- `src\gui\menus\sandbox-map-menu.cpp`: `122`
- `src\gui\menus\server-info-menu.cpp`: `33,35`
- `src\gui\menus\settings-menu.cpp`: `376,443`
- `src\gui\ui-system-buttons.cpp`: `75,77,85`
- `src\gui\ui-system-render.cpp`: `100,111,165,167,269`
- `src\gui\ui-system.cpp`: `64-65,130,148,152`
- `src\hot-reload\hot-reload-system.cpp`: `14,66,70,73,87,94,111,119,128,151,170-171,175`
- `src\input\input-commands-bind.cpp`: `112,128,130,152`
- `src\input\input-commands.cpp`: `46,411`
- `src\input\input-poll.cpp`: `98`
- `src\main-init.cpp`: `176,183-186,188,190,234,237,240,244`
- `src\main-systems.cpp`: `222,256,262,266,281,284,287-289,293,508`
- `src\main.cpp`: `253,260-261,270,276,281,290,298,328`
- `src\map\map_loader.cpp`: `71,76,130,425,482`
- `src\map\map-catalog.cpp`: `60,66,72,101,118,127`
- `src\map\map-loader-collision.cpp`: `135,151,207,240,257,314,330`
- `src\map\map-loader-gltf-extract.cpp`: `85,93,99`
- `src\map\map-loader-gltf-prims.cpp`: `93,101,107`
- `src\map\map-loader-material.cpp`: `31,39,47,51,59,63,71,75,80,88,95,99,198,210,223,231,237,251,274,280,292,295`
- `src\map\texture_manager.cpp`: `15-16,31,34,37,42,45,48,54,57,60`
- `src\map\texture.cpp`: `29,49,56,77,84`
- `src\network\client.cpp`: `64,72,77,86,94,100,126,165,184,201,256,267,289,306,441,445,470`
- `src\network\confirmed-damage-presentation.cpp`: `154,161`
- `src\network\coordinator-client.cpp`: `215,244,254,259,276,286,290,306,323,325,341,348,368,371,385,397,408,413,449,479,483,507,510,530,546,552,577,588,591`
- `src\network\ice\ice-agent.cpp`: `172,362`
- `src\network\ice\ice-server.cpp`: `175,196,260,266,270-271,282,288,290-296,312-313,324,327,414,422,432,442-443,457,475,480,491,494,514,532,567,573,586,591,598,609,614,619,636,768,772,940`
- `src\network\ice\ice-test.cpp`: `57,199,202,206-207,224,250-251,279-280,282,286-287,299,302,309,324,354,363,384,390,399,422,433,456,480,488,499,502,505,522,527,531-532,534,540,542,545,547,552,554,570,574,587-588,590,606,609,623-624,631,634,640-641,645-646,648,694,701,710,719,725,736-737,750,754,756,758,760,774,809,820,850,903,913,927,931,934,936,951,953,966-967,970,976,1014,1017,1023,1029`
- `src\network\multiplayer-chat.cpp`: `15`
- `src\network\multiplayer-interpolation.cpp`: `713,758,767,956,1060,1718,1732,1746,1758,1771,1791`
- `src\network\multiplayer-packets.cpp`: `157,159,191,345,357,364,369,377,383,389,462,478,496,661,672,798,805,810,826,831,846,851,860,869,874,884,893,922,935,946,953,964,974,980,985,993,998,1013,1024,1034,1192,1201,1210,1240`
- `src\network\multiplayer-physical-entities.cpp`: `75,132,141,182,235,260,264`
- `src\network\multiplayer-projectiles.cpp`: `334,550,655,707,719,876,956,1017,1024,1106,1113,1146,1157,1173,1183,1257,1339,1565,1724,1735,1742,1753,2008,2024,2257,2263`
- `src\network\multiplayer-reconcile.cpp`: `44,62,196,298,313`
- `src\network\multiplayer-shots.cpp`: `110,134,170,216,232,268,274,360,384,391,399,435,536,567,592,617,638,665,733,764,774,805,826,850`
- `src\network\multiplayer-tick.cpp`: `166,213,282,296,371,472,580,619,665,701,864,869,951,967,1047,1075,1085,1140,1149,1164,1169,1219,1398,2072,2078,2177,2198,2226,2287,2348`
- `src\network\net_common.cpp`: `27,43,62`
- `src\network\net_mode.cpp`: `100-119,121`
- `src\network\physical-entity-replication.cpp`: `211`
- `src\network\server-attack.cpp`: `912`
- `src\network\server-ice.cpp`: `182,193,198,203,216,223,244,263,278,287,319,333,350,355,392,404,409,420,431,444,454,512,530,564`
- `src\network\server-melee.cpp`: `143,166,203,225,235,267,296,305,322,331,441`
- `src\network\server-npcs.cpp`: `101,180,342,493,708,753,1495`
- `src\network\server-packet-chat.cpp`: `90,281,304,495,597,757,815,830,842,862,894,903`
- `src\network\server-packet-handlers.cpp`: `75,86,132,223,231,311,339,364,376,387,399,408,450,510,517,526,538,547,561,569,574,702,779,828,837`
- `src\network\server-packets.cpp`: `67,868,916,924,1241,1255,1270,1276,1301,1323,1335,1356,1392,1411,1419,1424,1428,1441,1444,1446,1451,1463,1484,1569,1582,1591,1603,1684,1740,2081,2091,2097,2109,2120,2138,2200,2281,2308,2328,2336,2343,2422,2435,2497,2518,2552`
- `src\network\server-players.cpp`: `194,452,617,655,693,1419,1455`
- `src\network\server-projectiles.cpp`: `253,255,257,259,261,263,265,267,272,301,863,961,1008,1091,1097,1124,1311,1355,1368,1489,1648,1727`
- `src\network\server-world.cpp`: `198-199,241,269,316,355`
- `src\network\server.cpp`: `283,383,396,398,410,418,425,429-435,437,439,452,459,465,476,483,491,499,508,510,519,525-526,581,610,622,631,637,642,652,654,686,754,844,850,913,925,935,1035,1040,1061,1068,1088,1090,1097,1099,1114,1116,1184,1196,1214,1220,1226,1231,1235,1251,1269,1322,1407,1415,1441,1473,1510,1525,1537-1552`
- `src\network\test-events.cpp`: `49`
- `src\network\udp-echo.cpp`: `60,67,81,89,97,105,134,144,150,157,161`
- `src\network\weapon-runtime-reconciliation.cpp`: `37,45,67,74,90,97,104,125,179`
- `src\perf\perf-frame.cpp`: `260,282-283,290-296,299-311,321`
- `src\perf\perf-visualize.cpp`: `48-62,64-79,81-89,91-97,99,103-106,110-111,113`
- `src\perf\perf.cpp`: `624-627,629,635,1238,1242,1244,1251,1270-1273,1275,1277-1278,1284-1294`
- `src\persistence\persistence-commands.cpp`: `16,24-28,32,35`
- `src\procedural\procedural-world.cpp`: `626,634`
- `src\profile\local-profile-system.cpp`: `60,99,107`
- `src\render\dynamic-light-commands.cpp`: `53`
- `src\render\lighting-config.cpp`: `33,103,107,115,133`
- `src\render\post-fx.cpp`: `43,100,104,115-116,170,180,203,218,229,275,284,474,478`
- `src\render\postfx-commands.cpp`: `19`
- `src\render\render-player.cpp`: `84,86,101,116,135,142,147`
- `src\render\render-world-mesh.cpp`: `302`
- `src\render\render-world.cpp`: `36,45,93,106,176,180`
- `src\render\skybox.cpp`: `21`
- `src\renderer\renderer.cpp`: `33,37,51,72,74,82,87,119,121,149,162,201,204,208,225,231,236,248,253,266,280,294,314,373,397`
- `src\replay\replay-camera-commands.cpp`: `53,83,93`
- `src\replay\replay-capture-commands.cpp`: `50,55,57,70,78,82,148,153,155,168,176-177,180,182,187,274-275,277-278,280,300-302,304`
- `src\replay\replay-commands-export.cpp`: `231,336,343-344,346,359,369,373,376,379,384`
- `src\replay\replay-commands-playback.cpp`: `37,49,150,177,224,234,246,257,269`
- `src\replay\replay-export-ffmpeg.cpp`: `61,138-139,180,227,243,248,255,261,270,485,539-541,543,678-682,684-685,688,691,695`
- `src\replay\replay-export.cpp`: `130,310-315,319-321,324,327-328,408,415,513-515`
- `src\replay\replay-factory-clips.cpp`: `60,67,70,73,80,89,93,145,154,187`
- `src\replay\replay-factory-worker.cpp`: `57,89,91`
- `src\replay\replay-factory.cpp`: `173`
- `src\replay\replay-io-save.cpp`: `226`
- `src\replay\replay-player-interp.cpp`: `90`
- `src\replay\replay-player-load.cpp`: `31,35,40,43,79,196,223,228,236,242,251`
- `src\replay\replay-player.cpp`: `114,134,137,143,145,151,156,162,180,187,224`
- `src\replay\replay-recorder-clips.cpp`: `113`
- `src\replay\replay-recorder.cpp`: `192,574,581,587,599,606,611,613,621,633`
- `src\shadow\shadow-config.cpp`: `33,73,77,85,103-104`
- `src\shadow\shadow-render.cpp`: `101,124,170,208,216`
- `src\terminal\network-commands.cpp`: `675,689,703,821,825,828,838`
- `src\terminal\replay-commands.cpp`: `223,254,263,266,270,274,278,302,310,313,316,318,333-336,338,344,346,348`
- `src\terminal\weapon-bench-commands.cpp`: `21,59,93-105,118,128,177-180,182-187,196,236,275-279,281,297-298,301,306-310,312-319,322-325,341-342,364,378,392,402,412-416`
- `src\terminal\weapon-commands.cpp`: `1201,1374`
- `src\video\frame-pacer.cpp`: `91,96,138,145`
- `src\video\video-settings.cpp`: `46,50,58,66,73,85,100,116,120,125,149,153`
- `src\world\world-gltf-loader.cpp`: `127-139,141,144,148,152,157,162,203,213,217,266,287,290,307`
- `tools\manifold_probe.cpp`: `60,69,76,89,102,104,111,124,127,147,152`

### Python launcher/tool/test `print()` output

Inventory snapshot: 2026-10-10. These are exact matching lines in
`devscripts`, `tools`, and `tests`; they are not `events.jsonl` writes.

- `devscripts/config_selftest.py`: `233-236,239,241,243`
- `devscripts/dev-loop.py`: `118,201,209,492,644,734,795,797,799,812,831,839,860-861,872,874,970,1037,1039,1042,1056,1061,1073,1076,1083,1090,1143,1145,1147,1151,1153-1154,1172,1174,1187,1205,1217-1218,1242-1243,1314,1342,1345,1349,1360-1362,1415,1434,1437,1439,1443,1451,1469,1479,1491-1496,1520,1523,1537,1542,1544-1548,1555,1569,1573,1575,1577`
- `tools/build_manifold.py`: `72,87,91,96,104`
- `tools/check-debug-logging.py`: `59-60`
- `tools/compress-8mb-v1-mimita.py`: `27,41-43,66,69,73`
- `tools/fonts/generate-atlas.py`: `143,145-146`
- `tools/network_smoke_build.py`: `81-82`
- `tools/test-ice-multiplayer.py`: `55,59,62,148,155,164,183,200,216,220,231,247,596,602-604,608,645,647,650,718,773,784-785,789,793,797,801-802,806,810,814,818,822,825`
- `tools/test-networking.py`: `29,42,44,59,65,73,83,85,103,115,117,155,157,186,196,206,210,212`
- `tools/test-udp-echo.py`: `109,114,143,176-178`
- `tools/test-udp-multiplayer.py`: `124,129,134-135,165,223-224,226-227,233`

Each line must be classified as launcher UI, build/test output, or diagnostic
output. Game/server diagnostic output must move to `events.jsonl`; standalone
tool/test harness output may remain only when it is outside the game logging
contract and is documented as such.

### Terminal and in-game scrollback output (`addLog`/`addLogf`)

Inventory snapshot: 2026-10-10. Each value is an exact matching source line; ranges contain only consecutive matches.

- `src\analytics\analytics-manager.cpp`: `288,300,311,323`
- `src\audio\hitmarker-audio.cpp`: `204,213`
- `src\audio\music-commands.cpp`: `44,57,69,84,93,107,119,124,133,142,150,160-161,171,184`
- `src\avatar\avatar-atlas.cpp`: `453,572`
- `src\avatar\avatar-commands.cpp`: `20,26,39,42,44,56,63,77,80,82,94,100,102,114,130,133,137,152,168,173,187,190,193,195,197,199,201,203,231,240-241,244,330,403,423`
- `src\avatar\avatar-editor.cpp`: `367`
- `src\avatar\avatar-fileops.cpp`: `85,101,136,172,184,249`
- `src\avatar\avatar.cpp`: `517,520,529,549,602,645,668,685,700,930,962,1047,1067,1195,1214`
- `src\camera\camera-commands.cpp`: `29,35,53,56,64,69,77,81,89,93,102,113,120`
- `src\combat\revolver-system.cpp`: `366,378`
- `src\combat\weapon-config.cpp`: `98,102,144`
- `src\combat\weapon-fire-effects.cpp`: `115,127`
- `src\combat\weapon-godball-movement.cpp`: `546`
- `src\combat\weapon-manager.cpp`: `54,58,65,110`
- `src\combat\weapon-system.cpp`: `118,766,772,1241,1243,1246`
- `src\competitive\competitive-commands.cpp`: `15,27,39,50,63,74,86,105,108,121,129`
- `src\crosshair\crosshair-commands.cpp`: `12,23,39,60,74,85,92,100`
- `src\debug\debug-diag-commands.cpp`: `62,67,69,71,73,75,77-80,88,90,98,100,106,120,126,130,133,139,142,144,150,154,157,163,167,169,175-176,182,188-189,194,196,207,222,225,230,237,243,253,258-260,278,285,330,350,373,394,397,405`
- `src\debug\transform-debug-commands.cpp`: `17,27,31,38,43,49,56`
- `src\devtools\dev-log-commands.cpp`: `23,35,43,140,147,170`
- `src\devtools\dev-overlay-commands.cpp`: `37,45,54,64,74`
- `src\devtools\terminal-builtins.cpp`: `28,38,53,56,58,77,80,82,84,86,95,104`
- `src\devtools\terminal-input.cpp`: `123,127,130,134,136`
- `src\devtools\terminal.cpp`: `134,182,187-188,190,193,196,235,292,297,308,313,323,333,352,364,367,379,387,395,403,406,418,422,434,441,450-451,461-462,474,476,487,492,495,558,650,666,704,730,738,749,796,815-816,825,838,857,877,900`
- `src\devtools\terminal.h`: `77`
- `src\effects\hitfx-commands.cpp`: `21,31,57,60,82,95,104,137,175,189,199,208,216,227,234,245,251,263,267,277,281,291,295,305,310,318`
- `src\engine\engine-tick-camera.cpp`: `341,360,393,430,515,543,572,599,601`
- `src\engine\engine-tick-combat.cpp`: `257,480`
- `src\engine\engine-tick-replay.cpp`: `916,936,940`
- `src\engine\engine-tick-setup.cpp`: `196`
- `src\engine\engine-tick-state.cpp`: `357`
- `src\engine\engine-tick-ui-replay-hud.cpp`: `317`
- `src\engine\engine-tick.cpp`: `311`
- `src\entities\aim-commands.cpp`: `27,29,57,71,85,104`
- `src\gui\gui-editor-commands.cpp`: `23,34,42-43,47,49,57,61,63,71-72,76,85,91,97,110,117,130,145,154,173,176-177,190,203`
- `src\gui\gui-main.cpp`: `1212,1219`
- `src\main-systems.cpp`: `317,471,473,494,509,520,525,530,540,562`
- `src\notifications\notification-commands.cpp`: `27,34,49,56,71,77,85,113,122,132,144,155,162`
- `src\perf\perf-commands.cpp`: `30,44,59,74,90,105,120,132,142,152,166,181,193,207,222,237,252,268,282,297,312,324,337,350,362,374,385,397,410,423`
- `src\ragdoll\ragdoll-commands.cpp`: `19,22,37,42,64,77,85,90,113,122,135,138`
- `src\render\dynamic-light-commands.cpp`: `22,25,31,38,40,54`
- `src\render\lighting-commands.cpp`: `13,15,22,27,39`
- `src\render\postfx-commands.cpp`: `20,27,31,38,46-49,54,57,63,65,67,70,79,84,87`
- `src\render\render-world.cpp`: `94,115,119,122,124,126,129,190,194,196,200,204,211,218,222,231,235,237,243,247,256,260,263,267,272,276,279,284,293,298,302,310,321,327,331`
- `src\replay\replay-camera-commands.cpp`: `18,27,36,52,61,68,70,82,92,102,110,115,128,137,139,148,150`
- `src\replay\replay-capture-commands.cpp`: `29,35,41,90,97-98,121,126,144,163,171,197,217,247,283-285,297`
- `src\replay\replay-commands-export.cpp`: `47,54,71,85,89,99,179,190,197-198,206,224,244,255,265,290,307,315,321,325,327,368,381,383,395,408,410,412,422,426,428,430,444,450,499,504,514,532,556,560,569,574,577,580,583,586,588,592,594`
- `src\replay\replay-commands-playback.cpp`: `21,32,34,36,50,59,65,76,84,93,107,115,124,132,140,151,162,173,179,190,201,212,235,247,258,270`
- `src\replay\replay-editor-commands.cpp`: `27,56-59,64,78,85,89,92,98,103,111,122-124,136,141,148,156-157,175,180,183,196,207,217,224,227,252,275,279,288,291,296,307,312,326-327,345,353,371,375,392,404,414,429,442,454,465,472,483,490,501,510,526,563,578,593,608,623,673,685,693,697,704,708,715,728,733,741,756,765,780,789,797,807,819,826,838,841,843,854,865,868,877,888,893,910,922,928,939,942,944,955,976,996,1008,1013,1024,1027,1029,1041,1044,1057,1059,1077,1090,1092,1102,1147,1265,1277-1281,1293,1299,1311,1323,1329,1337,1351,1356,1359-1360,1389,1409,1433,1445,1454,1461,1484,1522,1535,1544,1554,1560,1581,1597,1603,1609,1618,1634,1640,1649,1663,1675,1682,1700,1707,1711,1722`
- `src\replay\replay-export-mf.cpp`: `1212,1228,1233`
- `src\replay\replay-export.cpp`: `165,506,678,696,701`
- `src\shadow\shadow-commands.cpp`: `53,62,72,82,91,101,107,136,174`
- `src\terminal\actor-commands.cpp`: `102,105,112,123,155,164,178,194,207,212,227,235,247`
- `src\terminal\auth-commands.cpp`: `55,60,64,71,101,107-110,123,132,145,150,158-159,164-165,174-175,195`
- `src\terminal\badconn-commands.cpp`: `67,81,89,98,101,109,113,133,141,147,180`
- `src\terminal\crate-commands.cpp`: `72,172,175,191,202,217,231,244,256,269,281,293,308,321,331`
- `src\terminal\debug-commands.cpp`: `42,46,51,54-55,65,71,75,80,83,85,94,97,99,109,118-119,126,131-132,139,144-145,152,157-158,182,192,196,199,222,228,232,245,247,254,275,281,290,297,300,314,322,325,327-328,330,332,340,344,346,352,354,364,370,373,377-378,386,414,446,456,467,475,484,499,508,516,524,535,548,561,568,573,588,610,621,631,635,654,665,669,680,688,695,699,710,712,714,717,727,738,746`
- `src\terminal\duel-commands.cpp`: `38,69`
- `src\terminal\editor-commands.cpp`: `14,23,28,35,42,45,52,56,60,66`
- `src\terminal\map-entity-commands.cpp`: `71,85,96-101,108,111,113,121,131,133,140,142,151,153,166-167,199-200,208,210,223,230,238`
- `src\terminal\movement-commands.cpp`: `29,36,46,52,56,69,81,89,109,114,123,132,138,147,152,164,169,176,183,202,211,217,229,253,262`
- `src\terminal\network-commands.cpp`: `87,93,101,109,112,121,127,131,133,137,146,149,151,159,171,175,178,350,364,371,377,381,400,414,427,431,434,443,449,456-470,485,490,497,503,507,509,515,520,525,536,538,546,552,554,565,567,577,579,583,597,653,676,690,704,715,718-720,727-728,740,750,757,766,769-770,781,791,804,812,822,826,829,839,850,854,857,859,861,872,876,897,910,913,918,926,928,930,936,938,941,964,967,972,975,982,984,994,1001,1005,1011,1020,1025,1028,1059,1080,1082,1097,1131,1143,1151,1176,1192,1194,1201,1210,1217,1225,1232,1239,1243`
- `src\terminal\npc-commands.cpp`: `45,53,62,74,83,85,91,103,109,113-114,124,131,133,141,147,162,170,175,183,190`
- `src\terminal\object-commands.cpp`: `94,112,131,188,198,201,209,211`
- `src\terminal\player-commands.cpp`: `64,163,174,181,187,200,209,221,232,238,258,282,292,306,314,323,332,349,353,362-363,369,378,382,391,399,403,412,437,441,448,452,464,470,474,480,501,508,516,520,522,526,530`
- `src\terminal\procedural-world-commands.cpp`: `28,31,44,75,103`
- `src\terminal\replay-commands.cpp`: `31,35,79,87,92,100,114,121,126,142,145,150,162,171,178,198,202,211,218,221-222,232,235,241,253,280,301,320,349,358,371,379,426`
- `src\terminal\terminal-config-commands.cpp`: `70,101,112-113,172,195,205-213,223-225,290,297,308,318,322`
- `src\terminal\terminal-cursor-commands.cpp`: `24,36`
- `src\terminal\terminal-debug-toggles.cpp`: `25,42,59,76,93,110,127,144,157,171,188,205,222,239,256,273,286,300,315,319,329,341,356`
- `src\terminal\terminal-help-commands.cpp`: `27,34,39,49-52,54,68,73,90,96,101,104,130,134,145,169,176,199,205,230,236,241`
- `src\terminal\vip-commands.cpp`: `41,55,80,85,130,140`
- `src\terminal\weapon-bench-commands.cpp`: `22,42,60,107,129,197,327,343,354,361,377,391`
- `src\terminal\weapon-commands.cpp`: `119,125,164,182,224,254,302,310,336,345,360,373,381,385,389,398,403,415,420,429,435,449,463,484,496,499,515,517,529,533,586,591,603,608,620,625,637,642,654,662,666,677,682,696,701,713,718,732,737,751,756,768,773,794,813,899,1122,1135,1149,1350`
- `src\video\outro.cpp`: `94,100,112,130,144,154,157,165,176,190,205,216,230,239-240,251,265,273,277,286,290,294,301`
- `src\video\video-commands.cpp`: `21,25-29,34,43,45,50,61-62,64-65,67,74,81,89,92,103,117`
- `src\void-death\void-death-commands.cpp`: `27,33,43,49,62`

### C++ streams, debugger output, and Win32 output APIs

Inventory snapshot: 2026-10-10. Each value is an exact matching source line; ranges contain only consecutive matches.

- `src\debug\crash-handler.cpp`: `297,391,395,566,568,779`
- `src\map\texture_manager.cpp`: `40,47`

### stdout/stderr capture, flushing, and terminal-pipe plumbing

Inventory snapshot: 2026-10-10. Each value is an exact matching source line; ranges contain only consecutive matches.

- `src\combat\shot-profiler.cpp`: `150`
- `src\debug\log-manager.cpp`: `270,276,313-314`
- `src\game\game-cli.cpp`: `2402,2431,2437,2439,2443,2454,2559,2571,2607`
- `src\map\texture_manager.cpp`: `17`
- `src\network\ice\ice-agent.cpp`: `174,364`
- `src\network\ice\ice-server.cpp`: `261,297,423,820`
- `src\network\ice\ice-test.cpp`: `203,209,226,251,390,402,422,434,456,480,488,499,522,547,557,571,574,608,650,704,720,737,756,775,811,851,913,953,977,1020,1023`
- `src\network\server-ice.cpp`: `280,288,570`
- `src\network\server-packets.cpp`: `1279`
- `src\network\server.cpp`: `901,927`
- `src\network\test-events.cpp`: `50`
- `src\render\skybox.cpp`: `21`
- `src\replay\replay-export-ffmpeg.cpp`: `61`
- `src\replay\replay-export.cpp`: `130`


### Phase 5: validate the invariant before calling the work complete

Source review must prove that every `logs` writer reaches the canonical owner.
Build evidence is separate from runtime evidence. Runtime validation must use
the real executable and shared client/server path:

1. start from a clean test log root or an explicitly measured existing root;
2. run client and dedicated server concurrently;
3. force high-volume diagnostics and repeated rotations;
4. observe the live run directory while writing;
5. assert that every regular file is `<= 100,000,000` bytes;
6. assert that the recursive total is always `<= 1,000,000,000` bytes;
7. verify every JSONL segment parses line-by-line and chronological ordering
   remains recoverable;
8. kill one process and verify the surviving process does not delete its active
   file or corrupt the shared stream;
9. fill the quota and verify low-priority suppression/summarization rather
   than an unbounded append; and
10. inspect the live canonical journal for rotation, deletion, reservation,
    suppression, and final-size records.

The implementation is not complete when it merely builds or when one sample
run remains below 1 GB. Completion requires source inventory proof,
concurrent runtime proof, and separate human review of whether the retained
records still provide enough evidence for debugging.

## Noncanonical logging inventory to maintain

This section is the change ledger for logging methods that must disappear or
be explicitly approved as non-log output. Every entry must include:

```text
file:line-range
owner/function:
current sink or method:
why it is noncanonical:
disposition: delete | migrate-to-events.jsonl | approved-terminal-output | third-party-exception
replacement event/category/fields:
validation evidence:
```

Do not mark the migration complete until the repository-wide search returns no
unclassified direct writes under `logs`, no second events journal, no category
or summary file sink, and no diagnostic raw `printf`/`fprintf` path.

MiMITA debug logging and profiling specification
Date: July 18, 2026
Status: Target architecture
Maximum principle: Everything important must be observable, searchable, measurable, and reproducible.
________________


1. End goal
When mimita.exe runs, everything needed to understand the game’s behavior is:
printed live in the terminal if enabled
        +
written into readable text files
Logs must explain:
what happened
when it happened
where it happened
why it happened
what caused it
what was expected
what actually happened
how large the difference was
A human should be able to open the files and understand them.
An AI agent should be able to read the files and follow one event through the entire engine without guessing.
The logging system exists to replace statements such as:
I think it broke.
It feels laggy.
The grenade looks wrong.
The animation sometimes stops.
With evidence such as:
Grenade position error:
expected = (18.42, 3.10, 7.83)
actual = (18.42, 1.77, 7.83)
difference = 1.33 units vertically


First mismatch:
server tick = 18,401
client tick = 18,406
file = src/physics/loose-object-physics.cpp
line = 284
The central equation is:
bug = actual behavior - expected behavior

________________
2. Log folder and file output

All log folders and filenames follow the universal standard in
`docs/architecture/time-and-formatting/time-and-formatting.md`.

Folder format: `yyyy-mm-dd`
Filename format: `<Type>_yyyymmdd_hhmmss.txt`

All logs are written under:
    logs/yyyy-mm-dd/

Example:
    logs/2026-09-07/
        Summary_20260907_093015.txt
        Network_20260907_093015.txt
        Weapons_20260907_093015.txt
        Physics_20260907_093015.txt
        Performance_20260907_093015.txt
        Errors_20260907_093015.txt

A second execution creates:
    logs/2026-09-07/
        Summary_20260907_101422.txt
        Network_20260907_101422.txt
        ...

If the daily folder does not exist, create it.
If it already exists, reuse it.
Every execution creates new timestamped files.
Never overwrite an earlier run.

Summary contains the relevant contents of all enabled category logs in
chronological order. Logs must also print live to the game terminal. The
terminal and text files use the same formatted log records.

Historical format (pre-2026-09-06):
    Old logs used `mm-dd-yyyy` folders and `mmddyyyy_hhmmss` filenames.
    These remain valid historical evidence but must not be used for new logs.
________________


3. One central logger
There is one debug logging architecture. JUST ONE
NO PRINTF NO NOTHING ELSE JUST THIS 
Gameplay files do not each invent their own logging system.
All systems call one public logging entry point:
debug::log(...);
Conceptual interface:
namespace debug
{
    void log(const LogRecord& record);


    void logValue(const ValueLogRecord& record);


    void logEvent(const EventLogRecord& record);


    void logDifference(const DifferenceLogRecord& record);


    void logAssertion(const AssertionLogRecord& record);


    void logPerformance(const PerformanceLogRecord& record);
}
The central logger handles:
level filtering
category filtering
formatting
timestamps
tick and frame metadata
source file and line
event IDs
correlation IDs
queueing
thread safety
throttling
sampling
terminal output
file routing
summary output
flushing
retention
Gameplay systems only create structured records.
They do not decide how files are opened, named, rotated, or retained.
Do not use raw printf, std::cout, or unrelated file streams for debug output.
________________


4. Debug configuration
A single JSON file controls logging:
C:\important\mimita-priv-v8\config\debuglogger.json
This file is the authority for debug logging configuration.
It is hot-reloadable.
Changing it while the game runs must update logging without restarting the game.
Initial conceptual configuration:
{
  "enabled": true,
  "level": "important",


  "terminal_output": true,
  "summary_file": true,


  "categories": {
    "startup": true,
    "errors": true,
    "assertions": true,
    "performance": true,
    "network": false,
    "weapons": false,
    "projectiles": false,
    "damage": false,
    "movement": false,
    "collisions": false,
    "npcs": false,
    "animation": false,
    "effects": false,
    "gui": false,
    "avatar": false,
    "models": false,
    "replay": false,
    "audio": false
  },


  "sampling": {
    "default_every_n_ticks": 60,
    "log_on_abnormal_change": true
  },


  "retention": {
    "daily_logs_days": 14,
    "performance_logs_days": 30,
    "trace_runs": 3,
    "crash_logs_forever": true
  }
}
For now, logging is controlled through this JSON.
Do not make in-game console commands another competing authority.
An in-game GUI may be added later after the external JSON system is reliable.
________________


5. Log levels
Supported levels:
enum class LogLevel
{
    Off,
    Error,
    Important,
    Verbose,
    Trace
};
Off
No logging.
Error
Logs failures that prevent or seriously damage execution:
startup failure
missing required DLL
invalid required configuration
fatal network protocol mismatch
uncaught exception
assertion failure
NaN entering authoritative state
Important
Includes errors plus meaningful warnings and transitions:
player connected
player disconnected
attack rejected
file missing and fallback used
frame-time spike
server tick overrun
projectile correction above threshold
player died
map loaded
Verbose
Includes important plus detailed state needed to investigate systems:
exact function
exact duration
exact entity
exact tick
exact input
exact before state
exact after state
exact validation result
Trace
Records extremely detailed execution.
Trace may include individual steps through a system, but must still use sampling and throttling where continuous data would become unusable.
Trace is not permission to freeze the game by writing millions of lines per second.
________________


6. Log record format
Every log record should contain enough information to locate and understand the event.
Conceptual structure:
struct LogRecord
{
    LogLevel level;
    LogCategory category;


    uint64_t eventId;
    uint64_t correlationId;
    uint64_t parentEventId;


    uint64_t frameNumber;
    uint64_t simulationTick;
    uint64_t serverTick;
    uint64_t clientTick;


    EntityId entityId;
    PlayerId playerId;


    const char* sourceFile;
    int sourceLine;
    const char* functionName;


    std::string eventName;
    std::string reason;
    std::string message;
};
Formatted example:
[2026-07-18 09:30:15.284]
[IMPORTANT]
[NETWORK]
[EVENT=NETWORK_00381]
[CORRELATION=ATTACK_00481]
[PLAYER=2]
[CLIENT_TICK=1000]
[SERVER_TICK=1002]
[src/network/server-weapon-validation.cpp:184]
[validateAttackRequest]


Attack request accepted.


reason = player alive, weapon equipped, ammo available,
         cooldown complete, direction valid
Every record should answer:
What is this?
Why was it logged?
Which event does it belong to?
Where in the code did it happen?
Bad:
player pos = 1 2 3
Better:
[MOVEMENT]
Player position before wall collision resolution.


reason = investigating unexpected wall penetration
player = 2
position = (1.0, 2.0, 3.0)
velocity = (8.2, 0.0, -1.3)

________________
7. Event and correlation IDs
Every important operation receives a searchable ID.
Category event examples:
COLLISION_00042
GUI_00013
NETWORK_00381
AVATAR_00009
PERFORMANCE_00127
Cross-system operations receive correlation IDs:
ATTACK_00481
PROJECTILE_09821
EXPLOSION_15502
RESPAWN_00088
CONNECTION_00014
Example attack chain:
[ATTACK_00481] input detected
[ATTACK_00481] request created
[ATTACK_00481] request transmitted
[ATTACK_00481] server received request
[ATTACK_00481] ammo validated
[ATTACK_00481] cooldown validated
[ATTACK_00481] projectile created
[ATTACK_00481] confirmation transmitted
[ATTACK_00481] prediction reconciled
The projectile may continue under its own ID while preserving the parent:
[PROJECTILE_09821]
parent = ATTACK_00481
The explosion may then continue:
[EXPLOSION_15502]
parent = PROJECTILE_09821
root = ATTACK_00481
This makes the entire causal chain searchable.
________________


8. Expected, actual, and difference
Important diagnostic records must compare expected and actual behavior.
Conceptual record:
struct DifferenceLogRecord
{
    std::string measurement;


    Value expected;
    Value actual;
    Value difference;


    Value allowedMinimum;
    Value allowedMaximum;


    DifferenceStatus status;
};
Example:
[COLLISION DIFFERENCE]
event = COLLISION_00042
measurement = penetration depth


expected = 0.000 units
actual = 0.420 units
difference = +0.420 units


allowed maximum = 0.010 units
status = FAILED
over limit = 42.0x
Another example:
[PROJECTILE RECONCILIATION]
projectile = 9821


expected server position = (10.20, 4.40, 8.10)
actual client position = (10.18, 4.12, 8.09)


position error = 0.281 units
small correction threshold = 0.100 units
major correction threshold = 2.000 units


classification = MEDIUM_CORRECTION
Do not log only vague descriptions.
Use:
numbers
units
thresholds
ratios
exact states
true/false

________________
9. Assertions and invariants
Assertions define states that must never become invalid.
Examples:
player position must be finite
player velocity must be finite
health must remain within valid bounds
projectile radius must be greater than zero
camera quaternion magnitude must remain near 1
ammo must not become negative
server tick must never decrease
an event ID must not be processed twice
Conceptual assertion:
debug::assertInvariant(
    std::isfinite(player.velocity.x),
    "PLAYER_VELOCITY_FINITE",
    context);
Failure output:
[ASSERTION FAILED]
assertion = PLAYER_VELOCITY_FINITE


file =
C:\important\mimita-priv-v8\src\physics\movement\physics-collision.cpp


line = 582
function = resolvePlayerCollision


player = 2
tick = 18,401


current velocity = (NaN, 0.0, 4000.0)
previous velocity = (12.4, 0.0, -3.2)
collision normal = (NaN, NaN, NaN)
contact point = (42.1, 3.8, 17.2)


parent event = COLLISION_00042
An assertion record must include the surrounding state needed to investigate it.
Do not only print:
assert failed

________________
10. Performance profiling
The debug system includes one shared performance profiler.
Systems use scoped timers:
debug::ScopedTimer timer(
    "NPC_RESPAWN",
    LogCategory::Performance,
    PERFORMANCE_BUDGET_NPC_RESPAWN_MS);
The timer records:
start time
end time
duration
frame
tick
thread
entity
event
budget
over-budget ratio
Example:
[PERFORMANCE]
operation = NPC_RESPAWN
budget = 0.500 ms
actual = 4.200 ms
status = OVER_BUDGET
ratio = 8.40x
Frame-time accounting should provide a breakdown:
Frame 48,120


total frame time = 20.02 ms


simulation = 19.86 ms
    NPC respawn = 19.20 ms
    collisions = 0.31 ms
    weapons = 0.18 ms
    network = 0.09 ms
    animation = 0.08 ms


rendering = 0.16 ms


unaccounted time = 0.00 ms
The measured categories should approximately add to the total.
If they do not, report:
unaccounted frame time
Do not hide missing time.
The long-term goal is to reduce every duration toward zero, while budgets provide measurable current targets.

## JSONL frame-time profile

The runtime measures every frame. Each named scope records its inclusive
duration in memory with a parent frame ID; it does not write one line per
scope per frame. Every 60 simulation ticks it emits one bounded JSONL summary
for the preceding one-second window.

Each summary contains frame count, total/average/min/max/p95/p99 frame time,
simulation time, render time, present/swap time, unaccounted time, and
contributors sorted from greatest total time to least total time. Each
contributor includes call count, total time, average time, maximum time, and
the worst frame ID. If contributor totals do not approximately equal the
measured frame total, the record reports `unaccounted_frame_time`.

Networked summaries also include active client/server generations and hashes.
________________


11. Sampling, throttling, and queues
Do not write unchanged continuous state every tick unless explicitly required.
Instead of:
print player position every frame forever
Use policies such as:
log every 60 ticks
log when value changes beyond threshold
log when an abnormal state occurs
log the first occurrence
log once per time interval
log a summary of repeated events
Example configuration:
{
  "movement_position": {
    "every_n_ticks": 60,
    "minimum_change": 0.5,
    "always_log_abnormal": true
  }
}
Repeated event summary:
[THROTTLED EVENT SUMMARY]
event = PROJECTILE_SMALL_CORRECTION


suppressed repetitions = 842
period = 10.0 seconds


maximum error = 0.042 units
average error = 0.011 units
Logging must use a queue so gameplay systems do not constantly block on disk writes.
Conceptual flow:
gameplay thread creates record
        ↓
record enters bounded log queue
        ↓
logging worker formats record
        ↓
terminal and file outputs receive record
If the queue becomes full:
* Preserve errors and assertions.
* Preserve major performance events.
* Drop or summarize low-priority trace records.
* Report how many records were dropped.
* Never silently lose critical failures.
________________


12. Reproducibility header
Every log file begins with a run header.
The header includes:
run ID
start date and time
git commit
git branch
dirty working tree true/false
changed files summary
build configuration
compiler version
build timestamp
protocol version
operating system
CPU
GPU
RAM
command-line arguments
working directory
loaded mods
loaded maps
loaded config files
config hashes
weapon-definition hash
network mode
server/client role
Example:
============================================================
MiMITA DEBUG RUN
============================================================


run_id = RUN_20260718_093015_0001


git_commit = a42cd189
git_branch = network-rewrite
working_tree_dirty = true


build = Debug
compiler = MSVC 19.42
build_timestamp = 2026-07-18 09:10:42


os = Windows 11
cpu = Intel i5-13420H
gpu = NVIDIA RTX 4050
ram = 16 GB


process_role = client
network_protocol = 18
server_address_type = local


loaded_config:
C:\important\mimita-priv-v8\config\gameplay.json
C:\important\mimita-priv-v8\config\debuglogger.json
C:\important\mimita-priv-v8\config\weapons.json
This eliminates ambiguity about which code and configuration created the behavior.
________________


13. Causality and golden paths
Logs should explain causes, not only symptoms.
Example:
NPC stopped updating for 3.2 seconds.
The useful chain is:
NPC death
    ↓
ragdoll creation
    ↓
particle burst
    ↓
4,300 allocations
    ↓
allocator stall
    ↓
simulation tick overrun
    ↓
NPC update delayed
Golden paths define the expected stages of important operations.
Example weapon path:
attack input
    ↓
attack request created
    ↓
client prediction started
    ↓
server request received
    ↓
ammo checked
    ↓
cooldown checked
    ↓
attack accepted
    ↓
hitscan/projectile/melee executed
    ↓
damage or impact resolved
    ↓
effects emitted
    ↓
confirmation received
    ↓
prediction reconciled
Every stage logs the same correlation ID.
At completion, the logger may compare expected and observed stages:
[GOLDEN PATH RESULT]
path = PROJECTILE_ATTACK
correlation = ATTACK_00481


expected stages = 12
completed stages = 8


missing:
server_projectile_spawned
projectile_confirmation_sent
client_prediction_adopted
reconciliation_completed


first missing stage =
server_projectile_spawned
This identifies where execution stopped.
________________


14. System-specific logging
Every major system defines meaningful diagnostics.
Networking
Log:
packet type
packet size
sequence
acknowledgements
request ID
event ID
client tick
server tick
ping
jitter
packet loss
accept/reject result
rejection reason
prediction error
Weapons
Log:
weapon definition
weapon runtime before
attack request
ammo before/after
cooldown before/after
generated directions
spread seed
execution path
hit/projectile/melee result
Projectiles and loose objects
Log:
position
velocity
angular velocity
gravity
collision shape
contact point
contact normal
impact velocity
bounce result
fuse
client/server position error
Movement and collisions
Log:
input
position before
velocity before
collision query
surface normal
penetration depth
resolution impulse
position after
velocity after
expected movement
actual movement
NPCs
Log:
NPC ID
definition
position
rotation
state
decision
target
spawn reason
death reason
respawn duration
AI time
physics time
render time
GUI
Log:
element ID
position
size
scale
rotation
text
font
color
alpha
visible
hovered
clicked
layout parent
Avatar and models
Log:
exact file paths
file existence
asset hashes
loaded dimensions
metadata
face/body assignment
position
rotation
scale
color
alpha
model parse result
Animation and effects
Log:
event ID
entity generation
animation state
requested pose
actual pose
effect definition
spawn position
lifetime
destroy reason
Only enabled categories write detailed records.
________________


15. Logging around suspected code
When a specific line or operation is suspected, log immediately before and after it.
Example:
debug::logValue({
    .eventName = "VELOCITY_BEFORE_RESOLUTION",
    .value = player.velocity
});


resolveCollision(player, collision);


debug::logValue({
    .eventName = "VELOCITY_AFTER_RESOLUTION",
    .value = player.velocity
});
The records use the same correlation ID.
This creates:
state before operation
operation begins
operation result
state after operation
difference
Do not scatter custom file-writing code around the target function.
Only call the central logger.
Temporary investigation logs should include an explicit reason:
reason = investigating grenade vertical velocity becoming zero
When the investigation ends, either:
* Remove the temporary record.
* Convert it into a permanent useful diagnostic.
* Disable it through sampling or level configuration.
________________


16. Retention and crash safety
Default retention targets:
daily general logs:
keep 14 days


performance logs:
keep 30 days


trace logs:
keep latest 3 runs


crash logs:
keep indefinitely


assertion failure logs:
keep indefinitely
Retention runs safely at startup or shutdown.
It must not delete files from the current run.
Crash-sensitive records should flush promptly:
fatal error
assertion failure
server shutdown
protocol corruption
NaN authoritative state
The logger should maintain a small recent in-memory ring buffer.
If the game crashes, write recent records into a crash file:
Crash_07182026_093442.txt
This file includes the events immediately before the crash.
________________


17. Regression investigation
When a bug may have worked previously, the investigation process is:
1. Identify whether it ever worked.
2. Find the most recent known working commit.
3. Record the current commit.
4. Compare changed files.
5. Identify changed systems.
6. Reproduce the bug on the current version.
7. Reproduce the same test on the old version.
8. Compare logs.
9. Find the first event where behavior diverges.
10. Modify the smallest responsible layer.
The logging system records Git metadata, but it does not automatically rewrite or revert source code.
An AI agent investigating a regression should inspect:
current logs
known-working logs
git diff
changed configuration
changed asset hashes
changed packet protocol
The important comparison is:
same action
same initial state
old expected event path
new broken event path
first divergence

________________
18. Numeric success conditions
Every fix must define what counts as success.
Bad:
Camera export is fixed.
Good:
camera position error <= 0.05 units
camera rotation error <= 0.25 degrees
camera FOV error <= 0.10 degrees
Bad:
Grenade looks smooth.
Good:
server simulation = 60 Hz
client simulation = 60 Hz
average projectile correction <= 0.05 units
95th percentile correction <= 0.15 units
major corrections = 0 during test
duplicate visible projectile count = 0
Bad:
Respawn no longer lags.
Good:
respawn total CPU time <= 1.00 ms
maximum frame time during respawn <= 16.67 ms
unaccounted frame time <= 0.20 ms
unexpected allocations during respawn = 0
Expected values must come from the actual design requirement.
Do not define expected behavior by copying the broken actual value.
After a fix, read the logs and verify:
the test actually exercised the feature
the expected value is correct
the actual value meets it
the behavior repeats consistently
no new warnings appeared

________________
19. File responsibilities
debug/debug-log.h
Does:
define public logging API
define log levels
define categories
define structured record types
define helper macros for source location
Does not:
contain gameplay logic
know weapon behavior
perform collision resolution
debug/debug-log.cpp
Does:
receive records
filter records
assign event IDs
format output
route records
write terminal output
write category files
write summary file
Does not:
decide what gameplay should happen
calculate expected gameplay values
debug/debug-config.cpp
Does:
load debuglogger.json
validate configuration
hot reload configuration
provide current category and level settings
Does not:
write logs directly
contain GUI settings unrelated to logging
debug/debug-writer.cpp
Does:
manage output folders
manage files
write queued records
flush critical records
apply retention
Does not:
inspect gameplay entities
debug/debug-profiler.cpp
Does:
measure durations
track nested scopes
calculate budgets
calculate frame breakdowns
report unaccounted time
Does not:
render frames
update NPCs
debug/debug-events.cpp
Does:
generate event IDs
generate correlation IDs
track parent/root relationships
track golden-path stages
Does not:
execute weapon attacks
send network packets
Gameplay files only call these shared systems.
________________


20. Final rules
The debug architecture follows these permanent rules:
One logger.


One hot-reloadable JSON authority.


No raw printf-based debug architecture.


Every important record has a reason.


Every important operation has an ID.


Every cross-system operation has a correlation ID.


Every record includes exact source location.


Important bugs compare expected, actual, and difference.


Important values use numbers and units.


Performance uses budgets and measured ratios.


Continuous data uses sampling and throttling.


Critical records are never silently discarded.


Logs contain reproducibility metadata.


Logs explain causes, not only symptoms.


Golden paths reveal the first missing stage.


Assertions capture surrounding state.


Every fix has numeric success conditions.


Every run remains readable by a human and an AI.


The logger observes gameplay.


The logger does not own gameplay.
The final target is:
Something breaks
        ↓
open the newest Summary file
        ↓
search the event or correlation ID
        ↓
follow the complete event chain
        ↓
find the first expected/actual difference
        ↓
open the exact file and line
        ↓
fix the responsible system
        ↓
run again
        ↓
verify numeric success conditions

