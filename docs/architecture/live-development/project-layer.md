// 09 12 2026
/* purpose
* Describe the live project layer: versioned project tree, content store,
* change sets, history, dependency graph, state schemas, and the shared
* authoring surface for humans and agents.
* this file DOES NOT define gameplay or the runtime generation model
* this file DOES NOT replace hot-kernel.md or live-development.md
* this file DOES NOT edit project files; it defines the model
*/

# Live project layer

## Purpose

Generalize "edit one hot .cpp" into "the project is a live, versioned,
content-addressed tree that AI or a human can mutate while MiMITA.exe runs."
The runtime generation is derived from a project version; the project version
keeps history even after files are deleted or renamed.

## Primitives (`src/project/`)

- `ContentId { algorithm, digest }` (`project-types.*`): algorithm-agile
  content identity. Never treat one hash algorithm as the meaning of identity.
- `BlobStore` (`blob-store.*`): immutable content-addressed store at
  `.mimita/store/<algorithm>/<digest>`. Deletion of a project path never deletes
  a blob, so history stays recoverable.
- `ProjectTree` + `ProjectTreeScanner` (`project-tree.*`): scans a filtered
  subtree into `{path, ContentId}` entries, computes a whole-tree hash, diffs
  two trees into a `ChangeSet` with add/delete/modify/rename/move detection
  (rename = same directory, move = different directory, inferred from identical
  content), and can check out any tree hash back onto disk.
- `ChangeSet` / `ProjectVersion` (`project-types.*`): a mutation unit and an
  immutable version in a chain (`parentHash`, `treeHash`, `changeSetHash`,
  `changeId`, `timestampMs`).
- `ProjectHistory` (`project-history.*`): record, undo, redo, checkpoint,
  restore, per-version diff, persisted to `.mimita/history.jsonl`.
- `DependencyGraph` (`dependency-graph.*`): incremental graph built from
  compiler `.d` files; `affected(changed)` returns the units to rebuild. No
  hand-maintained dependency list.
- `StateSchemaRegistry` (`state-schema.*`): versioned state schemas plus
  package-supplied `migrate(fromVersion, toVersion, oldState) -> newState`.
- `ProjectControl` (`project-control.*`): one command surface
  (`status|history|record|undo|redo|checkpoint|restore|diff`).
- `ControlServer` (`project-control-server.*`): loopback TCP line server so an
  external agent drives the same commands. Humans use the `project` terminal
  command; agents use the socket; either can use either.

## A + C deletion semantics

- **A (last-good stays active):** a project mutation is a candidate; the active
  runtime generation keeps running until the candidate validates and activates.
- **C (everything undoable by hash):** deleting a path removes it from the new
  tree only; the blob and the previous tree remain addressable, so undo restores
  the exact bytes.

## Source vs runtime generation

```text
ProjectTree -> ProjectVersion -> build -> RuntimeGeneration
```

These are distinct: a project version is source history; a runtime generation is
a loaded, validated, active code package set. Undo of the project changes the
tree; the loader then detects the source change and builds a new runtime
generation. Undo of code does not rewind world state unless a state migration or
world replay is explicitly chosen.

## First scope

`src/` only (`.cpp/.h/.hpp/.cc/.cxx/.c`, excluding `src/generated/`). Assets,
shaders, and configs join the same tree later without changing the model.

## Verification

`mimita.exe --project-selftest` (and the standalone primitives harness) covers
blob dedupe, tree scan, add/modify/delete/rename diff, history
record/undo/redo/checkpoint/restore, `.d` dependency affected set, and state
schema migration.

## Phase 4-6 additions

- Per-source hot build with `-MMD` (per-TU `.d`) then link; `DependencyGraph`
  can consume those `.d` files.
- `PackageRegistry` (tree `package.json`) and `ResourceRegistry`
  (content-addressed assets).
- `SubsystemSlot` replacement lifecycle and `ComponentSchemaRegistry`.
- `ProjectWatcher` (`ReadDirectoryChangesW`) feeding the loader.
- `CapabilityRegistry`, `DependencyResolver` (hash verification; download later),
  and `WorldHash`.
- `sim/domain-scheduler.*`: multiple simulation domains at independent rates.
- `CodeGenerationPacket` extended with logical vs platform identity and
  status/READY/SWITCH phases.

`mimita.exe --phase456-selftest` covers the above primitives.

## Related

- `docs/architecture/live-development/hot-kernel.md`
- `docs/architecture/live-development/hot-kernel-next-steps.md`
- `docs/gold/2026-09-12-live-code-hot-reload-journey.md`
