# Payload on Mimita Pipes join crash: use-after-free found and fixed (build pending)

- Task: Investigate the reported crash joining a Payload server on `mimita-pipes`, make false conditions loud, and add an issue.
- Agent: `rocket-league-aerial`
- Issue: `issue-c9b65fb9948e` (canonical; `issue-a1d9a695459d` is a duplicate)
- Date: 2026-11-28 UTC
- Status: Fix built and published as **generation 809**. Root cause proven on the pre-fix gen-808 control (independently reproduced). End-to-end gen-809 network-join proof still pending a harness fix (see below).

## Report (verbatim, already preserved in ctx-9543f5ce1f70)

> can u invstigate why crash when joining paload server on mimita pipes map and make it so its super loud obvious why false suggest how and add issue for that

## Evidence: this is a real client crash at join time

`evidence-watch.py list --json` showed the user's Payload servers on `mimita-pipes`
(PID 17456 started 06:09:03Z; PID 3280 started 06:16:06Z) with `source=unverified`
and no journal. The matching managed run is:

- Run `20261128_054617_01`, build `2026-11-27T17:45:19.700118Z:758a748d...`, generation 808.
- client-01 PID 12028 exited `3221225477` (0xC0000005) at `2026-11-28T06:09:37.117Z`.
- client-02 PID 24360 exited `3221225477` at `2026-11-28T06:10:10.218Z`.
- Dumps: `.dev/crashes/20261128_054617_01/crash-2026-11-28_01-09-29-12028.{txt,dmp}` and
  `crash-2026-11-28_01-10-01-24360.{txt,dmp}`.
- Payload server PID 17456 has parent PID 12028 (the crashed client), confirming the
  user joined from that client session.

Both dumps are identical: `ACCESS_VIOLATION (0xc0000005)`, read at `0xffffffffffffffff`,
top frame `MultiByteToWideChar+0xd7`.

## Symbolized stack (game-g808.dll DWARF, addr2line)

Using `GetMimitaWorldRenderingModuleAPI` RVA `0xa8e59b` and `GetGameAPI` RVA `0x2f7dc6`
from the DLL export table:

```
MultiByteToWideChar
UTF8ToWchar                         include/tinygltf/tiny_gltf.h:2890
tinygltf::ReadWholeFile             include/tinygltf/tiny_gltf.h:3170
std::_Function_handler::_M_invoke
std::function::operator()
tinygltf::TinyGLTF::LoadBinaryFromFile   include/tinygltf/tiny_gltf.h:6955
modelLoadThreadFunc                 src/entities/player-loader.cpp:1975
Player::requestModelLoad::<lambda>  src/entities/player-loader.cpp:2025
invokeBox                           src/game/job-bridge-module.cpp:22
```

So the process was reading a **corrupt std::string filename** in tinygltf, not failing
in Payload/map logic.

## Root cause (first incorrect step)

`src/entities/player-loader.cpp::Player::requestModelLoad` stored the sole owning
`std::shared_ptr<PendingPlayerModel>` in `mPendingModel` (a `Player` member), then
launched a detached job with a **non-owning raw pointer**:

```cpp
mPendingModel = data;
PendingPlayerModel* raw = mPendingModel.get();
mimita::game::runDetached([raw] { modelLoadThreadFunc(raw); }, "player-model-load");
```

`Player` has the default destructor, so join-time actor/scene churn frees the
`Player` and its `mPendingModel` while the detached thread still runs.
`modelLoadThreadFunc` then reads `d->resolvedPath` from freed memory; the garbage
string pointer reaches `MultiByteToWideChar`, which reads `0xffffffffffffffff`.

Corroboration: `src/combat/weapon-model-cache.cpp:146` already captures the owning
shared_ptr (`[asset]`) for the identical pattern, so player-loader was the outlier.

## Change applied

`src/entities/player-loader.cpp`:

1. Detached job now captures the owning `shared_ptr` so `PendingPlayerModel` outlives
   the thread: `runDetached([data] { modelLoadThreadFunc(data.get()); }, ...)`.
2. Loud, once-per-load `player.model.load.invalid-path` IMPORTANT event with
   `condition`, `expected`, `actual`, `owner`, and an actionable `suggestion` when the
   caller passes an empty path (in `requestModelLoad`) or the resolved path is
   empty/missing (in `modelLoadThreadFunc`, which now refuses it instead of feeding it
   to tinygltf). Normal loads are unaffected.

Build note `9dd132be9fa2`; build request `build-request-59479dc50ebe`.

## Build / proof status

- Build request `build-request-59479dc50ebe` auto-approved (`no_peers`) once host CPU
  dropped below the gate. Published **generation 809** (`build\game\generation-809\game-g809.dll`,
  attempt `game-dll-20261128T063600310452Z-db408b`): 48.7s total (compile 29.9,
  link 5.99, publish 0.35), 2 compiled / 645 skipped, no invalidations. Postmortem
  largest cause was host resource pressure (mean CPU 89.6%, peak 99.7%, min free
  2664 MB); the player-loader TU alone took 29.3s.
- Root cause corroborated independently: `rocket-league-crashfix` reproduced the
  gen-808 client `ACCESS_VIOLATION` in `tinygltf` `UTF8ToWchar` from
  `modelLoadThreadFunc` in run `20261128_062046` (pre-fix control).
- Gen-809 runtime: **no process crash observed**. The gen-809 join attempt in isolated
  run `20261128_063715` started a healthy `mimita-pipes`/payload server but the join
  never completed: the client logged `network.join.map-required-missing`
  ("connected transport has no accepted player/map") and exited 1 — **no** `0xC0000005`
  and no `process.crash`.
- That join attempt is not a valid network proof because of a harness/config conflict,
  not the fix: launch mode 15 ("payload") injects client arg `--startup-command
  "start payload"` (a local host command) into the *joining* client, and the
  `tdm-validation` profile injects `--mode team_deathmatch --npcs 5` before the
  launch-mode `--mode payload --no-npcs`.
- Remaining proof step: server-only payload start on `mimita-pipes` (no tdm profile
  mode/npcs) plus a client `--connect ... --map mimita-pipes` with **no** local-start
  command, ideally with a second client to force remote-player model loads; assert
  Connected/mode-active, no `process.crash`, no exit `0xC0000005`.

## Changed files

- `src/entities/player-loader.cpp`: UAF fix + loud invalid-path diagnostics.
- `docs/changelog/2026-11-28/20261128-payload-pipes-join-crash-ufa-fix.md`: this record.

No live process was stopped, adopted, or killed. My own hung `addr2line` symbolization
processes were terminated.
