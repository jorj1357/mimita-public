# ccache PATH diagnostic

- Timestamp: 2026-11-07T23:43:12Z (2026-11-07 18:43:12 EST)
- Branch: `20261011zt`
- HEAD: `1fc06d59`
- Result: `PASS`

## Request

Diagnose the ccache compile failure reported while compiling
`src/world/streamed-world-layer.cpp` and verify that ccache works.

## Source and pre-existing state

- No source or build configuration was changed. `build.py` already prepends
  the selected compiler's directory to `PATH` at initialization
  (`build.py`, `COMPILER`/`CCACHE` setup, lines 68-74); `build_toolchain.py`
  resolves the installed compiler and ccache.
- The workspace had pre-existing changes in configuration, Blender tooling,
  engine, hot-reload, physics, procedural-world, and streamed-world files.
  These were not edited by this diagnostic.
- The user's earlier isolated ccache invocation omitted the MinGW compiler
  directory from `PATH`. A ccache invocation using the Chocolatey shim at
  `C:\ProgramData\chocolatey\bin\ccache.exe` and the explicit MinGW compiler
  then exited 1. Repeating the same streamed-world translation-unit compile
  after prepending `C:\important\msys64\mingw64\bin` to `PATH` exited 0.
- A separate two-compile cache proof with the same ccache and compiler, using
  an isolated temporary cache, returned exit 0 for both compilations and
  reported 1 hit / 1 miss.

## Build evidence

Ran the canonical dev-loop build request. It completed with
`result=success`; the published build identity was
`2026-11-07T23:41:56.842881Z`. The dev-loop also reports ccache at
`C:\ProgramData\chocolatey\bin\ccache.EXE`. This proves the build completed;
no new runtime or visual claim is made.

## Documentation and validation

- Read `docs/ROUTER.md`, `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`, and
  `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Validation: direct ccache compile of `streamed-world-layer.cpp` with the
  MinGW bin directory on `PATH` passed; two-run cache-hit proof passed; the
  canonical dev-loop build passed.
- Human review: not required for this environment-only diagnostic.
