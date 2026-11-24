# Gen584 build latency and header fan-out

## Evidence

Attempt `game-dll-20261124T065924976860Z-3ed108` completed generation 584 at
2026-11-24 07:10:45.454 UTC. It took 680.469 seconds for 16 changed paths,
with 258 translation units compiled and 387 skipped out of 644 sources. The
measured phase wall times were 43.156 seconds for changed-source preflight,
571.890 seconds for compilation, 25.766 seconds for LLD, 3.860 seconds for
object-cache scan, and 0.625 seconds for publish. The worker summary separately
reports 615.031 seconds of compile work; this is not the same metric as the
571.890-second phase wall time and should not be substituted for it.

The per-attempt timeline accounts for all 258 compiled units:

- 10 directly changed `.cpp` files;
- 247 other translation units invalidated by changed headers; none overlap the
  10 directly edited `.cpp` files;
- one generated `build/game/generation-584/module-identity-provider.cpp`.

The 247 header invalidations were attributed to `camera.h` (130),
`gui/ui-system.h` (78), `gamemode.h` (32), `render/post-fx.h` (4), and
`config/camera-config.h` (3). This explains why untouched `.cpp` files were
compiled: their included header timestamps were newer than their cached object
files.

The resource sidecar contains 212 samples. Mean host CPU was 99.437%, and
minimum available RAM was 29,274,112 bytes. Its runtime alignment is explicitly
unavailable because the server/client evidence did not match; these resource
figures do not prove the runtime pair caused the build delay.

## Interpretation and next work

The object-cache scan was under four seconds, so optimizing that scan alone
cannot meet the ten-second target. Header fan-out drove 247 rebuilds, while
compilation and the monolithic DLL link dominated this attempt. A separate
unchanged-header single-`.cpp` measurement is still needed to establish the
ordinary hot-edit baseline. Reaching sub-ten-second edits will also require a
smaller link boundary or hot behavior code that avoids native C++ compilation
and whole-game DLL linking; the existing hot-layer language/runtime bake-off
spec remains deferred until a runtime is selected.

The prior Juggernaut server/client pair is no longer available for activation
proof. The current unrelated Bloodtest runtime is preserved; no new process,
build, or capture was started during this investigation. The next proof must
use one owner-approved pinned Juggernaut pair, verify host DLL generation equals
NPC module generation, and capture build/resource/runtime evidence in the same
window.

Build details, exact invalidation counts, and the next discriminating check are
recorded in [issue-136d4553ff92](../../issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md).
