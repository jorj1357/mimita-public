# Current `mimita.exe` link-input inventory

**Evidence:** static inspection of `build.py` and its source catalog. No build or PE inspection was run. The per-file C++ list is in [cpp-source-inventory.csv](cpp-source-inventory.csv), filtered by `directly_compiled_into_mimita_exe=True`.

## Translation units compiled into the EXE

- **602 C++ files:** 582 `src/` files selected by the module ownership catalog, plus 20 Recast/Detour files. The CSV identifies each exact path.
- **22 C files:** `src/glad.c` plus 21 libjuice C sources:

  - `src/glad.c`
  - `external/libjuice/src/addr.c`
  - `external/libjuice/src/agent.c`
  - `external/libjuice/src/base64.c`
  - `external/libjuice/src/conn.c`
  - `external/libjuice/src/conn_mux.c`
  - `external/libjuice/src/conn_poll.c`
  - `external/libjuice/src/conn_thread.c`
  - `external/libjuice/src/const_time.c`
  - `external/libjuice/src/crc32.c`
  - `external/libjuice/src/hash.c`
  - `external/libjuice/src/hmac.c`
  - `external/libjuice/src/ice.c`
  - `external/libjuice/src/juice.c`
  - `external/libjuice/src/log.c`
  - `external/libjuice/src/random.c`
  - `external/libjuice/src/server.c`
  - `external/libjuice/src/stun.c`
  - `external/libjuice/src/tcp.c`
  - `external/libjuice/src/timestamp.c`
  - `external/libjuice/src/turn.c`
  - `external/libjuice/src/udp.c`

The 12 other `src/` implementation files are owned by current subsystem module DLLs, not directly compiled into `mimita.exe`. Those module files are already outside the EXE, though they still need to be merged into the planned single `game.dll` or explicitly retained as resident exceptions.

Two EXE-owned files are also listed in the focused presentation DLL: `src/effects/effect-part.cpp` and `src/hot-reload/quick-hit-debug-visual.cpp`. This is duplicate compilation, not whole-game hot reload.

## Other direct inputs and linked code

- `external/manifold-prebuilt/lib/libmanifold.a` is linked as a static archive, so its object code is included in the EXE. A saved Ninja manifest lists 18 Manifold `.cpp` files, but it is dated 2026-09-30 and does not prove which sources produced this archive. Treat the archive’s source provenance as unresolved.
- `mimita.rc` is compiled into `build/mimita.res.o`; it embeds the application icon and version/product metadata. The icon file is `assets/uitextures/mimita desktop icon v1.ico`.
- `src/pch.h` is force-included while compiling app C++ sources. It contributes declarations/code through the translation units; the PCH artifact itself is not linked as a separate object.

## Link libraries and runtime dependencies

The build script lists these linker libraries:
- `-lglfw3`
- `-lopengl32`
- `-lgdi32`
- `-luser32`
- `-ldwmapi`
- `-lwinhttp`
- `-lws2_32`
- `-ldbghelp`
- `-lpsapi`
- `-lole32`
- `-luuid`
- `-loleaut32`
- `-lmfplat`
- `-lmfreadwrite`
- `-lmfuuid`
- `-lmf`
- `-ld3d11`
- `-ldxgi`
- `-lbcrypt`
- `-lpthread`

`-lglfw3` resolves through the configured GLFW library directory; `glfw3.dll` is staged next to the EXE. The build also stages `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll`. These sidecar DLLs are loaded at runtime, not embedded in `mimita.exe`. OpenGL, Windows UI/network/debug/media/Direct3D libraries resolve from the Windows/toolchain environment.

## Shipped separately

The release packager includes `assets/`, `config/`, `shaders/`, `Characters/`, `tools/ffmpeg.exe`, and the runtime DLLs as separate files in the release ZIP. Those files are runtime data/tools, not linked into the EXE. Runtime changes to those data files are a separate reload path from C++ DLL reload.

## Scope for the requested hot-reload target

The current executable contains 602 directly compiled C++ translation units and 22 C translation units, plus the Manifold static archive and the Windows resource object. A whole-game cutover must account for every code-bearing object: route editable gameplay implementation into `game.dll`, put stateful transport/platform services behind an explicit resident owner, and document any remaining stable bootstrap/runtime exception. It cannot make the EXE loader itself replaceable while that same loader is executing; changes to the minimal bootstrap/runtime boundary still require a deliberate host update unless a separate external supervisor owns the process.

This is a source-declared link-input map, not a post-link PE import/object audit. The static Manifold archive is the main source-provenance gap; toolchain/system library resolution is environment-dependent.
