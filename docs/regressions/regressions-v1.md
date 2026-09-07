// 09 03 2026, 15 42
/* purpose
* record confirmed behavior breaks discovered through human review or playtesting
* preserve the exact wrong behavior, cause, fix, and proof for future agents
* connect each regression to the changelog session that introduced or fixed it
* this file DOES NOT record normal AI work history
* this file DOES NOT replace current specifications
* this file DOES NOT allow old entries to be rewritten or deleted
*/

# MiMITA Regression Records

This is an append-only record of confirmed regressions. Normal AI work history
belongs in `docs/changelog/`. A new entry must include the expected behavior,
actual behavior, exact specification, wrong code, corrected code, cause, fix,
proof, observed time, and related changelog file.

Whats this

- 9 2 2026 this tracks like  
- Ok if the behavior we want worked before, then later, it stopped working,   
- Write it here and why and then what fix etc 

9 2 2026 format

1. Issue: lala  
   1. Bad behavior  
      1.    
   2. Date and time first observed:   
      1.    
   3. Why bad behavior  
      1.     
   4. What fixed it, date and time  
      1.  
   5. What we learned  
      1. 

newest at top 9 3 2026

9 6 2026 2136 — Replay export camera fix confirmed working via diagnostic logging

1. Issue: replay export camera stuck under the world, not following player POV
   1. Bad behavior
      1. Exported video shows camera at ground level / (0,0,0) for entire clip
      2. Camera does not follow the player's recorded perspective
      3. All three exports from 9/6 (18:53, 19:35, 19:54) had this bug
   2. Date and time first observed: 9 6 2026 ~19:35 EST (same as 1951 regression)
   3. Why bad behavior
      1. Same root cause as 1951 regression: `beginPlayback()` in subprocess blocked recording
      2. Recording condition `isRecording() && !replayPlaybackActive` was false during export
      3. Scene frames in clip had empty camera data (tick=0, camera=(0,0,0), no actors)
      4. The fix at 19:51 was correct in source code but the running binary was not rebuilt
      5. Clips recorded with old binary had empty camera data baked into JSON
      6. Rebuilding binary and re-recording clips was required to fix the output
   4. What fixed it, date and time: 9 6 2026 2136 EST (confirmation via diagnostic logging)
      1. Verified the 19:51 fix is correct by adding diagnostic logging to engine-tick-camera.cpp
      2. Ran export subprocess directly: `mimita.exe --export-replay <clip> --output <path> --visible --timeout 30 --replay-export-verbose`
      3. Diagnostic logs confirmed camera position follows clip data at every tick:
         - Tick 0: frameCamPos=(6.97, -26.14, 95.66) → controller sets it → finalPos=(6.97, -26.14, 95.66)
         - Tick 1: frameCamPos=(6.97, -26.14, 95.55) → correctly interpolating downward
         - Tick 2: frameCamPos=(6.97, -26.14, 95.43) → still following clip data
      4. Export produced 5.4MB MP4 with correct camera (vs 600-760KB from broken builds)
      5. User confirmed test export plays correctly with camera following player
   5. What we learned
      1. The 1951 fix (recording condition + removing beginPlayback) is correct and sufficient
      2. The camera controller in "Recorded" mode reads scene frames and sets camera.pos correctly
      3. The export subprocess is identical whether run directly or spawned from the game
      4. Old clips recorded with broken binary have empty camera data that cannot be fixed by rebuilding — must re-record
      5. The `test-diag-export.mp4` (5.4MB) vs user's broken exports (600KB) proves the camera fix works when clip has valid data
      6. The `anyFreecam` flag was 0 during export, `camCtrlMode=0` (Recorded), `hasFrame=1` — all conditions correct
   6. Proof: diagnostic log file at `logs/09-06-2026/ReplayExport_log_*.txt` shows camera position at every tick matching clip data
   7. Files changed:
      1. `src/engine/engine-tick-replay.cpp:303-304` — recording condition: `isRecording() && (!replayPlaybackActive || isReplayExportActive())`
      2. `src/replay/replay-export-subprocess.cpp:281-288` — removed `beginPlayback()` before `seekToTick(0)`
   8. How to prevent this from breaking again
      1. NEVER call `beginPlayback()` in the export subprocess — `seekToTick()` is sufficient
      2. NEVER change the recording condition to remove the `|| isReplayExportActive()` check
      3. When testing export, always rebuild binary BEFORE recording new clips
      4. Verify clip JSON has non-zero camera positions in scene frames before debugging export issues

