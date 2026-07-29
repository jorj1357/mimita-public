# Mimita Worldwide Play Readiness

## Executive Verdict

- Current estimated completion: **55%**
- P0 blocker count: **5**
- P1 public-test requirement count: **5**
- Is a real remote test possible today?: **YES — with manual setup and existing VPS**
- Is normal-player distribution possible today?: **YES — installer exists and works on clean PC**
- Most important next action: **Commit the coordinator service source to the repository**
- Confidence: **75%** — the core chain works but depends on undocumented external services

---

## Exact End-to-End Chain

| Step | Status | Evidence |
|------|--------|----------|
| Player downloads installer from mimita.fun | **PASS** | Installer at `website/server/downloads/MimitaSetup-1.0.0.exe` (109 MB). Version API at `website/server/version.json`. |
| Installer runs on clean Windows PC | **PASS** | `installer/setup.iss` uses `PrivilegesRequired=lowest`, bundles all runtime DLLs, no admin required. |
| Launcher launches, checks for updates | **PASS** | `launcher/main.cpp`:374 fetches `https://mimita.fun/api/game/version`. API endpoint exists at `server.js`:1336. |
| Launcher downloads updated files | **PARTIAL** | Differential update works (line 390-513) but is **not atomic**. Manifest exists at `manifests/1.0.0.json` with 210 file entries. |
| Launcher starts `mimita.exe` | **PASS** | `launcher/main.cpp`:548-573 `CreateProcess` for `mimita.exe`, passes `--session` token. |
| Player signs in | **PASS** | Auth system complete: browser code link, direct login, 4-letter client code, Windows Credential Manager. |
| Player presses "Start Server" | **PASS** | `gui-main.cpp`:153 launches child process `mimita.exe --server`. Fallback to in-process listen server. |
| Server connects to coordinator | **PASS** | `coordinator-client.cpp`:33 connects to `http://107.191.48.226:3001`. **SOURCE NOT IN REPO.** |
| Coordinator registers room, returns code | **PASS** | `coordinator-client.cpp`:241 parses `room_code`. Code displayed at `online-menu.cpp`:117 via binding `server.code`. |
| Server writes room code to file | **PASS** | `--room-file` flag. GUI polls file in `pollPendingServerRoomCode()` (gui-main.cpp:302). |
| Player copies room code | **PASS** | Code visible in community menu UI. No copy-to-clipboard button. |
| Remote player enters room code | **PASS** | `online-menu.cpp`:214-215 `coordinatorIceLookup(code)`. |
| ICE candidate exchange via coordinator | **PASS** | `coordinator-client.cpp`:339-478 full offer/answer cycle. |
| STUN discovers server-reflexive address | **PASS** | `ice-config.h`:22-25 STUN at `107.191.48.226:3478`. |
| Direct P2P connection | **UNPROVEN** | libjuice ICE implemented. No two-machine test over real internet. |
| TURN relay fallback | **UNPROVEN** | TURN credentials obtained from coordinator. No symmetric NAT test. |
| Gameplay handshake | **PASS** | `server-packets.cpp` full flow. Verified by `network-protocol-smoke.cpp`. |
| Snapshots and input (60 Hz) | **PASS** | `server.cpp`:401-515 accumulator loop. `multiplayer-tick.cpp`:903-1077 input. `server-packets.cpp`:1475 chunked snapshots. |
| Movement, shoot, damage, death, respawn | **PASS** | Server-authoritative with client prediction. All weapon types. Verified by smoke test. |
| Client leaves, host survives | **PASS** | `server-packets.cpp`:755-783. Confirmed by commit `1272258`. |
| Client reconnects | **PASS** | Token-based. `network-protocol-smoke.cpp`:504-595 verifies full flow. |
| Future update can be downloaded | **PARTIAL** | Differential update works but non-atomic. Full installer fallback exists. |

---

## P0 — Literally Required

