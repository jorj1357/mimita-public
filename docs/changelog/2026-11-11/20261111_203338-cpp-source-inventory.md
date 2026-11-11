# Task

- Task ID: cpp-source-inventory
- Summary: Added a repository-local Python tool that inventories every `src/**/*.cpp`, its current ownership, direct and transitive project includes, available compiler dependency snapshots, and reload-review signals. Ran it to create a complete JSON report.
- Status: Inventory generated and structurally validated; no runtime or build claim.
- Date, time, timezone: 2026-11-11T20:33:38Z, ISO 8601
- Branch: Not verified; Git status inspection is blocked by the inaccessible `external/libjuice` submodule metadata.
- Base commit: Not verified for the same reason.
- Final commit: Not committed.

# Pre-existing changes

- Exact status output: `git -c status.ignoreSubmodules=all status --short` failed with `fatal: cannot chdir to 'C:/mimita-v9/.git/modules/external/libjuice': Permission denied`.
- Files not created or modified by this session: Previously observed changed paths were `config/analytics.json`, `docs/specs/hotreload/hotreload.md`, `docs/specs/open-gameplay-issues.md`, and untracked `build-identity.json`; this session did not edit them.

# Requested behavior

Provide an exhaustive and repeatable `.cpp` dependency inventory under `docs/handoffs/`, with a script that generates the JSON report.

# Specification alignment

- Current specification paths: `docs/architecture/time-and-formatting/time-and-formatting.md`; `docs/specs/hotreload/hotreload.md`.
- Exact requirements: Preserve source/build/runtime evidence distinctions; do not imply a report or successful compile proves live hot reload.
- Why the change follows the specification: The tool is read-only with respect to source and build ownership. It reports current ownership separately from proposed migration candidates and marks depfile freshness unknown rather than guessing.
- Conflicts or decisions: Compiler depfile contents are inventoried, but dependency timestamp freshness is left unknown to avoid expensive and permission-sensitive per-path filesystem checks.

# Exact implementation changes

## File: `devscripts/cpp-source-inventory.py`

- Lines/functions/headings: `current_include_graph`, `read_dep_snapshots`, `source_owner_data`, `build_inventory`, `main`.
- Old content or behavior: No repository tool produced a single full source/dependency/ownership JSON inventory.
- New content or behavior: Scans all `src/**/*.cpp`; records source size, lines, SHA-256, current EXE/module owner, migration candidate, direct/transitive project includes, matching debug/release `.d` snapshots, include fanout, module deps manifests, and reload review heuristics. Writes atomically under the repository and invokes no compiler.
- Reason: Make the current compilation and ownership graph inspectable before selecting the initial DLL migration set.
- Why unrelated behavior is preserved: The tool does not edit C++ files, ownership catalogs, build scripts, or runtime behavior.

## File: `docs/handoffs/cpp-source-inventory.json`

- Lines/functions/headings: JSON report, schema version 1.
- Old content or behavior: No generated inventory existed at this path.
- New content or behavior: Complete report for all 594 discovered `.cpp` files, including 1,133 existing debug/release compiler depfile snapshots across 583 translation units.
- Reason: Provide detailed evidence for deciding what can move to `game.dll` and what needs runtime/ABI/state work.
- Why unrelated behavior is preserved: Generated report only; it does not change source or build behavior.

# Diagnostics

- Owner/category: Inventory script progress and summary.
- Input: Repository `src/` tree, current module ownership catalog, existing `build/obj-debug` and `build/obj-release` `.d` files, and module `deps.json` files.
- Decision: Use compiler depfiles where present and source include traversal for the project include graph; distinguish current owners from future candidates.
- Output: 594 sources; 582 currently EXE-owned; 12 currently module-owned; 583 translation units with depfile snapshots; 11 without a matched depfile; dependency freshness unverified.
- Failure or rejection reason: An initial depfile parser treated escaped Windows drive colons as Make rule separators. It was corrected before the successful run. The source scanner reports 2,481 unresolved-or-external include spellings; this includes standard-library/platform/system headers and macro or conditional includes, and is not a claim of 2,481 missing project headers.
- Rate limiting: Progress printed every 200 compiler depfiles and every 1,000 scanned include files.

# Validation

- Focused skill paths and results: Not applicable.
- Tests and exact commands: `python -u devscripts/cpp-source-inventory.py` completed successfully. JSON was parsed with Python; verified 594 unique entries and that every listed `.cpp` exists.
- Build status: No build or compiler invoked, as required for this inventory task.
- Runtime or hot-reload evidence: None; this report does not implement or demonstrate hot reload.
- Output files: `docs/handoffs/cpp-source-inventory.json` (35,761,373 bytes); `devscripts/cpp-source-inventory.py`.

# Measured evidence

- Before values: No consolidated inventory.
- After values: 594 source files; 8,095 direct include edges in the current source traversal; 1,133 compiler dependency snapshots; 100 top fanout headers in the summary.
- Timestamps: Generated at 2026-11-11T20:33:38Z.
- Tick/frame/network measurements: None.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: No behavior regression observed; work is read-only inventory tooling and generated documentation.
- Related regression paths: None.

# Human acceptance

- Visual review: Not applicable.
- Gameplay review: Not performed.
- Multiplayer review: Not performed.
- Still unverified: Whether any candidate can unload safely, whether a source change can compile/activate live, runtime state continuity, DLL ABI safety, rollback, and build latency under the dev loop.

# Related feature record

- Feature path: `docs/specs/hotreload/hotreload.md`
