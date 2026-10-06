# Gold reference: Counter-Strike server preset in three prompts

Status: Gold reference behavior
Date: 2026-10-06
Repository: `C:\mimita-v9`
Reported model context: GPT-5.6, Medium, Luna

## Why this is gold

This is a reference for a small, successful AI-assisted feature loop. The
user described a real desired workflow, the agent inspected the existing
owners before changing code, and the result became a short command that can be
run from the game terminal:

```text
CT1
```

The implementation took three user prompts:

1. Ask whether aliases could chain commands and describe the desired
   Counter-Strike one-command launch.
2. Ask for a simple explanation of generalized server presets.
3. Approve implementation and request `CT1` as the first preset alias.

The user then reported that the feature worked after correcting the command's
letter case. The repository currently defines `CT1` in uppercase. The earlier
mention of `CC1` is treated as a conversational typo; the authoritative
configured alias is `CT1`.

## User-visible result

Running `CT1` from the game's terminal starts the first local server preset:

- map: `dust2cyberiav4` / Dust2 Siberia V4;
- mode: `counterstrike`;
- startup NPCs: disabled;
- Discord notification: disabled;
- automatic map rotation: disabled;
- local host: automatically joined;
- destination: gameplay instead of remaining in the main menu.

The short alias expands to one named preset command:

```json
{
  "CT1": "server_start_preset counterstrike_1"
}
```

The preset data is separate JSON:

```json
{
  "counterstrike_1": {
    "server_name": "Counter-Strike 1",
    "map": "dust2cyberiav4",
    "mode": "counterstrike",
    "startup_npcs": false,
    "npc_count": 0,
    "discord_notification": false,
    "map_rotation": false,
    "join_host": true
  }
}
```

## What made it work

### 1. The request was converted into one clear user action

The important product requirement was not “add several commands.” It was:

> “I am in the main menu, I type one short command, and I am immediately in
> Counter-Strike.”

That led to a single owner-level command,
`server_start_preset <preset>`, instead of asking the alias to coordinate a
long fragile list of unrelated commands.

### 2. Existing owners were reused

The implementation reused the existing paths for:

- JSON terminal aliases and semicolon expansion;
- local authoritative listen-server startup;
- Counter-Strike mode selection;
- direct localhost connection;
- pending-connect processing;
- the normal transition from menu to gameplay.

The new code supplies preset data and orchestration. It does not create a
second gameplay server, a second connection protocol, or a Counter-Strike-only
startup implementation.

### 3. The settings became data

The map, mode, NPC policy, Discord policy, rotation policy, and auto-join
choice live in `config/server-presets.json`. Adding another quick-start recipe
can therefore be done by adding another named JSON object and alias.

### 4. The implementation respected startup timing

Starting a server and immediately sending several follow-up commands can race:
the server might not exist yet when the next command runs. The preset command
starts the server first, then queues the existing connection path. The engine
handles the connection on its normal next-tick lifecycle.

### 5. The result was checked at multiple levels

The work separated:

- source ownership and command-path inspection;
- JSON parsing and value inspection;
- compilation and linking;
- executable identity via `--versioninfo`;
- live human gameplay acceptance.

The build and JSON checks passed. The final user report supplied the most
important live acceptance: the command worked after the exact alias spelling
was corrected.

## Useful AI behavior in this example

- It started by investigating instead of guessing whether aliases already
  existed.
- It found that semicolon chaining was already implemented.
- It identified the boundary between terminal aliases and server startup
  arguments.
- It explained the proposed architecture in child-level language before
  editing code.
- It chose one generalized preset command rather than hardcoding only a
  Counter-Strike special case.
- It preserved unrelated working-tree edits.
- It built the result and reported runtime/human acceptance separately.
- It noticed that the exact alias spelling mattered.

## What could have helped even more

### Exact command spelling up front

The fastest test instruction should always show the exact configured command in
a code block and explicitly say whether alias names are case-sensitive:

```text
CT1
```

The current alias resolver uses the configured token exactly. A future polish
could make aliases case-insensitive or print a helpful suggestion such as
`Did you mean CT1?` after an unknown command, but that would be a deliberate
command-surface behavior change.

### A live test in the same implementation turn

The build proved the code compiled and linked. A complete reference run should
also capture the live `events.jsonl` records for:

1. preset loaded;
2. local server started;
3. Counter-Strike mode applied;
4. localhost connection started;
5. client entered the map and gameplay.

That would make the document stronger as a reproducible runtime reference,
not only a source/build reference plus user confirmation.

### A preset-listing command

As more presets are added, a command such as `server_presets` would make the
feature discoverable without opening the JSON file. The first version did not
need that command to satisfy the user's immediate goal.

### More explicit success feedback

The command currently logs that the preset started. A future version could
also show a short in-game notification such as:

```text
Starting Counter-Strike 1 on Dust2 Siberia V4...
```

and then a connection-success notification once the player is authoritative
in the match.

## Reusable lesson

For small user-facing game features, the strongest pattern is:

```text
short user command
    -> named JSON preset
    -> one generalized owner command
    -> existing server/connection lifecycle
    -> visible gameplay result
```

That pattern keeps the user interaction tiny while keeping the implementation
general enough for future presets such as `FUN5`, `ZOMBIE1`, or `SANDBOXNPCS`.