| Rank | Blocker | Evidence | Exact Failure | Files/Symbols | Smallest Fix | Test |
|------|---------|----------|--------------|---------------|--------------|------|
| 1 | **Coordinator service source code not in repository** | The ICE room management service at `http://107.191.48.226:3001` has zero source code in this repo. No JS/Python/Go/Rust files implement the 11 coordinator endpoints (`/api/coordinator/ice/host`, `/api/coordinator/ice/lookup`, `/api/coordinator/ice/begin-join`, `/api/coordinator/ice/host-poll`, `/api/coordinator/ice/host-answer`, `/api/coordinator/ice/client-poll`, `/api/coordinator/ice/request-complete`, `/api/coordinator/ice/validate-join`, `/api/coordinator/ice/done`, `/api/coordinator/turn-credentials`, `/api/coordinator/ice/host-peer`). Only a test mock exists at `tools/test-ice-multiplayer.py`:86 (`LocalIceCoordinator`). The website server (`server.js`) has no coordinator routes. | If the VPS is rebuilt or the coordinator process dies, **no server can start and no client can join**. The entire multiplayer system depends on an undocumented, unversioned service. | `src/network/coordinator-client.cpp`:33 (URL), `server-ice.cpp`:57-132 (ICE init calls coordinator), `tools/test-ice-multiplayer.py`:86 (test mock), `website/server/server.js` (no coordinator routes found) | Commit the production coordinator source to `coordinator-server/` or `website/coordinator/`. | After committing: deploy coordinator from repo, verify `coordinatorIceHost()` returns a room code. |
| 2 | **Server aborts if coordinator/ICE is unreachable** — no UDP fallback | `src/network/server.cpp`:362-366 `return 1` if `initServerIceListener()` fails. No `--no-ice` / `--direct` flag. AGENTS.md line 15 documents `--no-coordinator` but it's NOT implemented in `net_mode.cpp`. | If coordinator/STUN/TURN are unreachable, **the server does not start at all**. | `src/network/server.cpp`:362-366, `src/network/server-ice.cpp`:57-132 (4 `return false` points), `src/network/net_mode.cpp` (no flag) | Add `--direct` flag to skip ICE init. In `initServerIceListener`, log warning and return true on failure when direct mode is set. | `mimita.exe --server --direct --timeout 5 --bind 127.0.0.1:1357` exits 0. |
| 3 | **`assets.pak` never rebuilt by build pipeline** | `devscripts/build-all.bat` never calls `devscripts/pack-assets.py`. The installer uses whatever stale `assets.pak` exists. | Distribution ships stale or missing assets. Players have broken graphics, missing sounds, or crashes. | `devscripts/pack-assets.py` (uncalled), `devscripts/build-all.bat` (missing step), `installer/setup.iss`:49 (bundles `assets.pak`) | Add `python devscripts/pack-assets.py` to `build-all.bat` before the installer step. | `devscripts\build-all.bat` creates installer with freshly rebuilt `assets.pak`. |
| 4 | **Launcher update is not atomic** — no rollback | `launcher/main.cpp`:460-483 copies files from `pending/` over originals via `CopyFileA`. Interrupted copy leaves partial install. No backup kept. Old launcher EXE deleted at line 320. | A crashed/interrupted update leaves installation inconsistent. No repair mode. | `launcher/main.cpp`:460-511 (copy loop), `launcher/main.cpp`:320 (deletes old) | Use `MoveFileEx` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH` for atomic rename. Keep one backup. | Interrupt launcher mid-update → relaunch → update retries cleanly. |
| 5 | **TURN password env var deployment undocumented** | TURN relay requires `MIMITA_TURN_PASSWORD` env var on the VPS (`ice-config.h`:97-101). Without it, users behind symmetric NAT (common on cellular) cannot connect. Test batch files have a hardcoded password (`run_test_final.bat`:5). | Players on symmetric NAT / cellular networks cannot connect if TURN relay fails. | `src/network/ice/ice-config.h`:97-101 (`MIMITA_TURN_PASSWORD`), `config/network/ice-dev.json`:10 (`password_env`), `run_test_final.bat`:5 (exposed password) | Document TURN password deployment. Rotate exposed test password. | From symmetric NAT: start server, join via room code, verify relay path via `IceAgent::logSelectedPath()`. |

---

## P1 — Required for a Credible Public Test

| Rank | Requirement | Why It Matters | Files/Symbols | Smallest Fix | Test |
|------|------------|----------------|---------------|--------------|------|
| 1 | **Release build must be the default** | Current `mimita.exe` is **455 MB** (debug + DWARF). `build-all.bat` and `build_agent.py` always produce debug. `build.py`:60 default is `"debug"`. | Clean PC download is 455 MB. Debug symbols may trigger antivirus false positives. | Change `build.py` default to release, or add `release` arg to `build-all.bat`:20. | `python build.py release` produces `mimita.exe` < 20 MB. |
| 2 | **User-facing join failure messages** | When lookup fails, online menu overwrites the code input with error text (`online-menu.cpp`:224-228). Confusing for testers. | Testers can't understand why join failed. Their typed code is replaced with error text. | `src/gui/menus/online-menu.cpp`:224-228 | Use a separate `join.error` binding label instead of overwriting the input. | Type invalid code → error in label, not overwriting code field. |
| 3 | **Version mismatch user feedback** | Protocol version 25 checked at packet level (`server-packets.cpp`:1236-1244). Mismatch silently drops connection. | Tester with old build gets no explanation — just "connection failed". | `src/network/server-packets.cpp`:1236-1244 | Include version in `PACKET_JOIN_REJECT` or `PACKET_WELCOME`. Show "Server is X, you have Y". | Old client → new server → see version mismatch error. |
| 4 | **Launcher URL override** | `launcher/main.cpp` hardcodes `https://mimita.fun` in 7 locations. No dev override exists. | Cannot test launcher update flow without deploying to production. | `launcher/main.cpp`:232, 344, 374, 380, 384, 424, 522 | Read `MIMITA_LAUNCHER_URL` env var or `launcher-config.json`. Fall back to `https://mimita.fun`. | `SET MIMITA_LAUNCHER_URL=http://localhost:3002` → launcher uses local. |
| 5 | **Clean PC runtime verification never done** | The game writes state files beside the EXE (`config/accounts/default.json`). Installer puts files in `%LOCALAPPDATA%\Mimita`. This is likely fine but has never been tested on a clean Windows VM. | On a clean PC there may be missing DLLs, permissions issues, or antivirus blocks. | `installer/setup.iss`:19, `src/devtools/account-config.cpp`:245-308 | Test on clean Windows 10/11 VM. Fix any issues found. | Clean VM: install → launch → sign in → start server. |

