# DLL build notes and terminal result evidence

- Added a mandatory agent gate to queue `build-note.py --changes` before the first whole-game DLL compiler-input edit. Separate notes from independent contributions remain attached together when edits coalesce into one DLL attempt.
- DLL build outcomes now carry build-attempt ID, source generation, revision, success/failure, artifact identity when available, all consumed notes, changed paths, and per-attempt timing/summary paths in the dev-loop run journal. Exceptions during attempt setup also return a failed result and write a fallback timing summary where the repository permits it.
- Expanded dashboard note display to show every consumed and pending note. Updated build-time observability, dev-loop, and canonical JSONL specs to define note aggregation and final build-result logging.
- Validation: Python AST parsing passed for `devscripts/dev_loop/game_build.py`, `devscripts/dev-loop.py`, and `devscripts/dev_loop/gui.py`; `git diff --check` passed. No build or runtime test was run.
