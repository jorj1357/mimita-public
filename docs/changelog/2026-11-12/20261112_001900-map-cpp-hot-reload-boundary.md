# Map repository C++ sources for whole-game hot reload

Updated the hot reload specification to state the whole-game `game.dll` goal and recorded the first-pass repository-wide C++ ownership/dependency map.

- Enumerated 731 repository `.cpp` files with per-file hashes, sizes, include relationships, existing compiler dependency snapshots, current build ownership, alternate target claims, and lexical state/callback/thread/job review leads.
- Classified 614 direct canonical game-build translation units: 594 `src/` files and 20 Recast files.
- Classified 18 Manifold files separately as members of a saved Ninja source manifest; the manifest is dated 2026-09-30 and does not establish the source contents of the linked archive. 99 other repository files are outside the reviewed game source/link manifests.
- Linked the CSV and JSON inventories from the hot reload specification. Corrected the inventory scanner and generated reports to keep direct build membership distinct from saved linked-library source manifests.
- Recorded focused DLL/EXE overlaps, alternate CMake declarations, compiler dependency snapshot availability, and static lifetime-review leads.

Validation: parsed the JSON and Python source, regenerated CSV from the saved inventory, and checked that the map has one row per enumerated source. No compiler, build, test, or game launch was run. Semantic callback/thread shutdown tracing, depfile freshness, DLL safety, timing baselines, and runtime proof remain open.