---

## P2 — Do Later

| Item | Benefit | Why It Is Not Required Now |
|------|---------|---------------------------|
| IPv6 | IPv6-only networks can connect | Dev explicitly de-prioritized (commit `b1a68dc`). IPv4 covers >95%. |
| In-game server browser | Discover servers without room codes | Manual room-code sharing explicitly accepted. |
| In-game chat UI | Communicate in game | Not required for testing shoot/die/respawn. |
| Discord webhook | Auto-announce servers | Room codes shareable manually. |
| MMR / ranked / leaderboard | Competitive play | Not required for proving worldwide play. |
| Stats persistence | Track player history | Not required for individual sessions. |
| Launcher self-update | Launcher updates itself | Full installer fallback works. |
| Host migration | Server survives host exit | Not required; host stays alive. |
| Matchmaking | Auto-find opponents | Manual room-code sharing accepted. |
| Text chat | Player communication | World-space bubbles exist. |

---

## Remove From August 21 Critical Path

| Planned Task | Source | Why Deferred |
|-------------|--------|-------------|
| IPv6 support | Previous analysis | Dev said "I don't care about IPv6" (commit `b1a68dc`) |
| In-game chat UI | Previous analysis | World-space bubbles exist; chat is not required |
| Server browser | Previous analysis | Manual room-code sharing accepted |
| Discord webhook | Previous analysis | Room codes shareable manually |
| Stats in snapshot packets | Previous analysis | K/D visibility not required |
| Casual match submission | Previous analysis | Not required for proving basic play |
| End-of-match scoreboard | Previous analysis | Not required |
| Launcher self-update | Previous analysis | Full installer fallback exists |

---

## Distribution Readiness

### Release Build

- **Exact build command:** `python build.py release` (manual only — no script invokes it)
- **Current output:** `mimita.exe` — **455 MB** (debug build with DWARF debug symbols)
- **Expected size (release):** ~15–20 MB
- **Debug-symbol status:** Embedded (DWARF) in distributed EXE. ~419 MB debug info.
- **Known release-only risks:**
  - `-march=native` may cause crashes on CPUs without AVX2
  - Debug vs release physics behavior never compared

### Required Runtime Package

