// 2026-11-20
/* purpose
* record the coordinated melee claimed-shape change and two-client harness
* preserve the build gate and the unproven runtime boundary
* this file DOES NOT claim live melee parity or human acceptance
*/

# Melee claimed-shape validation and two-client harness

# Task

- Task ID: melee-parity / melee-claims / melee-pvp
- Session ID: session-6d05a98105da
- Owner: melee-parity (opencode)
- Focus: make the visible melee contact the same shape the client predicts,
  sends, and the server validates; add NPC and PvP evidence paths
- Summary: the server no longer runs a second independent body-volume check
  after the client-claimed shape passes; claim events now expose first-divergence
  stages; the melee harness starts with the Mace already equipped, stops
  airborne horizontal drift, ground-validates near-player spawns, and can launch
  a second isolated client for PvP. A parity report correlates contact IDs.
- Status: NOT_TESTED — root game build d55095 (source 47, gen 391) was still
  building; isolated test runs are refused while a build is active
- Date, time, timezone: 2026-11-20T00:55:00+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: uncommitted shared-checkout edits

# Pre-existing changes

The checkout already contained extensive unrelated edits. This session preserved
them. Only the files listed below were touched here.

# Requested behavior

For Mace, Chainsaw, and SwordSword, the contact the player sees and hits must be
the same contact the client predicts, the same claim sent to the server, the
same shape the server validates historically, and the authoritative damage.
Prove it on TDM `dust2cyberiav4` with both NPCs and a second player, using the
shared agent coordination brain.

# Specification alignment

- `docs/specs/weapons/melee/melee-weapons.md`: Mode 1 physical contact, client
  prediction, historical server validation.
- `docs/specs/networking/contact-authority.md`: one contact contract, one
  damage sink, server referee.
- `docs/specs/dev-loop/agent-coordination.md`: shared brain, edit leases,
  decisions, and no competing build authority.

# Exact implementation changes

## `src/network/server-packet-handlers.cpp`

- `handleGenericMeleeContactBatch` now sets `contact.validateHitVolume = false`
  after the handler has already validated the client-claimed OBB/capsule against
  the rewound target. The second independent body-volume check no longer
  reinterprets the contact.
- Claim rejection events carry `stage` (`claim_time`, `bounds`, `eligibility`,
  `generation`, `shape`, `reach`, `occlusion`, `contact_resolution`), and the
  accepted event carries target kind/generation, claimed center/half, and the
  claimed-shape validation mode.
- Batch-level rejection now emits `melee.contact.batch.rejected` with
  `attacker`, `attacker_inactive`, or `attacker_generation` stages.

## `src/actor/player-ai.cpp`

- Airborne melee AI stops horizontal movement instead of walking off ledges while
  `ground.onGround` is false.
- Bounded fallback supply increased to 12 attempts, 8 s apart.

## `src/terminal/npc-commands.cpp`

- `npc_spawn_near` resolves the floor with `NpcNavigation::groundHeightAt` before
  sending the normal spawn request.

## `config/weaponsets.json`

- Added weapon set 31 (`melee-test`): Mace, Chainsaw, SwordSword, Revolver,
  Nothing. The player starts with Mace at slot 1, removing the mid-life equip
  delay that left the harness without a melee shape.

## `devscripts/dev-profiles/tdm-melee-validation.json`,
`devscripts/scenarios/melee_tdm_ai_v1.json`

- Use weapon set 31.

## `devscripts/dev_loop/parallel_test_runs.py`, `devscripts/dev-loop.py`,
`devscripts/dev_loop/cli.py`

- Added `--start-test-client --run-id <id>` to launch a second isolated client
  against the existing run server, with its own manifest record, journal, and
  command queue for PvP.

## `devscripts/melee-parity-report.py`

- Correlates client `melee.client.contact_claim` and batch events with server
  accept/reject decisions by `contact_id`; reports unsent claims, unanswered
  claims, rejection stages, and predicted-versus-accepted damage mismatches.
  `--strict` fails on a broken chain.

# Validation

- `python -m py_compile` passed for the changed Python files.
- `python -m json.tool` passed for `config/weaponsets.json`, the profile, and
  the scenario.
- Parity report fixture: one claim, one batch, one accepted decision, one
  matching damage value, `--strict` exit 0.
- `--start-test-run` refused with `root dev-loop has a pending or active EXE
  build`; the root game build was still active.
- No new live melee evidence in this session.

# Measured evidence

Run `20261119_234623`:
- zero client melee claims and zero server batch decisions;
- AI was at z ~232.9 with `on_ground=false`, `target_id=0`, `target_distance=0`
  after the client lost its player around 170 s;
- NPCs respawned at z ~237/282; the AI fell to negative z before resetting;
- the client later sampled a mace shape with actor `4294967294`, evidence that
  the local player identity was already invalid;
- reconnect lookup rejected with `token_not_found` after slot release.

# Regression review

- Regression entry appended: no
- This is an unproven harness/validation change, not a confirmed gameplay
  regression.

# Human acceptance

- Visual review: not performed
- Gameplay review: not performed
- Multiplayer review: not performed
- Still unverified: live NPC melee contact, live PvP contact, damage parity,
  reconnect-free sample

# Agent handoff context

- Owner: melee-parity
- Focus: publish the current artifact, then run NPC first and PvP second
- Files to check: `src/network/server-packet-handlers.cpp`,
  `src/combat/melee-client.cpp`, `src/actor/player-ai.cpp`,
  `config/weaponsets.json`, `devscripts/dev_loop/parallel_test_runs.py`
- What another agent should not redo: do not add a second body-volume validation
  to the melee claim path; do not start a competing build or dev-loop
- Next exact step: see the commands below

# Next commands

```text
python devscripts/dev-loop.py --start-test-run --test-profile tdm-melee-validation --test-launch-mode 41 --test-map dust2cyberiav4 --json
python devscripts/dev-loop.py --start-test-client --run-id <run-id>
python devscripts/dev-loop.py --run-id <run-id> --client client-01 --command "team_assign 1 1"
python devscripts/dev-loop.py --run-id <run-id> --client client-01 --command "team_assign 2 2"
python devscripts/dev-loop.py --run-id <run-id> --client client-01 --command "healthme 999999"
python devscripts/dev-loop.py --run-id <run-id> --client client-02 --command "healthme 999999"
python devscripts/dev-loop.py --run-id <run-id> --client client-01 --command "player_control ai melee mace"
python devscripts/dev-loop.py --run-id <run-id> --client client-02 --command "player_control ai"
python devscripts/melee-parity-report.py <run-dir> --strict
python devscripts/dev-loop.py --stop-test-run <run-id>
```

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- New file: delete `devscripts/melee-parity-report.py`
- Config: remove weapon set 31 and restore the profile/scenario to set 29
- Reverse only files touched by this session; preserve pre-existing changes.
