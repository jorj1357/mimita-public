# Runtime DLL staging lock handling

Fixed the development build failure where linking completed but
`build.py` exited with `PermissionError: [WinError 32]` while replacing a
root-local runtime DLL used by another process.

Changes:

- Root runtime-DLL staging is now best-effort when the destination is locked.
- The dev loop resolves MinGW and GLFW runtime DLLs from the configured current
  toolchain and copies them into each versioned `.dev\builds\####` directory.
- The dev loop now reports a missing per-build runtime DLL explicitly.

Validation:

- `python devscripts/dev-loop.py --help`: passed.
- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python build.py build-only`: passed while the numbered server/client were
  still running; link and four runtime DLL stages completed.

Related regression:
`docs/regressions/2026-09-27/runtime-dll-staging-lock-REG.md`