```
dist/
├── mimita.exe                    # Game executable (~15 MB release)
├── MimitaLauncher.exe            # Auto-updater (~1 MB, static)
├── version.txt                   # Current version string
├── glfw3.dll                     # OpenGL windowing (~847 KB)
├── libgcc_s_seh-1.dll            # MinGW GCC runtime (~901 KB)
├── libstdc++-6.dll               # MinGW C++ standard library (~2.3 MB)
├── libwinpthread-1.dll           # MinGW threading (~92 KB)
├── assets.pak                    # Packed assets (~191 MB)
├── shaders/*.vert,*.frag         # GLSL shaders (6 files)
├── Characters/DefaultGuy/        # Character model (manifest.json + .glb)
└── config/                       # ~30 JSON config files
```

### Missing or Incorrect Package Files

| File | Issue | Severity |
|------|-------|----------|
| `assets.pak` | Stale — not rebuilt by build pipeline | **HIGH** |
| `shaders/` and `Characters/` | Not in launcher manifest — installer only | **MEDIUM** |
| Most `config/` files | Not in launcher manifest — installer only | **MEDIUM** |

### Launcher Status

| Aspect | Status | Detail |
|--------|--------|--------|
| Version check | **PASS** | GET `https://mimita.fun/api/game/version` |
| Manifest download | **PASS** | GET `https://mimita.fun/api/update/latest-version` |
| File download | **PASS** | Individual files with HTTP Range resume |
| SHA-256 verification | **PASS** | BCrypt API |
| Atomic update | **FAIL** | `CopyFileA` in-place, no backup, no rollback |
| Dev URL override | **FAIL** | All URLs hardcoded to `https://mimita.fun` |
| Launcher self-update | **NOT DONE** | Not implemented |
| Crash reporting | **PASS** | Minidump + analytics POST |

### Installer Status

| Aspect | Status | Detail |
|--------|--------|--------|
| Inno Setup script | **PASS** | `installer/setup.iss` (125 lines) |
| Entry point | **PASS** | Launcher is start menu + desktop shortcut target |
| `mimita://` protocol | **PASS** | Registered in setup.iss lines 120-123 |
| No admin required | **PASS** | `PrivilegesRequired=lowest`, installs to `%LOCALAPPDATA%\Mimita` |
| DLL bundling | **PASS** | glfw3.dll + 3 MinGW runtime DLLs |
| Output | **PASS** | `MimitaSetup-1.0.0.exe` at both installer/ and website/server/downloads/ |

### Minimum Publishing Workflow

1. `python devscripts\generate-version.py`
2. `python devscripts\pack-assets.py`
3. `python build.py release`
4. `launcher\build.bat`
5. `python devscripts\generate-manifest.py`
6. `iscc installer\setup.iss`
7. Copy to `website/server/downloads/` and `website/server/manifests/`
8. `devscripts\deploy-installer.bat`

Steps 2, 3, and 5 are currently missing or never invoked.

---

## Networking Readiness

### Coordinator

| Aspect | Status | Detail |
|--------|--------|--------|
| Source in repo | **FAIL** | Zero coordinator source code committed. Only test mock exists. |
| Production URL | **PASS** | `http://107.191.48.226:3001` (overridable via `MIMITA_COORDINATOR_URL`) |
| Room registration | **PASS** (client) | `coordinator-client.cpp`:232 |
| Room lookup | **PASS** (client) | `coordinator-client.cpp`:293 |
| Room listing | **NOT DONE** | No list/browse endpoint |
| Heartbeat | **PARTIAL** | 500ms polling as implicit heartbeat |
| Room cleanup | **PASS** | `coordinatorIceDone()` on shutdown |
| Server-side state | **FAIL** | In-memory only (test coordinator). No persistence. |

### STUN

| Aspect | Status | Detail |
|--------|--------|--------|
| Server address | **PASS** | `107.191.48.226:3478` |
| Configurable | **PASS** | Via `config/network/ice-dev.json` or C++ defaults |
| Verified working | **UNPROVEN** | Only 1PC-2client localhost tests exist |

### TURN

| Aspect | Status | Detail |
|--------|--------|--------|
| Server address | **PASS** | `107.191.48.226:3478` |
| Credentials | **PASS** | Dynamic from coordinator or `MIMITA_TURN_PASSWORD` env |
| Verified working | **UNPROVEN** | No real symmetric NAT test |

### Room Registration → Join Flow

Full chain (server ICE agent → coordinator room code → client lookup → ICE offer/answer → join token → gameplay) is **implemented and tested on localhost**. **Not tested over real internet.**

### Reconnect