9 6 2026 1951 — Replay export camera stuck at (0,0,0) instead of following player POV

1. Issue: replay export produces valid MP4 with outro, but camera is stuck at position (0,0,0) for the entire export instead of following the player's recorded POV
   1. Bad behavior
      1. Exported video shows static view under the map at (0,0,0)
      2. Camera does not follow the player's recorded perspective
      3. All 619 scene frames in the clip have tick=0, camera=(0,0,0), no actors
   2. Date and time first observed: 9 6 2026 ~19:35 EST
   3. Why bad behavior
      1. `replay-export-subprocess.cpp` called `beginPlayback()` which set `mPlaying=true`
      2. This made `replayPlaybackActive = gReplayPlayer.isPlaying()` return true
      3. The recording condition `isRecording() && !replayPlaybackActive` became false
      4. The recording block in `engine-tick-replay.cpp` was skipped entirely
      5. Scene frames were never populated with camera position, actors, or tick data
      6. Ring buffer got empty frames (tick=0, camera=(0,0,0), no actors)
      7. `makeClip` copied these empty frames into the clip
   4. What fixed it, date and time: 9 6 2026 19:51 EST
      1. Changed recording condition to `isRecording() && (!replayPlaybackActive || isReplayExportActive())`
      2. Removed `beginPlayback()` from the export subprocess (seekToTick already sets mPlaying=true)
   5. What we learned
      1. `beginPlayback()` has side effects beyond setting mPlaying — it blocks recording via the replayPlaybackActive check
      2. The recording condition must account for the export state to allow recording during export
      3. `seekToTick()` is sufficient for the export subprocess — `beginPlayback()` is redundant and harmful
      4. Empty ring buffer frames propagate through makeClip into the exported clip, producing camera at (0,0,0)
      5. The export subprocess and main process have separate REPLAY_RECORDER instances — the subprocess's recorder must be properly initialized for recording to work

9 6 2026 1839 — Replay export produces 261-byte empty MP4 (totalTicks=0)

1. Issue: pressing P to export replay produces a 261-byte MP4 with no video content, and the outro fails to append
   1. Bad behavior
      1. Export produces 261-byte file (MP4 header only, no video frames)
      2. Outro append fails because MP4 has no video stream
      3. Export completes in 0.5 seconds with only 1 frame captured
   2. Date and time first observed: 9 6 2026 ~18:12 EST
   3. Why bad behavior
      1. `startReplayExport()` in `replay-export-json.cpp` spawned a subprocess without loading the clip to determine `gJob.totalTicks`
      2. The main process's `gJob.totalTicks` remained at 0 (default)
      3. The export loop checked `doneTick >= gJob.totalTicks` → `0 >= 0` = true → stopped after 1 frame
      4. The subprocess loaded the clip and set its own `gJob.totalTicks`, but this was the subprocess's copy — the main process's value was never updated
   4. What fixed it, date and time: 9 6 2026 18:39 EST
      1. Added `ReplayClip::load(jsonPath)` in `startReplayExport()` before spawning the subprocess
      2. Set `gJob.totalTicks = clip.header.tickCount` from the loaded clip
      3. Added validation: fail if clip has 0 ticks and no scene frames
   5. What we learned
      1. The subprocess export architecture means the main process and subprocess have separate copies of `gJob` — changes in the subprocess don't propagate back
      2. The main process must load the clip to extract metadata (totalTicks) before spawning the subprocess
      3. The 261-byte file was an MP4 container header with no video content — FFmpeg creates the output file on `BeginWriting()` but writes no frames when totalTicks=0
      4. The outro append fails on empty MP4s because FFmpeg's concat filter requires valid video streams
      5. Always validate clip metadata before starting export — don't assume the subprocess will fix it

