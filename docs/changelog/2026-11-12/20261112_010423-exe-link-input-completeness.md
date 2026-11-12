# Inventory every declared EXE code input

Compared the `.cpp` inventory with the canonical `build.py` source and link flow to answer whether it covered all code entering `mimita.exe`.

- Corrected source counts: 582 `src/` C++ files plus 20 Recast C++ files are directly compiled into the EXE (602 total); 12 other `src/` C++ files are built as subsystem module DLLs.
- Identified 22 additional C translation units directly compiled into the EXE: `src/glad.c` and 21 libjuice sources.
- Recorded the linked Manifold static archive, its unverified/stale source provenance, the `mimita.rc` icon/version resource, system link libraries, runtime DLL sidecars, and separately packaged runtime assets/tools.
- Updated the inventory scanner, JSON, CSV, hot reload spec, and handoff report to distinguish current EXE C++ files, subsystem DLL sources, C sources, and non-source link inputs.
- Refreshed the read-only C++ inventory; confirmed all C inputs, archive, resource script, and icon exist in this checkout. Added an explicit per-file CSV flag for direct EXE compilation.

Validation: inventory reports 602 direct EXE C++ translation units, 22 C units, 12 subsystem-module C++ sources, and 731 repository `.cpp` paths. The source-declared link map is not a post-link PE audit, and the current contents of `libmanifold.a` are not proven from the dated Ninja manifest. No compiler, build, test, or game launch was run. `docs/architecture/player-npc-systems/npc-movement.md:22` still has the unrelated note `9 8 2026 1019 est jorj todo - explain`; suggested replacement is a concise current movement/stuck-recovery ownership and evidence description. It was not changed.
