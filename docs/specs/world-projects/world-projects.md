<!--
Purpose: Define portable creator-owned world projects, versions, assets, and
world-scoped player data for MiMITA.
Audience: World, editor, networking, and persistence implementers.
Does not: Define RPG combat balance, a platform currency, or a cryptocurrency.
Status: Desired behavior and architecture recommendation. The local map
project CLI is an initial offline foundation; in-game integration and
multiplayer collaboration remain unimplemented.
-->

# MiMITA World Projects and Distributed Data

## 1. Goals

A world is a creator-owned, versioned project that can be opened, forked,
edited live, saved, exported, and played without requiring the original creator
or mimita.fun to be online. The same project model should later support other
creator tools, including music collaboration, without making those tools part
of the RPG editor's first slice.

Separate these data domains:

1. **Project source:** authored map, chunks, generation rules, NPC presets,
   interactions, world overrides, scripts when supported, and package metadata.
2. **Published version:** immutable project tree and dependency identities.
3. **Live edit session:** the host's authoritative working version and ordered
   edit operations.
4. **Player profile:** account identity, encrypted recovery data, and
   world-lineage-scoped progression records.
5. **Platform economy:** future shared items or currencies. It is not the same
   source of truth as a world's gold, XP, or custom currencies.

## 2. Project identity, versions, and forks

Every project has a stable project ID, owner account ID, parent project/version
when forked, a human-readable title, visibility, license metadata, and an
immutable version history. Every published version records its parent version,
content-tree hash, generator version, dependencies, license snapshot, author
and contribution lineage, and publication time.

The creator's working copy is mutable. A published version is immutable.
Opening an older version selects that exact version; updating is an explicit
operation that creates a new version. A fork keeps its parent lineage and
starts an independent history. A disconnected editor can continue from its
last confirmed state as a fork; it cannot silently write into the hosted
session.

Public is the default visibility and a public project is forkable. A project
may instead be private; private remains private until the owner changes it.
Public code defaults to MIT. Public media defaults to a reuse license requiring
creator attribution (CC BY 4.0 is the initial candidate). Each dependency keeps
its own license and provenance. A project may only redistribute an included
asset when that asset's license permits it. Record authorship and source
version through every fork and derived asset.

## 3. Files, cache, and delivery

Project manifests describe files and dependencies; binary media is stored as
content-addressed blobs and referenced by stable IDs and hashes. The installed
game is the runtime: a world package is data and must not replace or execute an
arbitrary downloaded game executable. Packages are validated before use. A
future scripting package is a separate, sandboxed extension and is disabled
until its capability and network rules are implemented.

World selection pins one immutable version. Download the manifest and the
minimum assets required to enter and play the current area first. Fetch distant
chunks, optional variants, and source files on demand. Verify each blob against
its declared content hash before loading it. Keep cached blobs reusable across
projects and versions. Peers and community caches may serve public blobs when
the creator is offline; availability is independent from who can author or
validate the project's state.

The exact per-user folder must use MiMITA's shared data-path owner when one is
established. Until then, do not scatter project files into repository `config/`
or authentication-token paths. Preserve projects and recovery data across game
updates. Export must produce a self-contained manifest plus every dependency
the selected license permits the exporter to include, with an attribution
inventory and hash report.

## 4. Live editing and permissions

One host server owns the authoritative edit-session state. Each client submits
an edit intent with an operation ID, author account ID, target project/version,
base revision, target object/property, and requested change. The host validates
identity, capability, target existence, value bounds, and base revision, then
orders accepted changes and broadcasts them. A client never writes directly
into the host's project files.

The owner starts with all editor capabilities and may grant named capabilities
to account IDs, including world geometry, chunks/generation rules, NPCs,
weapon overrides, interactions, undo, and publish. Revoke takes effect at the
next accepted operation boundary. Rejected requests return a stable reason and
the authoritative revision.

