<!-- 08 11 2026, 15 58 -->
<!-- purpose
* Lists every third-party component shipped with or linked into MiMITA.
* Documents the OSI-approved license for each so the project satisfies SignPath
* Foundation's "no proprietary, non open-source component" requirement.
* Does NOT list runtime data files (maps, textures, audio) or the website npm packages.
* Does NOT replace the LICENSE file at the repo root.
-->

# Third-Party Licenses

MiMITA is distributed under the MIT License (see `LICENSE` at the repo root).
The following third-party components are included in, or linked into, the
released binaries. All of them are distributed under OSI-approved open-source
licenses, or are System Libraries as defined by section 1 of the GPL v3 license
(and therefore permitted inside signed packages).

## Runtime DLLs shipped in mimita-game.zip

| File | Component | License | OSI-approved |
|------|-----------|---------|--------------|
| `glfw3.dll` | GLFW 3.4 | zlib/libpng | Yes |
| `libgcc_s_seh-1.dll` | MinGW-w64 GCC runtime | GPL v3 with GCC Runtime Library Exception | Yes (System Library) |
| `libstdc++-6.dll` | MinGW-w64 libstdc++ runtime | GPL v3 with GCC Runtime Library Exception | Yes (System Library) |
| `libwinpthread-1.dll` | MinGW-w64 winpthreads runtime | BSD 2-Clause / Zope Public License | Yes (System Library) |

These DLLs are unsigned upstream binaries (the GCC runtime and GLFW project do
not publish signed builds). SignPath Foundation's terms explicitly allow
including unsigned binaries of upstream OSS projects in signed packages.

## Statically linked into mimita.exe

| Component | Where | License | OSI-approved |
|-----------|-------|---------|--------------|
| libjuice (WebRTC ICE/STUN/TURN) | `external/libjuice/` | Mozilla Public License 2.0 | Yes |
| miniz (deflate/zip) | `external/miniz/` | MIT | Yes |
| stb_image | `include/stb_image.h` | public domain / MIT | Yes |
| stb_vorbis | `include/stb_vorbis.c` | public domain / MIT | Yes |
| stb_sprintf and other stb_* headers | `include/` | public domain / MIT | Yes |
| raylib (window/input/rendering helpers) | `include/raylib.h`, `raymath.h`, `rlgl.h` | zlib/libpng | Yes |
| GLFW (header interface for glfw3.dll) | `include/GLFW/` | zlib/libpng | Yes |
| glad (OpenGL loader) | `src/glad.c` | public domain / MIT | Yes |
| glm (math) | `include/glm/` | MIT | Yes |
| nlohmann/json | `include/nlohmann/` | MIT | Yes |
| miniaudio | `include/miniaudio.h` | public domain / MIT-0 | Yes |
| dr_wav / dr_mp3 / dr_flac | `include/` | public domain / MIT-0 | Yes |
| qoi | `include/qoi.h` | MIT | Yes |
| cgltf / cgltf_write | `include/cgltf.h`, `cgltf_write.h` | MIT | Yes |
| tinyobj_loader | `include/tiny_obj_loader.h` | MIT | Yes |
| tinygltf | `include/tinygltf/` | MIT | Yes |
| nanosvg | `include/nanosvg/` | zlib/libpng | Yes |
| zlib | `include/zlib.h`, `zconf.h` | zlib/libpng | Yes |
| LuaJIT | `include/luajit/` | MIT | Yes |
| dirent (Windows) | `include/dirent.h` | MIT | Yes |

## Statically linked into MimitaLauncher.exe

| Component | Where | License | OSI-approved |
|-----------|-------|---------|--------------|
| miniz (deflate/zip) | `external/miniz/` | MIT | Yes |
| GDI+ / Windows SDK APIs | system | Microsoft, provided by Windows | System |

`MimitaLauncher.exe` links only against Windows system DLLs plus `miniz`, which
is compiled directly from source in `external/miniz/`.

## Notes

- All libraries are compiled from their source included in this repository or
  (for `glfw3.dll`) obtained from the GLFW project's official 3.4 Windows
  binary release. The build scripts and CI workflow pin the exact versions used.
- No component in the released binaries is proprietary or closed-source. The
  only third-party code shipped or linked is covered by the table above.
