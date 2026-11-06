# Hide compiler and cache console windows

- Local time: 2026-11-05 20:34:01 EST
- UTC: 2026-11-06T01:34:01Z
- Branch: `20261011zt`
- HEAD at start: `a992f9a4ebb165eb403e6892f953e8adc87dc7c6`
- Result: `PASS_WITH_HUMAN_REVIEW`; source syntax passed. The dev loop later recovered and began rebuilding; the visible popup cause is still unresolved because separate desktop-app Git scans were observed.

## Scope and source change

The user authorized a patch after inspection identified Chocolatey `ccache.exe` invocations in the repository build path as the likely source of repeated console windows.

- `build_toolchain.py`, new `run_tool()` at line 64: added the shared compiler-tool subprocess owner. On Windows it supplies `subprocess.CREATE_NO_WINDOW`; on other platforms it preserves normal subprocess behavior.
- `build.py`, lines 453, 501, 533, 627, 703, 775, and 840: routed PCH, C++, C resource, resource-compiler, and linker invocations through `run_tool()`.
- `build_game_dll.py`, line 78: routed the hot-reload DLL compiler/cache invocation through the same helper.
- Follow-up: `devscripts/dev_loop/source_identity.py`, lines 12-27: added Windows no-console flags to the daemon's Git identity subprocess after live inspection showed that it was launched by the dev-loop daemon with plain `subprocess.run`.

Old behavior: these compiler and cache owners used `subprocess.run(...)` with no Windows creation flag, allowing console windows to appear when invoked by the windowless dev-loop build process.

New behavior: the shared `run_tool()` sets `creationflags=subprocess.CREATE_NO_WINDOW` on Windows and delegates to `subprocess.run()`. Child stdout/stderr handles and exit codes remain inherited, so existing build output and failure reporting continue through the same path. Game executable launches in `build.py` were intentionally left as ordinary `subprocess.run()` calls.

## Pre-existing work

The worktree was already broadly dirty before this task, including extensive configuration, gameplay, documentation, changelog, and regression edits. `devscripts/dev_loop/build.py` was also modified before this task and was not changed. The three implementation files listed above were clean before this patch. Unrelated work was preserved.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/specs/performance/performance.md`
- `docs/skills/efficiency-checker-v1.md`

Review finding: one small helper in the existing toolchain module avoids repeating Windows process flags across both build entry points. The implementation changes no gameplay path or runtime executable launch behavior.

## Validation and evidence

- `python -m py_compile build.py build_game_dll.py build_toolchain.py`: passed.
- `git diff --check -- build.py build_game_dll.py build_toolchain.py`: passed; Git emitted existing LF-to-CRLF working-copy warnings for `build_game_dll.py` and `build_toolchain.py`.
- Canonical build request `python devscripts/dev-loop.py --request build --wait-for-build`: not completed. It returned `[DEV CONTROL] daemon/watchdog recovery failed; no control endpoint`.
- Follow-up `python devscripts/dev-loop.py --status`: reported `lifecycle=stopped`, `build=failed`. The watchdog state points to an attempted restart and `.dev/dev-loop-console.log` records a `FileNotFoundError` for `.dev/builds/2026-11-06T01-33-12.179985Z/build-identity.json` while the daemon was initializing. No competing or cold build was started.
- Build behavior with hidden consoles has not been observed in a completed build. Human confirmation that future compiler/cache invocations no longer create visible windows remains required.

No regression record was added: the build interruption is an existing dev-loop recovery issue and the patch did not establish a new confirmed regression.

## Follow-up live watch — 2026-11-05 21:14 EST

- Process inspection found Git launched by the dev-loop daemon for source identity, plus separate Git scans launched by OpenCode and the ChatGPT desktop app. OpenCode's observed commands included `git diff-files --name-only` and `git ls-files --others` against `C:\mimita-v9`.
- Windows Terminal PID 23272 showed a main-window title that changed between `C:\Program Files\Git\cmd\git.exe` and `C:\mimita-v9\.dev\current\mimita.exe`; it started at 21:08:22. The CUA window inventory did not include a visible Terminal window during captures, so process metadata does not prove which caller surfaced the user's popup.
- After the source-identity patch, Git commands from the dev-loop daemon were observed with the daemon as parent, and no Terminal window appeared in the captured window inventory. OpenCode and desktop-app Git scans remain separate and were not modified.
- `python -m py_compile build.py build_game_dll.py build_toolchain.py devscripts/dev_loop/source_identity.py`: passed. `git diff --check` for tracked implementation files passed with existing LF-to-CRLF warnings.
- At 21:14 EST, `python devscripts/dev-loop.py --status` reported a build in progress at the link step; the server and client remained alive. A completed build result and conclusive human confirmation that popups have stopped are still pending.
- The user's later report said the terminal sometimes displays `process exited`. The Windows Terminal settings file has no explicit `closeOnExit`; its documented default is `automatic`. Setting it to `always` would close completed tabs, including failed commands, but would not prevent Git from launching a terminal. Git processes from OpenCode and the ChatGPT desktop app were observed, so attribution of the visible Terminal window to a single caller remains unproven. No Windows Terminal setting was changed.
