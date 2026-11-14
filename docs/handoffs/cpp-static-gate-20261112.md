# Game static gate (2026-11-12)

**Status:** static scan of game-owned sources for raw lifetime patterns that
must use a resident capability instead. Per-call local file handles with a
matching fclose are allowed; persistent handles are not. This is not a semantic
review and not runtime evidence.

- Game sources scanned: 617
- Violations: 1
- Files with violations: 1

## Rule counts

| Rule | Capability | Violations |
|---|---|---:|
| raw_thread | CAP_JOBS | 0 |
| raw_gl_resource | CAP_RESOURCES | 1 |
| raw_socket | CAP_TRANSPORT | 0 |
| raw_file_handle | CAP_RESOURCES | 0 |

## Files

| File | Rule | Line | Snippet |
|---|---|---:|---|
| `src/render/render-command-legacy.cpp` | raw_gl_resource | 480 | `glGenerateMipmap(toGl(target));` |