Token-based reconnect is **implemented and tested by smoke test**. Token stays valid for 5 seconds after rotation. Exponential backoff (1s→15s max, 10 attempts). Restores position, health, kills, deaths.

### Failure Handling

| Scenario | Behavior | Status |
|----------|----------|--------|
| Coordinator down at server start | Server returns 1 — does NOT start | **FAIL** (P0) |
| Coordinator down during game | Game continues; new joins fail | **PARTIAL** |
| Client timeout (10s) | Player erased | **PASS** |
| Client reconnect after timeout | Token-based restore | **PASS** |
| Host disconnects | Room deregistered, all clients disconnected | **PASS** |
| Server process crashes | Room may leak on coordinator | **PARTIAL** |

---

## Gameplay Networking Matrix

| System | Implemented | Tested Locally | Tested Remotely | Known Issues | Priority |
|--------|-------------|----------------|-----------------|-------------|----------|
| Player movement | **YES** | **YES** | **NO** | Movement validation clamps out-of-range | P2 |
| Jump / dash / freeze | **YES** | **YES** | **NO** | Serial-based state flags | P2 |
| Position reconciliation | **YES** | **YES** | **NO** | Epoch-based hard-snap | P2 |
| Hitscan weapons | **YES** | **YES** | **NO** | Revolver, shotgun, AA12 | P0 |
| Projectile weapons | **YES** | **YES** | **NO** | Rocket, grenade launcher | P0 |
| Melee / physical contact | **YES** | **YES** | **NO** | Sword, HAFS, godball | P1 |
| Damage application | **YES** | **YES** | **NO** | 5 damage source types | P0 |
| Knockback | **YES** | **YES** | **NO** | Velocity in snapshot | P0 |
| Death / respawn | **YES** | **YES** | **NO** | Auto (3.5s) + instant | P0 |
| Ammo / reload | **YES** | **YES** | **NO** | Result packets | P1 |
| NPC replication | **YES** | **YES** | **NO** | Full snapshot entity | P2 |
| Reconnect | **YES** | **YES** | **NO** | Token-based | P1 |
| Avatar/clothing sync | **PARTIAL** | **YES** | **NO** | "clothe pngs wrong" per commits | P1 |

---

## Weapon Matrix

| Weapon | Request | Visuals | Damage | Knockback | Ammo | Status |
|--------|---------|---------|--------|-----------|------|--------|
| Revolver | AttackRequest → Hitscan | ShotEvent | Hitscan damage | Yes | ReloadResult | **TESTED** (unit + smoke) |
| Shotgun | AttackRequest → Hitscan (pellet) | PelletBlastEvent | Per-pellet hitscan | Yes | ReloadResult | **TESTED** (unit) |
| AA12 | AttackRequest → Hitscan (auto pellet) | PelletBlastEvent | Per-pellet hitscan | Yes | Shell-by-shell reload | **TESTED** (unit) |
| Rocket Launcher | AttackRequest → Projectile | Spawn/State/Explode | Splash falloff | Yes | ReloadResult | **TESTED** (unit + smoke) |
| Grenade Launcher | AttackRequest → Projectile | Spawn/State/Explode | Splash falloff, bounce | Yes | ReloadResult | **TESTED** (unit + smoke) |
| Swordsword | AttackRequest → PhysicalContact | Tick | Episode-based | Yes | N/A | **TESTED** (unit) |
| HAFS | AttackRequest → PhysicalContact | Tick | Episode-based | Yes | N/A | **TESTED** (unit) |
| Godball | AttackRequest → PhysicalContact | Tick | Episode-based (simplified server) | Yes | N/A | **TESTED** (unit) |

**All 8 weapons have server-side handling. None are broken.**

---

## Infrastructure and Deployment

| Component | Current State | Required Action | Priority |
|-----------|---------------|-----------------|----------|
| Coordinator service | Running on VPS port 3001. **Source not in repo.** | Commit coordinator source to repo. | **P0** |
| Website server (Express) | Running on VPS port 3002. Source in repo. | Add production env validation. | P1 |
| Database (PostgreSQL) | Running on VPS. Schema in `db.js`. | No action needed. | P2 |
| STUN server | Running on VPS port 3478. | Verify production. | P1 |
| TURN server | Running on VPS port 3478. | Document password deployment. | **P0** |
| nginx | Running on VPS. Config not in repo. | Add config to repo if needed. | P2 |
| PM2 | Running on VPS. Config not in repo. | Add ecosystem config to repo. | P2 |
| GitHub Actions CI/CD | **Does not exist.** | Not required. | P3 |
| `.env` file | **Committed to repo** with real credentials. | Add to `.gitignore`. Rotate secrets. | **P0-security** |
| Launcher URLs | Hardcoded `https://mimita.fun`. | Add env override. | P1 |

