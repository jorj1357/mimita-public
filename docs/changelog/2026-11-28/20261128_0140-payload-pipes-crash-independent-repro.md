# Payload on Mimita Pipes join crash: independent reproduction and loudness review

- Task: Investigate the reported crash joining a Payload server on `mimita-pipes`,
  make false conditions loud, suggest how, and add an issue.
- Agent: `rocket-league-crashfix`
- Issue: `issue-c9b65fb9948e` (canonical; `issue-a1d9a695459d` deferred as duplicate)
- Date: 2026-11-28 UTC
- Status: Root cause independently reproduced and symbolized. Fix source landed and
  built (game generation 809) under `rocket-league-aerial`; runtime proof remains
  pending because the host was CPU-saturated during the verification attempt.

## Report (verbatim, preserved in ctx-9543f5ce1f70)

> can u invstigate why crash when joining paload server on mimita pipes map and make it so its super loud obvious why false suggest how and add issue for that

## Reproduction (pinned root dev-loop)

`dev-loop.py --start-test-run --test-profile tdm-validation --test-launch-mode 15 --test-map mimita-pipes`

- Run `20261128_062046`; EXE `2026-11-27T17:45:19.700118Z` sha
  `758a748d…096fdc`; game generation `808` code_hash `6443de75…f5ac1`.
- Server pid 27796 payload/`mimita-pipes` online; client pid 18776 connected and
  reached active mode (tick 1550), then crashed at `2026-11-28T06:21:43Z`.
- Crash: `ACCESS_VIOLATION 0xc0000005`, read at `0xa`, in
  `MultiByteToWideChar`/`RtlUTF8StringToUnicodeString`.
- Artifacts: `%LOCALAPPDATA%\MiMITA\crashes\crash-2026-11-28_01-21-43-18776.{txt,dmp}`;
  durable copies in `logs\2026-11-28\20261128_062046\`.

## Symbolized stack (addr2line, game-g808.dll)

```
MultiByteToWideChar
UTF8ToWchar                       include/tinygltf/tiny_gltf.h:2890
LoadBinaryFromFile / modelLoadThreadFunc
                                  src/entities/player-loader.cpp:1975/1997
game/job-bridge-module.cpp:22 invokeBox
host/host-runtime.cpp run         (detached player-model load thread)
```

The failing conversion operates on `PendingPlayerModel::resolvedPath`. The pinned
gen-808 snapshot still captured a raw `PendingPlayerModel*` in
`runDetached([raw]{ ... })`; server-join actor/scene churn frees the object while
the load is in flight, so tinygltf reads freed string storage. Fix (capture the
shared_ptr plus invalid-path guard) is now in the working tree and gen 809.

## Loudness observations

1. The crash report's Recent activity had only `process-identity`; the model-load
   thread records no breadcrumb, so the report never names the model/player and
   leans on unsymbolized ntdll offsets.
2. The crash handler shows a modal MessageBox but other threads keep simulating:
   the client journal continued ~65s after the crash report. A hidden dialog looks
   like "it just closed with no reason".
3. Recommended loud fixes: `recordCrashBreadcrumb` in `modelLoadThreadFunc`
   (resolved path, player, model key) plus the join/actor-spawn request; fail fast
   after a fatal write or show an in-game banner; include top game-DLL frame names
   in the crash text.

## Verification status

Build gen 809 (`src/entities/player-loader.cpp`) published 06:36:47Z. Verification
run `20261128_063715` reached ready but the client did not complete the join
(`network.join.map-required-missing`, no server `join.received`) under 96-100%
host CPU with three concurrent MiMITA servers; it did not crash but did not
exercise the join churn path. Not proof. Owner should rerun the exact command when
CPU is below 90% and assert join stages, a >=60s post-active-mode survival, and no
new crash artifact.
