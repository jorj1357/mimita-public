# MiMITA Third-Party Notices

This file lists third-party software, fonts, and assets used by MiMITA along with
their licenses. The authoritative license text for each component lives with that
component (e.g. `external/<name>/LICENSE`) or at the link provided.

## Noto Serif CJK TC Regular

- **Component:** Noto Serif CJK TC (NotoSerifCJKtc-Regular.otf)
- **License:** SIL Open Font License 1.1
- **Copyright:** (c) 2014-2021 Adobe Systems Incorporated, (c) 2014-2021 Google LLC
- **Usage:** Used as the source to generate MiMITA's bitmap `.fnt` + `.png` UI font
  atlas (`assets/font/noto-serif-cjk-tc-mimita-v1.fnt` + page PNGs). The game ships
  only the generated atlas; the `.otf` itself is a dev-time regeneration source kept
  under `tools/fonts/` (not shipped, not tracked in Git).
- **Regeneration:** `python tools/fonts/generate-atlas.py`
- **Download / full license text:**
  - https://github.com/googlefonts/noto-cjk
  - https://scripts.sil.org/OFL

## Bundled libraries

- **GLFW** (`glfw3.dll`) — zlib/libpng license — https://www.glfw.org
- **libjuice** — Mozilla Public License 2.0 — `external/libjuice/LICENSE`
- **miniz** — MIT — `external/miniz/LICENSE`
- **stb_image** — MIT — https://github.com/nothings/stb
- **glad** (OpenGL loader) — MIT — https://github.com/Dav1dde/glad
- **glm** — MIT — https://github.com/g-truc/glm
- **LuaJIT** — MIT — https://luajit.org
- **nanosvg** — zlib — https://github.com/memononen/nanosvg
- **nlohmann/json** — MIT — https://github.com/nlohmann/json
- **tinygltf** — MIT — https://github.com/syoyo/tinygltf