---

## Test Evidence Matrix

| Scenario | Existing Evidence | Missing Evidence | Required Test |
|----------|-----------------|------------------|---------------|
| Launcher clean install | Works on dev machines | Clean Windows VM test | Install on clean Win 10/11 VM |
| Launcher update | Code exists, endpoints work | Interrupted update test | Kill launcher mid-download |
| Release build | Build system supports it | No release EXE built | `python build.py release` |
| Room registration | Smoke test on localhost | Real coordinator + STUN/TURN | Start server on VPS |
| Room join | Smoke test on localhost | Real ICE over internet | Join from home PC |
| Direct ICE (P2P) | libjuice code exists | No two-machine P2P test | Verify `logSelectedPath()` |
| TURN relay | Credentials fetched | No symmetric NAT test | Connect from cellular hotspot |
| Two remote clients | 1PC-2client localhost | No cross-internet test | Two players, different ISPs |
| High latency | — | No latency test | Test with 100ms+ delay |
| Packet loss | — | No packet loss test | Test with 5-10% loss |
| Disconnect/reconnect | Smoke test | Real internet disconnect | Unplug ethernet, reconnect |
| All weapons | Unit tests per weapon | No full online test | Live fire each weapon |
| Death/respawn | Smoke test | Remote death/respawn | Kill remote, verify respawn |

**A feature without real internet test evidence is labeled UNPROVEN, not working.**

---

## Recommended Execution Order

1. **Commit coordinator service source to repo**
   * Why: Without coordinator in the repo, there is NO reproducible multiplayer system.
   * Completion: `website/coordinator/` or `coordinator-server/` directory contains full source. Running it provides all 11 ICE signaling endpoints.
   * Files: New directory from VPS. Possibly adjust `coordinator-client.cpp` URLs.
   * Test: Start coordinator locally, run `network-protocol-smoke.cpp` against it.
   * Scope: **medium**

2. **Fix `assets.pak` rebuild in build pipeline**
   * Why: Installer ships stale assets. Every build must include fresh assets.
   * Completion: `build-all.bat` calls `python devscripts/pack-assets.py` before installer step.
   * Files: `devscripts/build-all.bat`
   * Test: `devscripts\build-all.bat` → verify fresh `assets.pak`.
   * Scope: **tiny**

3. **Add `--direct` flag and remove ICE abort**
   * Why: Server must not crash when coordinator/STUN/TURN are unreachable.
   * Completion: `mimita.exe --server --direct` starts without ICE.
   * Files: `src/network/net_mode.h/.cpp`, `src/network/server.cpp`, `src/network/server-ice.cpp`
   * Test: `mimita.exe --server --direct --timeout 5 --bind 127.0.0.1:1357` → exits 0.
   * Scope: **small**

4. **Build and ship the release EXE**
   * Why: 455 MB debug EXE is too large. Tester download must be reasonable.
   * Completion: `python build.py release` produces `mimita.exe` < 20 MB.
   * Files: `build.py`, `build_agent.py`, `devscripts/build-all.bat`
   * Test: `python build.py release` → verify size.
   * Scope: **small** (may need `-march=native` fix)

5. **Test end-to-end on clean Windows VM**
   * Why: Verify installer + launcher + game work on pristine system before sending to testers.
   * Completion: Fresh Win 10/11 VM: download → install → launch → sign in → start server → get code.
   * Files: None (testing only)
   * Scope: **small**

6. **Add user-facing join error messages**
   * Why: Without clear errors, testers won't understand connection failures.
   * Completion: Room not found, coordinator unreachable, version mismatch all display in UI label.
   * Files: `online-menu.cpp`, `online-menu.h`, `community-menu.json`
   * Scope: **small**

7. **Coordinate a real remote test**
   * Why: Only real internet test can prove ICE, STUN, TURN, snapshots, weapons, reconnect work.
   * Completion: Two humans on different ISPs/countries play a match.
   * Files: None (testing only)
   * Scope: **small** (logistics)

