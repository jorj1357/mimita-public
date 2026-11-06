# FCS Team and Avatar Identity Repair

**Session:** 2026-11-05 16:40 EST (`2026-11-05T21:40:54Z`)
**Branch:** `20261011zt`
**Scope:** Implement the accepted FCS team/avatar/NPC plan and preserve the
separate late-join and unrostered-startup-NPC regressions.

## Existing working-tree state

The repository already had extensive dirty and untracked work before this
session. The FCS, networking, avatar, server, and actor-command files touched
here were already modified. Changes were applied incrementally; existing work
was not reverted or replaced. In particular, the surrounding collaborative
world-editing and dev-loop changes in `docs/specs/networking/networking.md` and
the in-progress NPC collision/startup work in `src/network/server.cpp` remain
outside this session's claims.

## Source changes

- `src/network/server-gamemode.cpp`,
  `assignMissingRoundModePlayers`: the old early return required
  `victory_condition == "rounds"`. FCS has `victory_condition: ""` and defines
  rounds through its authored `victory.type`/`rounds` data, so its active late
  join repair never ran. The guard now explicitly includes FCS. Roster
  serialization still only uses an actor's identity when it is a real roster
  member with a valid team and role.
- `src/network/server-gamemode.cpp`, roster construction: FCS roster rebuilds
  remove leftover low-ID NPCs that lack explicit mode team/role identity and
  write a bounded removal event. Human role assignment now writes the forced
  role avatar into the server player identity and logs identity/avatar
  transitions.
- `src/network/server.cpp`, startup NPC creation: FCS generic `--npcs` startup
  creation is suppressed because FCS owns its NPC set through its authored
  roster. The separate `npc_spawn.policy` does not control this startup path.
- `src/network/server-avatar-network.cpp` and `src/network/server-players.cpp`:
  forced live-change policy and avatar allow/deny checks are enforced at
  manifest and spawn boundaries, including mode policy when no role is yet
  resolved. Rejections identify the actor, mode/role policy, requested avatar,
  fallback, and reason. Accepted avatar identity transitions are recorded.
- `src/terminal/actor-commands.cpp`, `actorlist`: explicit server/client
  inspection reports controller, team, role, avatar, and policy outcome, then
  writes a bounded `actor.presentation.audit` record to that process's
  structured journal. An empty client roster also produces an explicit empty
  audit record.
- `docs/specs/gamemodes/funcounterstrike.md` and
  `docs/specs/networking/networking.md`: clarified the legacy-field gate,
  server-authoritative role avatar identity, snapshot convergence, mode policy,
  and unknown-team presentation contract.
- Added distinct regression records and indexes for missing late-join identity
  and the generic startup NPC. The reported Boomer identity remains unresolved;
  the captured run identified `demongirl`.

## Documentation review

Read `docs/ROUTER.md`; the FCS, general gamemode, actor-role, networking,
debug-logging, canonical JSONL, runtime validation, dev-loop quality and
multi-client workflow documents; terminal command architecture; regression
indexes; and the specification, logging, and documentation review skills.
The added specification statements agree with the existing neutral unknown
team rule and the per-process journal contract.

The required whole-docs TODO scan found the user note at
`docs/specs/avatar/avatar-editor.md:9`: “also todo clean this doc!!!!!! cant
have so much stuff inthis doc just spam”. The router says not to edit such
notes. Suggested expansion: reorganize the avatar guide around user tasks while
keeping normalization/loading ownership, fields, and diagnostic evidence easy
to find. That file was left unchanged.

## Validation evidence

- `git diff --check` passed for the changed implementation/spec files; Git only
  reported existing LF-to-CRLF normalization warnings.
- Canonical dev-loop build artifact
  `2026-11-05T21:30:27.625680Z:a316f7111d218cf2317627776c1a032e2c4ee936429e4e813dd5ed8bcecc934e`
  completed successfully from source generation 24. All changed C++ files in
  this session predate that build snapshot.
- `--versioninfo` on that exact artifact reported
  `EVENTS_JSONL_PATH=logs/11-05-2026/20261105_163335/events.jsonl` and the same
  executable path/build artifact identity. This was an identity-only launch,
  not FCS gameplay validation.
- Later shared source continued changing. The dev-loop status showed source
  generation 44 building, latest successful generation 24, and a prior build
  attempt marked failed without a retained compiler reason in the status
  output. No failure is attributed to this patch; the active build was left
  alone.
- The active run `20261105_212323` is Vehicle Arena on the older build
  `2026-11-05T21:22:19.587876Z:9e25de08...`, with `auto_restart=false` and a
  live server plus two clients. It was not stopped or replaced.
- The historical FCS run manifest remains at
  `logs/2026-11-05/20261105_200615/run.json`, but its server/client JSONL files
  have since been pruned. Earlier direct inspection found client 2's local
  team `255` and no role, and server actor 1000 with avatar `demongirl` at tick
  0. These findings are recorded as historical evidence; fresh JSONL evidence
  is required for re-verification.
- No unit or synthetic tests were added or run.

## Runtime and human acceptance still required

Run FCS through an isolated dev-loop session so the active Vehicle Arena
session remains untouched. Use the NPC dev client and regression-checker client;
inspect server and both client `EVENTS_JSONL_PATH` values. Confirm both clients
receive valid authoritative teams/roles, teammates are not shown as enemies,
all replicas use their role avatar, generic startup NPCs are absent, and
mode-rostered NPCs remain. Attempt a disallowed avatar change and verify the
server rejection. Review the reported Boomer only after a current journal names
its actor ID and avatar. No live FCS gameplay or human visual acceptance is
claimed in this session.