9 3 2026

1. Issue: the website https://mimita.fun the signing up and logging in is broken 9 3 2026 1526
   1. Bad behavior  
      1.    i go to the site and i log in and it sas database connection failed. check server logs.
      2. thats not good and sucks dick
   2. Date and time first observed:   
      1.    this hapepned before, but its happened first viewed at  9 3 2026 at 201 am est  bc of a discrod message in mimita discord
      2. viewed again  9 3 2026 1527
   3. Why bad behavior  
      1.    i need to be able to log in like thats a  core, if mimita is a tree, logging in/signingup is the roots of the tree, thats so so so imporatnt to work 
   4. What we learned  
      1.    
   5. What fixed it, date and time  
      1.  

9 2 2026

1. Issue: ui making performance suck dick   
   1. Bad behavior  
      1.  It suckds dick and t  
      2. Takes like 10ms on a integrapted graphics computer  w no gpu  to render ui thats bad   
   2. Date and time first observed:   
      1.  Like  prob aug 1 2026 but recent was  aug 30 2026  
   3. Why bad behavior  
      1.  Bc all the ui was getting drawn   
   4. What we learned  
      1.  Do not draw all the ui all in one, matter of fact  
      2. Batch evreuthing we can  
      3. Dont collide with all triangels in the whole world either  
   5. What fixed it, date and time  
      1.  Batching ui calls into 1 singel call  
      2. Made fps go to liek 100 minimum on a family computer  
      3. Just needs better  tweaks long term for better perofmance etc   
   
2. Issue: collisions sucked dick and made huge frame time lag when getting close to a big clinder  
   1. 9 2 2026 fill in later bc thats liek mimita preview liek v2.14 on mimtia youtube channel

9 6 2026 resolution for the 9 3 2026 website authentication outage

1. Expected behavior: `https://mimita.fun` signup and signin requests can query
   the account database and return normal validation or authentication results.
2. Actual behavior: PostgreSQL cluster `14/main` was down, with no PostgreSQL
   process or listener on port 5432. The online `mimita-api` process returned
   `500` and `ECONNREFUSED` for `/api/auth/signin`, `/api/auth/me`, and
   `/api/site/banner`.
3. Why it happened: the API's configured database dependency was unavailable;
   the application could not connect to PostgreSQL. This was an infrastructure
   service outage, not a bad username, password, signup form, or frontend route.
4. What fixed it, 9 6 2026 08:33 EST: started only the existing PostgreSQL
   `14/main` cluster on the VPS. It became `online` and `pg_isready` reported
   `accepting connections`; no code or VPS files were changed.
5. Proof: a correctly formatted non-mutating invalid-signin probe returned
   HTTP `401` with `invalid username/email or password`, proving the auth query
   path reached the database instead of failing with HTTP `500`.
6. Related session record: `docs/changelog/09-06-2026/09-06-2026-08-34-02-website-auth-recovery.md`.
7. 9 6 2026 0900 extra note: SO ENSURE THE VPS IS ALWAYS RUNNNING, AND IF DATABASE ISSUES HAPPEN, NEED TO BE ABLE TO START THE VPS FROM MOBILE PHONE NOT JUST PC