Edits to independent properties merge. Concurrent edits to the same property
are surfaced as a conflict; the author chooses whether to reapply their value
against the latest revision or discard it. The server serializes accepted
operations. Each editor may undo their own most recent accepted operation;
the owner may undo any operation. Undo is a new inverse operation in history,
not deletion of the earlier record.

Each accepted edit receives a monotonically increasing session sequence. Clients
acknowledge the latest sequence. A gap triggers ordered replay from the host's
bounded operation history; if the gap is no longer retained, the host sends a
validated full snapshot and its revision. Heartbeats indicate session
connectivity; they do not substitute for sequence acknowledgements or replay.

Each client keeps a rolling temporary recovery copy of the latest confirmed
world state and unconfirmed local edits. On disconnect, the client can resume
from the confirmed revision or continue on an independent fork. On clean host
shutdown, persist the live state as a recoverable draft and offer it as a new
immutable project version. Do not discard edits because a client or host
disconnected unexpectedly.

## 5. World-scoped player data

The MiMITA account ID identifies the player. A world lineage owns its own
progress schema, so a fork may explicitly preserve or reset inherited progress.
Joining a world loads that player's records for the selected lineage. World
records are separate from platform-wide account statistics and future shared
inventory.

World hosts may submit rewards produced by their published rules and
authoritative live session. A reward record identifies the player account,
world lineage, exact world version, host identity, event ID, and schema version.
Do not let a host replace or erase another player's prior world history. Reject
replayed event IDs. Corrections are explicit later records. Schema evolution is
additive: adding a field such as `diamonds` provides its declared default for
existing profiles while preserving existing fields and records. Breaking
migrations require a new schema version, a reversible migration plan, and
recovery coverage.

Encrypted account backups may be replicated to peers or community storage, but
only the account owner can decrypt them. Recovery credentials stay under the
player's control. Do not publish passwords, password hashes, authentication
tokens, or private keys to a public ledger or content network. The account ID
remains the user-facing identity; cryptographic keys prove control of records.

## 6. Distributed-storage recommendation

Evaluate the storage protocol separately from project semantics. Compare:

| Approach | Useful properties | Costs / limits |
|---|---|---|
| Blockchain consensus | Public ordered history and shared validation without trusting one host. | Requires a consensus and abuse-resistance model; every node must validate the same rules. It does not by itself store large assets, keep creators online, or make private profile data private. |
| Signed host event log | Simple authoritative ordering for one live world; identity, project version, operation IDs, and audit trail can be verified. | A host can submit dishonest rewards or stop serving data. Signatures prove authorship, not that gameplay was honest. |
| Content-addressed peer storage | Immutable blobs can be verified and copied by peers or caches without a central content origin. | A content hash proves bytes, not availability, permission, or authority. A peer/cache can disappear. |

**Recommendation for the first implementation:** use immutable content-addressed
project versions and dependencies, a server-ordered signed edit/reward log for
each live world session, and encrypted player-owned backups. Allow peer copies
and optional community caches to distribute public content. Keep optional
central services as convenience discovery/identity mirrors, never as the only
source of a project's bytes or its world-scoped save. Do not implement a new
blockchain or cryptocurrency for the editor slice. Revisit consensus only if a
specific platform-wide value transfer requires it.

This design is decentralized for storage and portable project history, but it
does not make a custom-world host trustless. A host may fabricate its own
world-scoped rewards. Label their world/version provenance and keep competitive
or platform-wide rewards outside this trust model until separately designed.

Content addressing separates data identity from location: the same project
version can be retrieved from a creator, peer, or cache and verified by hash.
Bitcoin-style consensus is designed to agree on a shared public transaction
history, not to serve world assets or private account backups. IPFS's own docs
also warn that public peer discovery exposes provider IDs and content IDs;
private account backups therefore require encryption before distribution.

## 7. Extensibility and scope boundaries

Future world scripts use a sandboxed Lua profile first, with capabilities for
named events/actions and shared behavior reusable by many objects. The editor
must not allow a world script to call arbitrary native functions, access files,
credentials, or unbounded networking. Define quotas, deterministic/server
authority rules, versioning, and failure behavior before adding a script editor.
Additional languages require the same host-defined capability interface.

