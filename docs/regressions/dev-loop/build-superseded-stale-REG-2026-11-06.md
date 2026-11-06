# Build superseded during concurrent edits

Time created: 2026-11-06T04:04:40Z (2026-11-05 23:04:40 EST)  
Time last updated: 2026-11-06T04:04:40Z

Status: OPEN — observability and workflow risk; final build succeeded

## Summary

During a two-agent session, the dev-loop build appeared to be "lost" and took
much longer than one compile. The build was not lost. It was repeatedly
superseded because two agents edited the same checkout while the dev-loop runs
one build at a time and queues another build when source changes during an
active build.

The published artifact at `2026-11-06T03:08:24.312691Z` (source generation 28)
became stale as later edits landed. The dev-loop correctly did not launch a
stale artifact. The final build at `2026-11-06T03:56:32.587910Z` (source
generation 92) succeeded, took 221.8 seconds, and was launched.

## Evidence

### Build lifecycle

- Source generation advanced from 28 to 92 during the session.
- The artifact at `2026-11-06T03:08:24.312691Z` was the newest successful build
  for part of the session, but later source changes made it stale.
- Final successful build:
  - artifact: `2026-11-06T03:56:32.587910Z`
  - SHA-256: `6dd25768311625072969940edc7b15a5aa91b688aebf7a6b3a04f63cc7b7ab91`
  - source generation: 92
  - duration: 221.7974109 seconds
  - dev-loop result: `published_current`, then launched
- `builds_match` became `true` after the final artifact was launched.

### Why the rebuild was large

The checkout had 68 dirty paths, including widely included headers:

```text
src/network/server.h
src/network/packets.h
src/entities/player.h
src/combat/weapon-execution.h
src/effects/hit-effects.h
src/effects/effect-part.h
```

The resulting object tree contained 618 objects totaling about 456 MB. Many
objects were rebuilt at 22:56 local time for the final artifact. A header change
in this set invalidates a large fraction of the executable.

### Dev-loop scheduling behavior

`devscripts/dev-loop.py` documents and implements this policy:

```text
one build at a time; edits during a build queue another build
```

Relevant code:

- `devscripts/dev-loop.py:4051`
- `devscripts/dev-loop.py:4058-4067`
- `devscripts/dev-loop.py:2498-2535`

The daemon increments the source generation on each detected change
(`devscripts/dev-loop.py:2453-2459`) and marks the latest published build stale
when it no longer matches the current generation.

### Transient failed compile

One failure window left `weapon-execution.cpp` without a valid object. The same
source later passed a syntax check and compiled successfully in the canonical
build. A direct `cc1plus` probe returned `0xC0000135` (DLL not found), but that
probe was run outside the canonical MinGW runtime environment and is not
evidence of the canonical build failure. The actionable evidence is the
successful rebuild of the same source.

### Not a build cause

The flashing terminal windows observed during the session were Git scans launched
by OpenCode/ChatGPT, not MiMITA compiler or dev-loop activity. They stopped when
OpenCode stopped scanning. Manual `python build.py` attempts while the dev-loop
owned the build lock also failed with `PermissionError`; the correct behavior is
to use the dev-loop's single build request path.

## Expected behavior

- A successful build should publish one artifact whose source generation matches
  the current source.
- The running game should only be replaced with a non-stale artifact.
- Build status should make it obvious that repeated source changes superseded an
  earlier artifact.

## Actual behavior

- Earlier artifacts were repeatedly superseded by new source generations.
- The final build succeeded and matched source, but the intermediate period
  looked like a lost or stalled build.
- No single runtime acceptance result was available until the final artifact
  launched.

## Workflow response

1. During multi-agent work, treat a rising `source_generation` as expected
   supersession, not a lost build.
2. Do not start a second build while the dev-loop owns the build lock.
3. Batch shared-header edits where practical; a change to `server.h`,
   `packets.h`, `player.h`, `weapon-execution.h`, `hit-effects.h`, or
   `effect-part.h` should be expected to trigger a near-full rebuild.
4. Use `python devscripts/dev-loop.py --status --json` and compare
   `source_generation`, `latest_successful_source_generation`, and
   `builds_match` before judging whether a build was lost.
5. Only run the launched artifact when `builds_match` is true.

## Open improvement

The dev-loop status currently exposes only a short build-output tail. A silent
compiler-process failure can therefore look like a failed build with no visible
diagnostic in status. A future improvement is to preserve the full compiler
stderr for the failed translation unit in the daemon run directory or build log.

## Follow-up 2026-11-06T04:35:35Z — disk exhaustion confirmed

The silent compiler failures were not a source error. At the failure point the
system drive had only about 113 MB free. A compiler process starting a new
translation unit could fail without emitting a diagnostic, leaving the expected
object missing. The same source later compiled and linked successfully.

Repair evidence:

```text
before: C: free ~113 MB
removed: build/ccache (generated cache) ~1.72 GB
removed: three superseded .dev/builds artifacts ~660 MB
after:  C: free ~2.45 GB
```

After freeing generated build space, the canonical build published:

```text
artifact: 2026-11-06T04:33:32.276873Z
build id: 2026-11-06T04:33:32.276873Z:4e1ea8eb9a8ef8493214c01931b85d98fc8b2761e70f5a88295454fbde260fd7
source generation: 9
result: published_current
```

The earlier direct `cc1plus` probe returning `0xC0000135` remains separate: it
was invoked outside the build's MinGW runtime path and is not evidence about the
canonical build. The actionable causes were source supersession plus low disk
space.


## Related documents

- `docs/specs/dev-loop/dev-loop.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/regressions/dev-loop/README.md`
