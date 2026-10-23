# Watchdog stack and journal cutoff investigation

**Time:** 2026-10-23 00:27:52 EDT (04:27:52 UTC)  
**Branch:** `20261011zt`  
**HEAD:** `cbfa4b0d` (`bad performance like working on it but still getting worse idk what problem is`)  
**Commit created:** no

## Pre-existing work

The checkout already contained many unrelated and earlier diagnostics/performance edits, gameplay issue documentation, and changelogs. Those were preserved. This record covers only the two source changes below and the runtime investigation performed in this session.

## Source changes

### `src/perf/perf-frame.cpp`, `resourceWatchdogMain`, lines 539-563

Before, the watchdog went directly from deciding a stall existed to calling `captureThreadStackTrace()`; no separate record showed whether capture began or returned. Added a best-effort `performance.stall-stack-capture` record before and after capture. The start record includes the target thread, last frame, heartbeat age, and CPU delta; the end record includes success and captured stack text.

Before:

```cpp
            stackCaptured = captureThreadStackTrace(
                mainThreadId, stackTrace, sizeof(stackTrace));
```

After:

```cpp
            StructuredLogger::instance().tryWriteEvent(
                StructuredCategory::Performance, StructuredLevel::Important,
                "performance.stall-stack-capture", "performance",
                "begin main-thread stack capture",
                static_cast<uint32_t>(gLastCompletedFrameNumber.load(
                    std::memory_order_relaxed)),
                nlohmann::json{
                    {"phase", "begin"},
                    {"main_thread_id", mainThreadId},
                    {"frame_heartbeat_age_ms", heartbeatAgeMs},
                    {"last_completed_frame", gLastCompletedFrameNumber.load(
                        std::memory_order_relaxed)},
                    {"main_thread_cpu_delta_ms", mainCpuDeltaMs}},
                __FILE__, __LINE__, __FUNCTION__);
            stackCaptured = captureThreadStackTrace(
                mainThreadId, stackTrace, sizeof(stackTrace));
            StructuredLogger::instance().tryWriteEvent(
                StructuredCategory::Performance, StructuredLevel::Important,
                "performance.stall-stack-capture", "performance",
                "finished main-thread stack capture",
                static_cast<uint32_t>(gLastCompletedFrameNumber.load(
                    std::memory_order_relaxed)),
                nlohmann::json{
                    {"phase", "end"},
                    {"main_thread_id", mainThreadId},
                    {"captured", stackCaptured},
                    {"stack", stackTrace}},
                __FILE__, __LINE__, __FUNCTION__);
```

### `src/debug/crash-handler.cpp`, `captureThreadStackTraceInternal`, lines 236-242

Before, symbol initialization and stack walking happened while the target main thread was suspended. Now the thread is resumed immediately after copying its CPU context, before symbol lookup. This prevents a slow or stuck symbol lookup from keeping the game thread suspended. Stack walking can still fail or hang in the watchdog thread; the new start/end records are intended to expose that case.

Before:

```cpp
        if (GetThreadContext(thread, &context)) {
            appendSymbolizedStack(out, static_cast<DWORD>(outSize), &context, thread);
            captured = true;
        } else {
            std::snprintf(out, outSize, "GetThreadContext failed: error=%lu",
                          static_cast<unsigned long>(GetLastError()));
        }
        ResumeThread(thread);
```

After:

```cpp
        if (GetThreadContext(thread, &context)) {
            ResumeThread(thread);
            appendSymbolizedStack(out, static_cast<DWORD>(outSize), &context, thread);
            captured = true;
        } else {
            std::snprintf(out, outSize, "GetThreadContext failed: error=%lu",
                          static_cast<unsigned long>(GetLastError()));
            ResumeThread(thread);
        }
```

## Runtime evidence

- Build 2873 succeeded through the Dev Loop. Executable: `.dev/builds/2873/mimita.exe`, SHA256 `8c6703eadf58529f54ef4a7da226ef74caff92df40b485cca3a9377d9b002674`.
- Launched Juggernaut mode 9 on `dust2cyberiav4`. The exact run journal is `logs/2026-10-23/20261023_042413/events.jsonl`. `versioninfo` was delivered successfully near startup and identified that journal. The Dev Loop unexpectedly reported visible mode even though `--headless` was requested; the run is therefore not a headless acceptance run.
- The client journal became quiet again. Its final event was at 49.592 seconds (`anim.state.changed`, sequence 3375). Server events continued afterward, so the shared journal file and cross-process mutex were not globally stuck.
- Near the cutoff, 60 frames averaged 9.76 ms with 96.7% over the 4.167 ms 240-FPS budget; another later window averaged 8.24 ms with 100% over budget. The earlier window reported logger flush time 0.43 ms and mutex wait 0.08 ms. These values do not support slow journal writes as the frame-time cause.
- A later `versioninfo` request was accepted by the Dev Loop but not delivered or completed. The client process remained alive. Windows thread inspection showed its last known main thread was still running and had accumulated about 68 seconds of CPU time; this confirms the process did not exit, but does not identify what that thread was executing.
- The new `performance.stall-stack-capture` start/end events did not appear after the cutoff. This means there is still no reliable stack evidence. The nonblocking logger can drop the marker if the shared mutex is busy, so absence alone cannot prove the watchdog never entered capture.
- An independent Windows Performance Recorder CPU trace was attempted, but Windows rejected it with `0xc5585011: Failed to enable the policy to profile system performance`. The current session lacks the required system profiling privilege.
- The Dev Loop stopped the test client/server. No unrelated game process was terminated. The run ended before a complete 60-second journal and does not meet the 240-FPS target.

## Findings and next evidence

The current evidence points to a client-side frame/update path that stops completing observable frame work while the process remains alive. It weakens the theory that the shared JSONL file is globally blocked. It does not yet identify the function or prove the cause of the no-longer-delivered terminal command.

The stack-capture code previously kept the main thread suspended during symbol lookup. That was unsafe and has been corrected, but build 2873 still showed the journal cutoff, so this change is not a fix for the underlying frame stall. The in-process start marker was absent and Windows denied external CPU-stack sampling. Exact function-level diagnosis remains blocked on a native stack capture. Repeat with Windows Performance Recorder started from an elevated session, or use an approved debugger that can capture the MiMITA process without terminating it. Then compare the main-thread stack at the last completed frame against the stack during the quiet period.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/specs/performance/performance.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/skills/logging-checker-v1.md`, and `docs/skills/efficiency-checker-v1.md`.
- The router's `docs/regressions/regressions-v1.md` target does not exist in this checkout; no confirmed root-cause regression was added.
- Logging review: markers are categorized and only attempted at the stall threshold / repeat interval, but the nonblocking writer can drop them. **Partial; exact quiet-time stack still needed.**
- Efficiency review: the stack helper now resumes the target before symbolization. **Source/build validated; hang prevention not runtime-proven.**

## Validation and remaining acceptance

- Dev Loop build 2873: success.
- Runtime run `20261023_042413`: cutoff reproduced; exact `events.jsonl` inspected; external host resource samples saved beside it.
- Windows Performance Recorder stack capture: blocked by system profiling privilege (`0xc5585011`).
- No tests were added or run. No commit was created.
- Source/build proof is separate from runtime proof. No complete headless 60-second run, no root-cause stack, and no 240-FPS acceptance were achieved.
