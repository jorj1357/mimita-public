# Repository inventory — cleanup branch

Status: initial structural inventory; no cleanup deletions performed.

Branch: `20260924cleanup`

## Architectural objective

The target direction is a small, stable executable kernel with as much
behavior as practical moved into hot-reloadable modules, packages, schemas,
systems, data, and resources. Actors should be the shared foundation from
which players, NPCs, weapons, and other gameplay objects emerge. This is a
direction, not yet proof that every current subsystem can safely move hot.

## Current visible shape

The working tree contains approximately:

| Area | Approximate files | Initial confidence | Meaning |
|---|---:|---|---|
| `src/` | 1,372 | High | Main native implementation and hot-runtime code. Requires ownership mapping before deletion. |
| `include/` | 605 | Medium | Headers and public/shared interfaces; likely overlaps with `src` ownership. |
| `docs/` | 552 | High | Specifications, architecture, workflows, operations, skills, changelogs, and historical material. |
| `assets/` | 832 | Medium | Runtime and authored content; needs active-loader tracing before pruning. |
| `website/` | 9,597 | Low | Separate product surface with generated/dependency/content material mixed in; must be inventoried independently. |
| `config/` | 172 | Medium | Runtime configuration; active loading paths must be resolved before classifying files. |
| `devscripts/` | 63 | Medium | Build, validation, and maintenance tooling; some may be historical. |
| `tests/` | 62 | Medium | Tests and self-tests; need owner and execution-path mapping. |
| `external/` | 93 | Medium | Third-party or vendored material; licensing and build references required before movement. |
| `archive/` | 21 | High | Explicit archive candidate, but retention/deletion requires evidence and decision. |
| root build artifacts | many | High | Timestamped executables, DLLs, logs, object/dependency files, reports, and packages visibly accumulate at the repository root. |

The raw recursive inventory reports about 4,031 files, including generated,
ignored, dependency, website, build, and historical material. This number is
not yet the number of source files an AI must understand.

## Immediate risk signals

1. The root contains many timestamped `.exe` files, `.d` dependency files,
   build logs, archives, reports, and extracted/runtime artifacts. These are
   strong cleanup candidates, but first we must distinguish tracked release
   evidence from disposable build output.
2. `src/` and `include/` are large enough that ownership cannot be inferred
   safely from filenames alone. The next audit must trace definitions and
   callers for high-conflict concepts, beginning with actor/NPC/player state.
3. `docs/` contains both current authority and historical/archive material.
   The router already says archive material must not override current specs;
   the inventory should make that boundary visible to humans and AI tools.
4. The website is large enough to distort repository traversal and should be
   treated as a separate product surface during the first pass.
5. The current working tree contains substantial pre-existing NPC lifecycle
   and hot-reload changes. Those changes must be labeled pre-existing during
   ownership analysis.

## Proposed audit order

### Phase 1 — repository truth

Map tracked versus ignored files, generated output, active manifests, build
inputs, runtime-loaded configuration, and archive material. Nothing is deleted
in this phase.

### Phase 2 — ownership truth

For each major concept, record the definition owner, state owner, mutation
owners, read callers, network serialization owner, hot-reload boundary, spec,
tests, and runtime evidence. Start with actor → player/NPC → weapon/combat →
network state because those are the suspected overlapping owners.

### Phase 3 — executable boundary

Identify what is genuinely kernel/OS/process lifetime and what can become a
hot package, system, schema, event, capability, resource, or policy. Measure
the executable inputs before proposing movement; a smaller file alone is not
proof of a safer or more hot-reloadable design.

### Phase 4 — cleanup decisions

Classify each item as `KEEP`, `CONSOLIDATE`, `MOVE`, `REWRITE`, `ARCHIVE`, or
`DELETE`, with confidence and evidence. Deletion happens only after the active
loader/build/test/runtime references are checked.

## Confidence rules

- **High:** directly confirmed by repository instructions, manifests, build
  inputs, or exact references.
- **Medium:** strongly suggested by structure or naming but still needs caller
  and runtime tracing.
- **Low:** inference from filename, age, or apparent duplication only.

## First human decisions requested

1. Keep the full `docs/`, `src/`, `include/`, tests, and tooling as the initial
   protected set while inventory proceeds?
2. Treat the website as a separate audit after the game runtime map?
3. Permit moving generated artifacts out of the repository working tree after
   we prove whether they are tracked or required by tooling?
4. For NPCs, approve actor/lifecycle/state ownership as the first deep audit?