9 6 2026
1. Issue: all UI was drawn individually calls, making perfomrance very bad on low power devices and all devices in general. goes directly against a big assertion/invariant of  as close to 0ms frame times as we can possibly get, for any device at all
   1. Bad behavior  
      1.    all UI renderd individually, meaning each new letter = new draw call, unecessary work
   2. Date and time first observed:   
      1.    not sure it has been like that for like months maybe like 5 1 2026, but observerd again in a bad way like 8 31 2026s
   3. Why bad behavior  
      1.     because we didnt even know the code was bad bc i just assumed that it was fine, and also bc the pc i use to code it is powerful device so its not fair
   4. What fixed it, date and time  
      1.  we fixed it sometime between  8 31 2026 and like 9 6 2026, its in the git commit history, i committed from the low power device ihave
   5. What we learned  
      1. sometimes code is not efficient even if it seems like it, like who would thiink teh UI is taking 3ms to render every single frame, so question all assumptions over and over bc it might be the most randomest little thing making the issues happen 

   ## 2. 9 6 2026 2000 — Articles lost after VPS git pull

   1. Bad behavior
      1. 20-30 articles created through the admin editor at /admin/articles disappeared from the live site
      2. Only 1 article remains (welcome-to-mimita-news.md)
      3. The /articles page shows the article but clicking it does nothing (broken link)
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. Articles are stored as .md files in content/articles/ on the VPS filesystem
      2. These files were never committed to git — they existed only on the VPS
      3. When we ran `git pull --ff-only` to deploy the data-saving persistence, the content/articles/ directory was updated to match the repo state (which only had 1 article)
      4. The untracked .md files were overwritten/lost by the pull
      5. Additionally, ArticlesIndex.jsx used `article.url` but the generated JSON has no `url` field — only `slug` — so links were broken even for the remaining article
   4. What fixed it, date and time
      1. Fixed ArticlesIndex.jsx: changed `article.url` to `/articles/${article.slug}` (9 6 2026)
      2. Articles cannot be recovered from git — they were never committed
      3. Added rule to vps-deployment.md: content/articles/ must be committed to git so articles survive deployments
   5. What we learned
      1. Files created on the VPS through the admin editor are NOT automatically committed to git
      2. `git pull --ff-only` does not preserve untracked files in tracked directories
      3. Always commit user-generated content (articles) to git before deploying
      4. The ARTICLES_DIR path in admin.js resolves to content/articles/ at the repo root — this directory must be tracked

   ## 3. 9 6 2026 2000 — Profile stats not visible on live site

   1. Bad behavior
      1. The profile page at /users/admin did not show gold, XP, kills, deaths, or playtime
      2. The API returned correct data but the frontend didn't render it
   2. Date and time first observed: 9 6 2026 ~19:30 UTC
   3. Why bad behavior
      1. ProfileStats.jsx and persistentStats.js were added to the repo on 9 6 2026
      2. The VPS frontend dist was built on 9 1 2026 — before these components existed
      3. The deployed JS bundle had 0 matches for ProfileStats, playtimeTicks, or formatPersistentStat
      4. The VPS deployment procedure did not include a frontend rebuild step
   4. What fixed it, date and time
      1. Added "rebuild frontend" step to vps-deployment.md (9 6 2026)
      2. Ran `npm run build` on VPS to rebuild dist with new components (9 6 2026)
   5. What we learned
      1. Every deployment that touches website/src/ MUST rebuild the frontend
      2. A stale dist/ serves old JavaScript that may lack new components
       3. The deployment procedure must include `cd /root/mimita-site/website && npm run build` as a mandatory step

9 6 2026 2000 — In-game sign-in fails with "Could not load account data" and status text overlaps error

   Game version: v2.0.6
   Git branch: 8292026stash
   Git HEAD: decc9fa
   Affected clients: all v2.0.x releases (v2.0.0 through v2.0.6)
   Affected endpoint: GET /api/game/me (bootstrap)

