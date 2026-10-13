2026 10 13 1600 jorj - 
since we are using docuemnts more often thats aweesome
so 
write in the .md what u want the exact behavior to be
adn then reference that doc when working on things 
for example, C:\mimita-v9\docs\specs\dev-loop\dev-loop.md  not 
not super specific
so 1. make it super ultra specific in explaingin what we want it to  do 
2. work with the code and stuff and then mak the script match the spec as best as it can 

put logs + run the exe + see the value of the logs + use that info to make decisions e.g. confident theres a clear solution = implement, not confident yet = imrpove/edit/cahnge/add/remove/etc logs   - 
continue, getting more evidence for/against and building evidence to support claim that  whatevre fix  u want to implement is the best solution for it right now 

C:\mimita-v9\docs\specs\colliisons\collisions.md

C:\mimita-v9\docs\specs\debug-logging\AI-fix-live.md

work according to ai fix live  , to get this to be true:

events.jsonl shows a consistent  240fps or more when using the dve loop script in mode 11  with the avatar being equipped is "monstertest1"


11": {
      "name": "atdm map collision lag test 2026 10 11",
      "server_args": [
        "--no-npcs"
      ],
      "client_args": [
        "--startup-command",
        "teleport -43, -28, 290",
        "--startup-command",
        "m1"
      ],
      "map": "atdm"  
    }

i switched it to "m1" bc that equips the  boomer avatar
so , keep that there, and work according to the ai live fix doc, and get it so  1. the monster avatar is equipped, and 2. real .exe is at 240fps with collisions working and monstertest1  avatar equipped , i tested  just now adn i was at like 5-10 fps not good 
it sas fps is 4 or 5  avg is 10 or 9 or 8  dropping 

ING uptime=00:03:55 visibility=visible
[DEV STATUS] PIDS: server=28420 client=[28960]
[DEV STATUS] EVENTS: ]8;;file:///C:/mimita-v9/lo[DEV STATUS] MODE: 11 (atdm map collision lag test 2026 10 11)
[DEV STATUS] BUILD: running=- latest=2151 match=False
[DEV STATUS] RUN: RUNNING uptime=00:03:55 visibility=visible

this the log im using 

run the exe , look at events.jsonl for results of ur changes, and work based on the logs to get the success condition to be true

todo 2026 10 11 2028 jorj  put this in here gernally , like the above is not clean i want to implement in a  general  applies to any task way 

# Live AI fix procedure

Date: 2026-10-11

## Purpose

This is the evidence-first procedure for investigating and fixing a gameplay,
editor, presentation, configuration, network, physics, audio, or runtime
problem while MiMITA is being developed. It turns a user-visible symptom into
an owner-level diagnosis, then proves the smallest fix in the real executable.

This is a workflow specification. It does not replace `AGENTS.md`,
`docs/ROUTER.md`, the selected feature specification, architecture documents,
or build/runtime procedures.

## Does not

- Treat a successful build, test, or config parse as runtime proof.
- Guess that the executable is stale before tracing the complete data path.
- Add a synthetic self-test before observing the real scenario.
- Add unmanaged debug files or a feature-specific logging sink.
- Claim visual, audio, multiplayer, network, or human acceptance without
  directly observing the relevant result.
- Change the specification silently to make the existing implementation look
  correct.

## 1. Route the task and freeze the observable failure

Read `AGENTS.md`, `docs/ROUTER.md`, the selected specification, related
architecture, the runtime workflow, the focused skills, and current changelog
state. Search the repository before proposing a new owner.

Write the failure in one sentence containing:

```text
actor or screen, input/action, expected state, actual state,
runtime identity, and exact evidence location
```

Examples:

- “Avatar Editor selecting `mimita-boomer-v6.glb` should preserve Blender's
  node transforms; the preview applies default-body offsets; run identity and
  journal are not yet known.”
- “The player enters a real damage volume but health does not decrease; the
  expected server damage event is absent from the fresh shared journal.”