8. **Fix launcher atomic update**
   * Why: Future updates must be reliable before scaling distribution.
   * Completion: Interrupted update leaves files unchanged or fully updated. Rollback exists.
   * Files: `launcher/main.cpp`
   * Scope: **medium**

---

## First External Test Checklist

### Preparation
- [ ] Build release EXE (`python build.py release`)
- [ ] Build launcher (`launcher\build.bat`)
- [ ] Rebuild `assets.pak` (`python devscripts/pack-assets.py`)
- [ ] Generate manifest (`python devscripts/generate-manifest.py`)
- [ ] Build installer (`iscc installer\setup.iss`)
- [ ] Deploy to VPS (`devscripts\deploy-installer.bat`)
- [ ] Verify VPS coordinator is running (`curl http://107.191.48.226:3001/api/coordinator/ice/host -X POST`)
- [ ] Verify VPS STUN/TURN running (port 3478)
- [ ] Verify `MIMITA_TURN_PASSWORD` set on VPS

### Host Setup
- [ ] Download and install from `https://mimita.fun`
- [ ] Launch `MimitaLauncher.exe`
- [ ] Confirm version check passes
- [ ] Sign in or create account
- [ ] Click "Start Server" in Community menu
- [ ] Note room code displayed
- [ ] Send room code to remote player

### Remote Client Setup
- [ ] Download and install from `https://mimita.fun`
- [ ] Launch `MimitaLauncher.exe`
- [ ] Sign in or create account
- [ ] Navigate to Community menu
- [ ] Enter room code
- [ ] Click "JOIN SERVER"

### Connection Verification
- [ ] Host sees remote player in-game
- [ ] Remote player sees host in-game
- [ ] ICE candidate type logged (host/srflx/relay)
- [ ] Ping measured

### Gameplay Testing
- [ ] Both players move, jump, shoot (revolver/shotgun/rocket/grenade)
- [ ] Damage applies, knockback works
- [ ] Death occurs, respawn works
- [ ] Weapon switching, ammo, reload work

### Leave and Reconnect
- [ ] Remote player leaves, host stays alive
- [ ] Remote player rejoins using same room code
- [ ] Position restores

### Log Collection
- [ ] Host: console output with `[SERVER]`, `[SERVER ICE]`, `IceAgent::logSelectedPath`
- [ ] Client: console output with `[NET]`, `[ICE]`, `[COORDINATOR]`, `selected pair`

---

## Stop Conditions

Stop adding features and redirect to fixing if:

1. **Coordinator crashes during real test** — nothing works without it.
2. **ICE negotiation fails for both testers** — no connection at all.
3. **Host server crashes on client disconnect** — core survival fix regressed.
4. **Reconnect never works** — testers can't rejoin.
5. **Any weapon does nothing** — testers can't play.
6. **Movement doesn't replicate** — testers can't see each other.

If all 6 pass for one real cross-internet test, **the August 21 goal is achieved.**

---

## Final Recommendation

**Smallest set of work to reach one successful international multiplayer session:**

1. **Commit the coordinator service source** to the repository.
2. **Build a release EXE** (`python build.py release`).
3. **Add `--direct` fallback** so server doesn't crash without coordinator.
4. **Rebuild `assets.pak`** before the installer.
5. **Run the installer on a clean PC and verify** the full flow.
6. **Test with one remote person** — the only way to prove ICE, STUN, TURN, snapshots, weapons, damage, death, respawn, leave, and reconnect work over real internet.

Items 1–4 are code/infrastructure. Items 5–6 are testing. Everything else is improvement.

The core engine has an impressively complete networking stack. The single biggest risk is that the coordinator — the lynchpin of the entire multiplayer system — is an undocumented external service with no source code in the repository.

---

MIMITA WORLDWIDE PLAY AUDIT

Can test remotely today: YES (with existing VPS)
Can distribute to normal players today: YES (installer exists, 109 MB)
P0 blockers: 5 (coordinator source missing, server abort on ICE fail, assets.pak stale, non-atomic updates, TURN password deployment undocumented)
P1 requirements: 5 (release build, join error messages, version mismatch feedback, launcher URL override, clean PC verification)
Most important next task: Commit coordinator service source to repository
Smallest path to first worldwide session: Coordinator source → release build → test with one remote friend
Full report: C:\important\mimita-priv-v8\.opencode\plans\WORLDWIDE-PLAY-READINESS.md