1. Issue: signing in through the in-game exe fails with "Could not load account data" error, and the green "Signing in..." / "Loading account..." status text overlaps the red error text at the same screen position
   1. Bad behavior
      1. User enters correct admin credentials and clicks Sign In
      2. Login succeeds (HTTP 200, accountId=1, username=admin returned)
      3. The `getGameBootstrap()` call to `GET https://mimita.fun/api/game/me` throws a JSON parse exception: `json.exception.type_error.302: type must be number, but is string`
      4. Bootstrap fails, error "Could not load account data" is set
      5. But the green status text ("Loading account...") is never cleared, so both green status and red error render at position (720, 504) overlapping each other
   2. Date and time first observed: 2026-09-06T19:40:00Z (first log capture), confirmed again at 2026-09-06T00:02:00Z
   3. Why bad behavior
      1. Three bugs combined:
         A. SERVER-SIDE: node-postgres returns PostgreSQL BIGINT columns (OID 20) as JavaScript strings by default. The `game_stats` table has BIGINT columns: `total_xp`, `gold`, `playtime_ticks`, `playtime_seconds`, `lifetime_player_kills`, `lifetime_npc_kills`, `lifetime_deaths`. The `/api/game/me` endpoint returns these as `"0"` (string) instead of `0` (number). The `db.js` file had no `pg.types.setTypeParser` configuration.
         B. CLIENT-SIDE: `parseStats()` in `api-client.cpp` used `s.value("total_xp", 0LL)` which throws when the JSON value is a string like `"0"` instead of a number `0`. All v2.0.x clients have this bug.
         C. CLIENT-SIDE: `setError()` in `auth-controller.cpp` set `mRuntime.state = Failed` and `mRuntime.errorMessage` but never cleared `mRuntime.statusText`. Since both `statusText` and `errorText` elements in `login-menu.json` are at the same position (x=720, y=504), both rendered on screen simultaneously.
   4. What fixed it, date and time: 2026-09-06T00:05:00Z
      1. Fix A — SERVER: Added `pg.types.setTypeParser(20, parseInt)` to `db.js` to convert BIGINT to JavaScript Number globally. This fixes ALL existing v2.0.x clients without requiring a new release.
      2. Fix B — SERVER: Fixed `defaultStats()` in `game-api.js` line 38: changed `playtime_ticks: "0"` (string) to `playtime_ticks: 0` (number). This fixes the fallback for new users with no stats row.
      3. Fix C — CLIENT: Replaced direct `.value()` calls with helper functions `jsonInt()`, `jsonLong()`, `jsonFloat()` in `parseStats()` that check `is_number_integer()`, `is_number()`, or `is_string()` and convert accordingly. This makes the client robust against both string and number values.
      4. Fix D — CLIENT: Added `mRuntime.statusText.clear()` to `setError()` so the green status text disappears when an error is set.
      5. Fix E — CLIENT: Added `Debug::warn(Debug::Category::Auth, ...)` at all 5 `getGameBootstrap()` failure return paths so future failures are diagnosable from logs.
   5. What we learned
      1. node-postgres returns PostgreSQL BIGINT as JavaScript strings by default (not numbers). This is deliberate to avoid precision loss beyond Number.MAX_SAFE_INTEGER. Any code that reads BIGINT columns must handle string values. The proper fix is `pg.types.setTypeParser(20, parseInt)` in `db.js`.
      2. Never assume JSON numeric fields are always numbers. Any field that goes through a database driver, HTTP transport, or JSON serialization boundary can be a string. Use defensive parsing (check type before get).
      3. `setError()` must clear all previous status text. If it doesn't, both status and error text render at the same GUI position and overlap visually.
      4. Silent failure paths in network calls are dangerous — the original `getGameBootstrap()` had 5 return paths with zero logging. Adding logging at each path immediately revealed the root cause on the first test run.
      5. The server-side `defaultStats()` must return numbers, not strings, for all numeric fields. A string default creates a type inconsistency that breaks clients.
      6. Server-side fixes (db.js type parser) help ALL existing clients immediately without requiring a new game release. Always prefer server-side fixes for backward compatibility.
   6. Proof: after deploying the server fix, v2.0.6 clients can sign in successfully. The `/api/game/me` endpoint now returns numeric fields as numbers. The client bootstrap completes and transitions to the main menu.
   7. Files changed:
      1. `website/server/db.js:17` — added `pg.types.setTypeParser(20, parseInt)` to convert BIGINT to Number
      2. `website/server/game-api.js:38` — fixed `playtime_ticks: "0"` → `playtime_ticks: 0`
      3. `src/auth/auth-controller.cpp:263-268` — added `mRuntime.statusText.clear()` to `setError()`
      4. `src/website/api-client.cpp:300-343` — added `jsonInt()`, `jsonLong()`, `jsonFloat()` helpers and rewrote `parseStats()` to use them
      5. `src/website/api-client.cpp:426-475` — added `Debug::warn` logging at all 5 `getGameBootstrap()` failure paths
   8. How to prevent this from breaking again
      1. NEVER remove or comment out the `pg.types.setTypeParser(20, parseInt)` line in `db.js`. Without it, all BIGINT fields return as strings and break C++ clients.
      2. NEVER use `.value("key", 0)` on a JSON field that comes from an external API without checking the type first. Use the `jsonInt`/`jsonLong`/`jsonFloat` helpers.
      3. ALWAYS clear `mRuntime.statusText` in `setError()` — any new code path that sets an error must not leave stale status text.
      4. When adding new fields to `GameStats` or `GameUserInfo`, add them to `parseStats`/`parseUserInfo` with the same string-or-number defensive parsing.
      5. The server-side `defaultStats()` in `game-api.js` must return numbers, not strings, for all numeric fields.
   6. When changing the database schema (adding BIGINT columns), always verify the API response types by curling the endpoint and checking JSON types.