The first world-editor implementation does not include a script editor, a
platform cryptocurrency, music collaboration, trustless competitive rewards,
or a scale guarantee for arbitrarily many players. C++ source inspection and
source-level forks are an advanced creator/developer workflow, separate from
loading public world content.

## 8. Acceptance requirements

- Forking and checking out a version preserves its project ID lineage, exact
  content hashes, creator credits, and dependency licenses.
- Public packages can be fetched from a peer/cache and every blob is verified;
  a missing blob is reported without silently substituting another version.
- Two clients connected to one host converge on the same accepted edit
  sequence, recover gaps, and keep their last confirmed local recovery copy.
- A client cannot write to the host project by changing local files or by
  submitting an unauthorized capability.
- World progress remains scoped to the selected lineage; replayed rewards are
  rejected, and additive schema changes preserve existing values.
- Encrypted account backups can be exported, restored, and read only by the
  account owner. No credentials appear in public records or logs.
- A private project remains private until its owner changes visibility. A
  public fork preserves typed licenses and author attribution.

## 9. Initial local implementation

`python devscripts/world-project.py` remains an offline importer/thin utility
for map-project JSON in the per-user `Mimita/WorldProjects` folder.

The native owner is now `src/worldproject/world-project-store.*`:

- format-2 manifests and immutable SHA-256 map versions;
- fork lineage, checkout with hash verification, dependency metadata, package
  export/import with per-file hash verification, and recovery drafts;
- format-1 migration that preserves the source manifest as
  `project-format1.json` instead of deleting it;
- `worldproject` terminal commands for create, open, save, fork, versions,
  export, import, drafts, recovery, and migration.

`src/worldproject/world-edit-session.*` owns typed operations, per-property
revisions, capability checks, same-property conflicts, independent-property
merges, own/owner undo, acknowledgements, bounded replay, and snapshot
generation. `src/gamemode/map-edit-applier.*` applies accepted operations to
`MapConfigRegistry`, and the `worldedit` terminal command exercises the local
host path with journal events (`world-edit.accepted`, `conflict`, `rejected`,
`snapshot`). This is still **not** connected to the client/server packet
family: `worldedit` is a local host-session validation path, not multiplayer
authority.

The map model now parses and saves authored chunks and connectors, generation
zones, NPC presets, weapon overrides, and interaction definitions. Map-authored
interactions are translated into shared `InteractionSystem` targets by
`src/interaction/world-interaction-runtime.*`; action ids resolve through
`InteractionActionRegistry`, and hold mode advances on fixed simulation ticks.
`src/procedural/procedural-world.*` exposes a deterministic zone/chunk hash
that includes the generator version and dependency identity.

Still unimplemented from the full design: in-game project-browser panel,
client/server world-edit packets and snapshot transfer, OpenPGP/librnp identity
and signing, peer/community content distribution, encrypted profile backups,
and world-scoped progression. The SHA-256 map hash is not an account signature
or a complete project-tree hash.

## References

- [World edit command reference](world-edit-commands.md)
- [World generation mode JSON](worldgen-mode-format.md)
- [Live project layer](../../architecture/live-development/project-layer.md)
- [Live authored world](../../architecture/live-development/live-authored-world.md)
- [Map Entity Editor](../entity-editor/entity-editor.md)
- [Networking](../networking/networking.md)
- [Data saving](../data-saving/data-saving.md)
- [Procedural worlds](../procedural-infinite-world/procedural-infinite-world.md)
- [Bitcoin whitepaper](https://bitcoin.org/bitcoin.pdf)
- [IPFS content addressing](https://docs.ipfs.tech/concepts/content-addressing/)
- [IPFS privacy and encryption](https://docs.ipfs.tech/concepts/privacy-and-encryption/)
- [Creative Commons licensing](https://creativecommons.org/share-your-work/cclicenses/)
- [OWASP Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
