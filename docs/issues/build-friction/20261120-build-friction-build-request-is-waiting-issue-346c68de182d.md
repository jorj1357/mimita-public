# issue-346c68de182d: Build friction: build request is waiting

## Human report (verbatim, immutable)

> a build is pending with no active generation


## Triage

### 2026-11-20T01:54:00.074Z — coordination-v2

Summary: Root dev-loop build is pending with no active generation; diagnose queue/worker and source generation 57
Owner: coordination-v2
Priority: high

## Assignment

### 2026-11-20T01:54:00.607Z — coordination-v2

Owner: coordination-v2

## Task linked

### 2026-11-20T01:54:27.954Z — coordination-v2

Task: diagnose-build-wait (step 0)

## Task linked

### 2026-11-20T03:33:16.918Z — coordination-v2

Task: build-393-slow (step 0)

## User quote

### 2026-11-20T03:33:33.620Z — coordination-v2

Via coordination-v2; supports issue-346c68de182d:

> Gen-393 attempt game-dll-20261120T024625732466Z-8281b0/source_generation 58: elapsed 2785s, dependency_scan 1629s, changed_source_preflight 252s, compile 692s, completed 642, current DLL still generation 390; last_result failed; no new DLL published. Build note a4ae110de0dc waits for the next build.