## 9 7 2026 — VIP source changes not visible through SSH-tunnel development site (UNRESOLVED)

1. Bad behavior
   1. The local repository file `website/src/pages/Vip.jsx` contains the prepaid slider markup at lines 191-202, including `input type="range"`, `min="1"`, `max="12"`, and `step="1"`.
   2. The browser page opened through `website/npm-run-dev-ssh-v2.bat` shows only the monthly subscription buttons. It does not show the prepaid slider, prepaid summary, or lifetime purchase button.
   3. The user can see the source around line 190 as JSX closing syntax (`)}`), but the running page does not correspond to the current source file.
2. Date and time first observed: 2026-09-07, local development session.
3. Current evidence and likely boundary
   1. `website/npm-run-dev-ssh-v2.bat` starts an SSH tunnel with `ssh -L 3002:localhost:3002 root@107.191.48.226` and separately starts local Vite with `npm run dev`.
   2. The local React page calls `/api/vip/config`; the slider is conditionally rendered only when the API returns a purchase with `type: "prepaid"`.
   3. The VPS API can therefore supply an older configuration even when the local Vite bundle contains the newer `Vip.jsx`, especially because the API changes have not yet been deployed to the VPS.
   4. A previous regression on 2026-09-06 confirmed the same class of failure: source/frontend changes were invisible because the deployed `dist/` bundle had not been rebuilt. That entry is `docs/regressions/regressions-v1.md`, “Profile stats not visible on live site.”
4. Status: UNRESOLVED — do not mark fixed until the browser network response for `/api/vip/config`, the served Vite/bundle source, and the API/frontend deployment commit are compared and the slider is visibly accepted in the browser.
5. Required investigation
   1. Confirm which process owns ports 5173 and 3002 and whether the browser is actually at `localhost:5173`.
   2. Inspect the browser response from `/api/vip/config` and confirm it includes `prepaid` and `lifetime` for all three tiers.
   3. Confirm Vite is serving `C:\mimita-priv-v8\website\src\pages\Vip.jsx`, not a different checkout or stale `dist/` directory.
   4. If the API is the old VPS version, deploy only after confirming the exact branch and commit, then rebuild the frontend and restart the relevant service.