If the expected behavior is not specific enough to name an observable state,
stop at `NEEDS_SPEC_DECISION` and improve the specification before changing
code.

## 2. Identify the authoritative live run

Use the repository dev-loop status/control path to identify the current
profile, client/server processes, build, generation, and journal. When the
journal is unknown, run the exact executable's `--versioninfo` and use its
reported `EVENTS_JSONL_PATH`. Never infer a journal from a similarly named old
run. Do not launch a competing loop.

The canonical path is:

```text
python devscripts/dev-loop.py
```

Use the documented status/control options when a loop already owns the
runtime. Preserve unrelated user-owned processes.

## 3. Trace the complete input-to-output chain

Name the owner and evidence at every boundary. Use the route appropriate to
the feature, not a weapon-specific template:

```text
user input or external event
 -> command/UI/network entry
 -> authoritative state/policy decision
 -> serialization or handoff
 -> client/server or async boundary
 -> applied state
 -> physics/presentation/output
 -> structured journal and visible result
```

For each boundary record expected value, actual value, and the first point
where they diverge. Keep identity, configuration, source selection, and
presentation as separate values. For an avatar GLB, for example, distinguish
model path, root name, required body nodes, transform source, authored
transforms, applied overrides, async completion, and final visible model.

## 4. Add bounded owner-level evidence

If the existing journal cannot identify the first divergence, add or improve a
`StructuredLogger` event at the owner that makes the decision. Include only
joinable fields:

- event name, UTC time, process/PID, run/build/generation identity;
- tick or request/operation ID when applicable;
- actor/avatar/model/entity identity;
- input/requested value and resolved value;
- source/policy, old state, new state, result, and rejection/error reason;
- the next boundary's handoff or completion state.

Log transitions and bounded samples, not every render frame. Use the existing
category/filter configuration and flush through canonical `events.jsonl`.
Do not create a second file. If evidence is still ambiguous, add the next
smallest owner-level field and repeat the real scenario.

## 5. Run the real scenario before deciding the fix

Publish a newly identified executable through the dev loop after source or
logging changes. Capture its exact `--versioninfo` output and journal path.
Exercise the actual user flow: click the editor control, enter gameplay,
teleport to the damage volume, join a second client, reload a model, or use
the relevant terminal command. Do not substitute a unit test for a runtime
behavior that has not been observed.

For an avatar model case, the minimum scenario is:

1. load the target avatar;
2. select and apply the GLB in the Avatar Editor;
3. save, reload, and apply it again;
4. enter the real game with that avatar;
5. inspect the fresh journal for model selection, validation, transform source,
   async completion, and runtime application.

For a damage-volume case, the minimum scenario is:

1. start the exact fresh executable;
2. enter the real map/mode;
3. move or teleport the player into the volume;
4. inspect health, volume-contact, damage, and replication events in the same
   fresh journal.

## 6. Decide from evidence

- If the first divergence names one owner and the intended behavior is clear,
  implement the smallest fix at that owner.
- If the journal is incomplete or contradictory, improve logging and repeat
  the real scenario.
- If the behavior is an intentional policy restriction, document it instead
  of bypassing authority.
- If code and specification disagree, quote the specification, record the
  disagreement, and either align code or request a human specification
  decision.

Do not tune downstream visuals, add retries, or rewrite a packet before the
first missing/incorrect boundary is known.

## 7. Rebuild, rerun, and separate claims

After the change, repeat the same scenario with a newly identified run. Report
these claims separately:

1. source/configuration change and exact owner;
2. build/dev-loop identity and build result;
3. fresh journal path and relevant event values;
4. first divergence before the fix and closed boundary after it;
5. directly observed visible, audio, network, or multiplayer acceptance;
6. human review or limitations still required.

Build success proves compilation/linking only. Journal evidence proves the
instrumented runtime path. Human acceptance proves the experience.

## Completion

Every repository-touching session creates exactly one final changelog file in
the current date folder. The changelog records routed documents, focused skill
results, pre-existing edits, exact source/doc changes, build and runtime
evidence, regressions if confirmed, and human review still needed.
