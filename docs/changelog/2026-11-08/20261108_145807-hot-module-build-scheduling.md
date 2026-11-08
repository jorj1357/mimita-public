# Hot module build scheduling and ownership trace

## Change

- Limited the running module host to two lower-priority background workers and kept deferred source changes pending until a worker actually starts.
- Changed the module worker to compile and cache translation-unit objects by source and discovered header inputs, then link a new immutable DLL generation. Compiler failures identify the translation unit; link failures identify the link phase.
- Excluded module-only `*-module.cpp` entries from the dev-loop cold EXE watcher. Shared headers and host sources still trigger the serialized EXE build.
- Updated the hot-reload and dev-loop specifications with the build rule and a first ownership inventory for the reported repair areas.

## Evidence and limits

- Source inspection: the current module catalog contains narrow editor, rendering, world generation, collision-policy, and NPC-policy entries. Most gameplay, network, lifecycle, and rendering mechanisms still reside in the EXE.
- Static checks: Python syntax check and module ownership audit passed; `git diff --check` passed.
- Build: the existing dev loop is building changed host code while preserving the connected game. The new worker's compile/link path has not yet completed in an uncontended run.
- Runtime: the connected Payload/FunWorld3 run observed here has zero NPCs and an older EXE identity. It cannot prove the 19-NPC, frame-rate, damage, or correction targets. No gameplay acceptance is claimed.

## Review

- Routed specification and behavior, efficiency, and logging review guidance were consulted. The remaining module extraction requires tracing each input-to-result path and preserving server authority and fixed-tick mechanisms.
