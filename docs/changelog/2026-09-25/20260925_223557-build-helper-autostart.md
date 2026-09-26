# Build helper dependency setup and auto-start

Time: 2026-09-25 22:35:57 EDT
Branch: afad20a-rebuild
Base commit: afad20a6296d38bde7a5abf685f045d1d3e2a3b6

## Request

Make `C:\mimita-v9\buildv2.py` work by itself, create the executable, and
start the executable it built.

## Documents read

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/regressions/regressions-v1.md`

## Source change

Changed `buildv2.py` only.

Added local dependency setup that:

1. initializes `external/libjuice` when `juice/juice.h` is missing;
2. preserves explicit `MIMITA_COMPILER`, `MIMITA_GLFW_INCLUDE`, and
   `MIMITA_GLFW_LIB` values;
3. otherwise finds the local compiler and GLFW installations used by this
   Windows development machine;
4. leaves the existing clean-relink and `build.py` launch behavior intact.

No game source code or shared toolchain resolver was changed.

## Build and runtime proof

Ran exactly:

`python C:\mimita-v9\buildv2.py`

The script automatically found:

- `C:\important\msys64\mingw64\bin\g++.exe`
- `C:\important\glfw-3.4.bin.WIN64\include`
- `C:\important\glfw-3.4.bin.WIN64\lib-mingw-w64`

It linked:

`C:\mimita-v9\mimita.exe`

Executable size: 108,366,104 bytes.

The launched process was observed as `C:\mimita-v9\mimita.exe` and reached
OpenGL initialization, shader compilation, engine initialization, UI setup,
avatar loading, networking configuration, and hot-reload DLL activation.

## Cleanup

The launch-generated changes to `config/accounts/default.json`,
`config/analytics.json`, and `src/pch.h.d` were restored. The remaining tracked
source change is only `buildv2.py`.

## Human review still needed

The build and executable startup are proven. Gameplay and multiplayer behavior
still need human review in this clean checkout.
