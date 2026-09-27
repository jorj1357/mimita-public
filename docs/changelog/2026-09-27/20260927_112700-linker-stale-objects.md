# Linker Stale Objects

Date: 2026-09-27
Time: 2026-09-27T11:27:00-04:00 (America/New_York)

## Problem

The executable link reported undefined references to server chat, server
command, NPC damage, timeout, and void-death functions. The source files were
present, but `build/obj-debug/network_server-packet-chat.o` was a 311-byte
partial object and exported none of its expected symbols.

## Fix

- Forced `src/network/server-packet-chat.cpp` to rebuild, restoring the
  expected server symbols to the object file.
- Updated `build.py` so a failed C++ compile removes its partial object and
  dependency file instead of allowing a later incremental build to skip the
  source.

## Validation

- `python build_agent.py`: `Status: SUCCESS`, return code 0.
- Link completed and runtime DLLs were staged.
- The prior undefined-reference set no longer appeared.
- `git diff --check` passed with only existing line-ending warnings.

## Scope and review

The source change is limited to a rebuild note in
`src/network/server-packet-chat.cpp`; gameplay/network ownership was not
redesigned. Runtime server startup and human gameplay acceptance remain
pending.

Focused documents used:

- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/live-development/live-development.md`
- `docs/skills/terminal-command-checker-v1.md`
