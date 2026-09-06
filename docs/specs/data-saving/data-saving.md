# MiMITA Persistent Player Data, Rewards, Autosave & Leaderboards

**Document date:** 09-06-2026  
**Status:** Desired behavior / implementation specification

---

# 1. Purpose

MiMITA should have persistent player progression tied to each player's MiMITA account.

The complete player experience should work end-to-end:

1. Player launches `mimita.exe`.
2. Player creates an account or signs in.
3. Player joins a game.
4. Player kills other players.
5. Player receives kills, gold, and XP.
6. Player dies and receives deaths.
7. Player can earn separate XP from NPC kills.
8. The host periodically saves changed player data.
9. The MiMITA database permanently stores the player's totals.
10. The MiMITA website displays these totals on player profiles.
11. Public leaderboards rank players by those totals.

The system should be lightweight, server-authoritative, resistant to duplicate rewards, and reusable by future MiMITA systems.

---

# 2. Core Principle

The client does **not** decide its permanent stats.

The authority chain is:

```text
Client
    ↓
Host game server process
    ↓
MiMITA VPS / database
```

The host game server determines what happened during gameplay.

The MiMITA database stores the permanent totals and is the final persistent source of truth.

A client may immediately display confirmed gameplay information, but it cannot independently award itself permanent:

- Gold
- XP
- Kills
- Deaths
- Playtime

---

# 3. Initial Persistent Player Data

For V1, every MiMITA account needs the following permanent statistics:

```text
gold
xp
kills
deaths
playtime_ticks
```

These are the primary public statistics.

NPC-specific reward tracking must be kept separate from PvP statistics.

NPC kills must **not** increase the player's normal PvP `kills` value.

The initial system should remain small.

Do not add unnecessary progression statistics merely because they may be useful in the future.

The purpose of V1 is to create a reliable persistent-data pipeline that can later be expanded.

---

# 4. Player vs Player Kill

When Player A kills Player B:

Player A receives:

```text
+1 kill
+50 gold
+100 XP
```

Player B receives:

```text
+1 death
```

Example:

```text
Jorj1357 kills PlayerTwo.
```

Jorj1357 sees:

```text
+1 kill on PlayerTwo
+50 gold
+100 XP
```

PlayerTwo sees:

```text
+1 death from Jorj1357
```

These changes are added to the server's authoritative in-memory player state.

The player's persistent state is then marked as dirty and waiting to be saved.

---

# 5. Self Kill

A self kill counts as a death.

It does **not** count as a kill.

It awards:

```text
Killer:
+0 kills
+0 gold
+0 XP

Player:
+1 death
```

The death popup should identify the cause as the player themselves where appropriate.

Example:

```text
+1 death from yourself
```

A player must never be able to farm gold, XP, or kills by killing themselves.

---

# 6. Environmental Death

An environmental death counts as:

```text
+1 death
```

It does not award another player:

```text
+0 kills
+0 gold
+0 XP
```

The death source should be represented as:

```text
environment
```

Example popup:

```text
+1 death from environment
```

---

# 7. NPC Kill

NPC rewards are separate from PvP rewards.

When a player kills an NPC:

```text
+0 PvP kills
+0 gold
+10 XP
```

The player should deliberately see the zero-gold popup.

Example:

```text
+0 gold
+10 XP
```

Showing `+0 gold` is intentional.

It communicates clearly that:

> NPCs can give XP, but NPCs do not give gold.

An NPC kill must **not** increase the public PvP kill counter.

NPC-related tracking should remain logically separate from PvP tracking so it can be expanded later without corrupting PvP statistics.

---

# 8. Reward Popup System

Rewards and stat changes should produce visible client-side popups.

Examples:

```text
+1 kill on PlayerTwo
+50 gold
+100 XP
```

NPC:

```text
+0 gold
+10 XP
```

Death:

```text
+1 death from PlayerTwo
```

Environmental death:

```text
+1 death from environment
```

---

# 9. Popup Colors

Default visual behavior:

### Kill

Normal configured kill/stat color.

```text
+1 kill on PlayerTwo
```

### Gold

Gold/yellow.

```text
+50 gold
```

### XP

Turquoise.

```text
+100 XP
```

### Death

Configured death color.

```text
+1 death from PlayerTwo
```

These are defaults, not hardcoded permanent visual rules.

---

# 10. Hot-Reloadable Popup Configuration

Popup appearance must be hot reloadable.

Changing the configuration should not require restarting MiMITA.

The popup configuration should support at minimum:

```text
text color
text size
screen X position
screen Y position
duration in ticks
spacing / offset
stacking behavior
```

Reward amounts are **not** part of this hot-reload visual configuration.

The visual system and reward rules should remain separate.

---

# 11. Tick-Based Popup Timing

Popup lifetime must be measured in simulation ticks, not rendered frames.

MiMITA currently assumes:

```text
60 ticks = 1 second
```

For example:

```text
180 ticks = 3 seconds
300 ticks = 5 seconds
```

Do not implement:

```text
show popup for 300 rendered frames
```

because different devices can render at different frame rates.

A low-power device running at 30 FPS and a high-power device running at 300 FPS should receive approximately the same gameplay-time popup duration.

Rendering can occur at any frame rate.

Gameplay timing remains tick based.

---

# 12. Overlapping Popups

The popup system must support configurable stacking behavior.

One supported configuration must deliberately allow every popup to appear at the same location.

Example:

```text
+1 kill on PlayerTwo
+50 gold
+100 XP
```

may visually overlap and become somewhat difficult to read.

This is intentional and should remain possible.

Do not automatically "fix" overlapping popups unless the configuration requests another behavior.

The system may later support:

```text
overlap
vertical stack
horizontal stack
queue
combine
```

but overlap must remain a valid configuration.

---

# 13. Server-Authoritative Rewards

Permanent progression cannot be awarded solely because a client reports:

```text
"I killed PlayerTwo."
```

The host server must confirm the gameplay event.

Conceptually:

```text
Client action
    ↓
Host simulates / validates action
    ↓
Host confirms death
    ↓
Host generates authoritative reward
    ↓
Host updates authoritative player state
    ↓
Host tells client what happened
    ↓
Client displays popup
```

The client displays the result.

The host determines the result.

---

# 14. Database as Persistent Source of Truth

The database is the permanent source of truth for player totals.

When a player enters an authenticated game session, the host should obtain the player's existing persistent state.

Example:

```json
{
  "user_id": 1357,
  "gold": 882935,
  "xp": 458024,
  "kills": 8429,
  "deaths": 9385,
  "playtime_ticks": 39294821
}
```

The host then maintains the current session's authoritative state in memory.

Changes during gameplay modify that server-side state.

The database is periodically synchronized with the authoritative host state.

---

# 15. Dirty-State Tracking

Do not constantly write player data to the database.

Each player's persistent state should have a dirty state.

Example:

```text
Player joins:
dirty = false

Player gets kill:
dirty = true

Player gets XP:
dirty = true

Playtime changes:
dirty = true

Autosave succeeds:
dirty = false
```

Only players with changes that need persistence should be included in the save batch.

---

# 16. Autosave Interval

The normal autosave interval is:

```text
60 seconds
```

At 60 simulation ticks per second:

```text
60 × 60 = 3,600 ticks
```

Therefore:

```text
AUTOSAVE_INTERVAL_TICKS = 3600
```

Do not base the autosave on rendered frames.

---

# 17. Batched Autosaving

Autosaving should be lightweight.

Do **not** make one database request for every stat change.

Do **not** make one database request for every kill.

Do **not** make unnecessary individual requests for every player when one batch can represent the same information.

Instead:

```text
Game events occur
        ↓
Host updates player state in memory
        ↓
Changed users become dirty
        ↓
3,600 ticks pass
        ↓
Collect dirty users
        ↓
ONE batch save request
        ↓
MiMITA VPS
        ↓
Database transaction
```

Example batch conceptually:

```json
{
  "players": [
    {
      "user_id": 1357,
      "gold": 883035,
      "xp": 458224,
      "kills": 8431,
      "deaths": 9385,
      "playtime_ticks": 39298421
    },
    {
      "user_id": 420,
      "gold": 15250,
      "xp": 99400,
      "kills": 812,
      "deaths": 901,
      "playtime_ticks": 5293310
    }
  ]
}
```

If 32 players changed, one save operation can contain the changed state for all 32 players.

More players should primarily increase the amount of data inside the batch rather than creating one separate database request per gameplay event.

---

# 18. No-Change Autosave

If there is no persistent data requiring synchronization:

```text
do not send an unnecessary database write
```

The system should detect that there is nothing meaningful to persist and skip the write.

---

# 19. Autosave Notification

Players should receive visible information about autosaves.

When the host begins an autosave, display something equivalent to:

```text
Host attempted autosave
```

This means:

```text
Host collected changed player data
and attempted to send it to the MiMITA database.
```

This notification does **not** mean the database confirmed the save.

---

# 20. Confirmed Autosave Notification

Only after the MiMITA backend/database confirms successful persistence should the game display:

```text
Confirmed autosave at MM-DD-YYYY, HH-MM-SS for Jorj1357
```

Example:

```text
Confirmed autosave at 09-06-2026, 10-25-31 for Jorj1357
```

The distinction is important:

```text
ATTEMPTED
≠
CONFIRMED
```

Never display a confirmed-save message merely because the request was sent.

A confirmed-save message means the backend has acknowledged successful persistence.

---

# 21. Additional Save Triggers

The 60-second autosave is not the only save opportunity.

The host should also attempt persistence when appropriate during:

```text
match end
player disconnect
clean host/server shutdown
```

These reduce the amount of progression that can be lost between normal autosaves.

They should use the same persistence pipeline rather than implementing separate save systems.

Conceptually:

```text
saveDirtyPlayerData(reason)
```

where reason might be:

```text
AUTOSAVE
MATCH_END
PLAYER_DISCONNECT
SERVER_SHUTDOWN
```

---

# 22. Duplicate Protection

Permanent rewards must be idempotent.

Network retries, duplicated packets, reconnects, backend retries, or repeated save requests must never accidentally award the same reward twice.

Every persistent reward/event should have a stable unique identity.

Conceptually:

```text
reward_event_id
```

Example:

```text
server-session-id + authoritative-death-event-id
```

If the same reward is received twice:

```text
first request:
apply reward

duplicate request:
already applied
do not apply again
```

This is especially important for gold.

The system must be designed so unreliable networking cannot duplicate permanent currency.

---

# 23. Autosave Retry Safety

Suppose:

```text
Host sends save
        ↓
Database successfully saves it
        ↓
Response packet is lost
        ↓
Host thinks save may have failed
        ↓
Host retries
```

The retry must **not** double:

```text
kills
gold
XP
deaths
playtime
```

Saving and retry behavior must therefore be idempotent.

---

# 24. Failed Save Behavior

If an autosave fails:

1. Do not mark the affected state as safely persisted.
2. Do not display a confirmed-save notification.
3. Keep the relevant state dirty.
4. Allow the persistence system to retry later.
5. Log enough information to diagnose the failure.

Example:

```text
Autosave attempted
↓
request failed
↓
dirty state remains
↓
next save can retry
```

The client should never be lied to about successful persistence.

---

# 25. Concurrent Save Safety

Do not lose progression that happens while an autosave is in flight.

Example:

```text
Player has 100 gold
        ↓
Host starts saving 100
        ↓
Before confirmation:
player earns +50
        ↓
Current state = 150
        ↓
old save confirms
```

The confirmation for the 100-gold snapshot must **not** incorrectly mark the newer 150-gold state as fully persisted.

Use a revision, generation, sequence number, snapshot version, or equivalent mechanism.

Conceptually:

```text
state revision 10:
100 gold
save begins

state revision 11:
150 gold

revision 10 save confirms

revision 11 remains dirty
```

This prevents silent progression loss.

---

# 26. Playtime

Playtime should be stored in ticks.

```text
playtime_ticks
```

At the current simulation rate:

```text
60 ticks = 1 second
```

The website can convert this into human-readable time.

Example:

```text
21d 4h 18m
```

The underlying persistent representation remains tick based.

Playtime should represent authenticated gameplay time according to MiMITA's server-side definition, not client-reported time.

---

# 27. Website Player Profile

Every public MiMITA player profile should display the persistent statistics.

Example:

# Jorj1357

```text
882,935 gold
458,024 XP
8,429 kills
9,385 deaths
21d 4h playtime
```

Players can view:

```text
their own profile
other players' profiles
```

The website reads these values from the MiMITA backend/database.

The website should not depend on a game server being online to display already-persisted profile information.

---

# 28. Public Leaderboards

MiMITA should have public leaderboards for:

```text
Most XP
Most Gold
Most Playtime
Most Kills
Most Deaths
```

The **Most Deaths** leaderboard is intentional.

Do not replace it with K/D unless explicitly requested later.

Example:

```text
MOST KILLS

1. PlayerA       84,291
2. Jorj1357      72,381
3. PlayerC       69,120
```

The leaderboard should use persisted database values.

---

# 29. NPC Statistics Separation

NPC-related progression must remain distinguishable from PvP progression.

For example, the architecture must not require:

```text
PvP kills += NPC kills
```

NPC rewards currently behave as:

```text
NPC kill:
+0 PvP kills
+0 gold
+10 XP
```

If additional NPC-specific statistics are stored internally later, they should remain separate from the public PvP kill total.

---

# 30. Reward Event Model

Prefer a shared authoritative event path instead of several unrelated systems modifying progression independently.

Conceptually:

```text
Authoritative gameplay event
            ↓
     Progression system
            ↓
   Server player state
       ↙          ↘
Popup event      Dirty state
                    ↓
                Persistence
                    ↓
                 Database
```

For example:

```text
PlayerKilledEvent
NPCkilledEvent
PlayerDeathEvent
```

can drive progression without weapon code directly knowing how database persistence works.

Weapon code should not need to perform database writes.

---

# 31. Separation of Responsibilities

Keep these concepts separate:

```text
Gameplay
Progression
UI
Persistence
Database
Website
```

Example:

```text
Rocket Launcher
    ↓
causes authoritative death

Death system
    ↓
creates death/kill event

Progression system
    ↓
awards stats

Popup system
    ↓
shows result

Persistence system
    ↓
batches dirty data

Backend/database
    ↓
stores permanent state

Website
    ↓
displays permanent state
```

Do not tightly couple all of these into weapon-specific code.

---

# 32. Performance Requirements

The progression system must remain lightweight.

Normal gameplay must **not** cause database traffic every tick.

A kill should primarily result in cheap in-memory operations:

```text
increment value
mark state dirty
generate popup/event
```

Database synchronization happens separately.

The intended scaling model is:

```text
more gameplay events
≈ more cheap memory updates

more active players
≈ larger periodic batches

NOT

more gameplay events
= enormous number of database requests
```

---

# 33. Client Trust Boundary

The client may request actions and display information.

The client must not be trusted to say:

```text
give me 1,000,000 gold
```

or:

```text
I killed 500 players
```

and have those values directly persisted.

Permanent progression originates from server-confirmed gameplay.

---

# 34. Initial Constants

Initial desired behavior:

```text
TICK_RATE = 60

AUTOSAVE_INTERVAL_TICKS = 3600

PVP_KILL_REWARD:
kills = +1
gold = +50
xp = +100

PVP_DEATH:
deaths = +1

SELF_KILL:
kills = +0
gold = +0
xp = +0
deaths = +1

ENVIRONMENT_DEATH:
kills = +0
gold = +0
xp = +0
deaths = +1

NPC_KILL:
pvp_kills = +0
gold = +0
xp = +10
```

Reward values are gameplay rules.

Popup appearance is separately hot reloadable.

---

# 35. Acceptance Test — PvP Kill

Starting state:

```text
Player A:
gold = 100
xp = 200
kills = 3
deaths = 4

Player B:
deaths = 8
```

Player A kills Player B.

Expected server state:

```text
Player A:
gold = 150
xp = 300
kills = 4
deaths = 4

Player B:
deaths = 9
```

Player A sees:

```text
+1 kill on PlayerB
+50 gold
+100 XP
```

Player B sees:

```text
+1 death from PlayerA
```

Both changed persistent states become dirty.

---

# 36. Acceptance Test — NPC Kill

Player kills an NPC.

Expected:

```text
PvP kills unchanged
gold unchanged
XP += 10
```

Popup:

```text
+0 gold
+10 XP
```

---

# 37. Acceptance Test — Self Kill

Player kills themselves.

Expected:

```text
kills unchanged
gold unchanged
XP unchanged
deaths += 1
```

No kill reward may occur.

---

# 38. Acceptance Test — Environmental Death

Player dies to the environment.

Expected:

```text
deaths += 1
```

Popup:

```text
+1 death from environment
```

Nobody receives a kill, gold, or XP.

---

# 39. Acceptance Test — 60-Second Autosave

1. Player joins.
2. Database state is loaded.
3. Player earns rewards.
4. Server marks player state dirty.
5. 3,600 simulation ticks pass.
6. Host creates one batch containing changed players.
7. Host displays attempted-autosave state.
8. Host sends batch to MiMITA backend.
9. Backend successfully persists it.
10. Backend confirms success.
11. Host marks the matching saved revisions clean.
12. Client displays:

```text
Confirmed autosave at MM-DD-YYYY, HH-MM-SS for Username
```

---

# 40. Acceptance Test — No Changes

1. Player state has not changed in a way requiring persistence.
2. Autosave interval occurs.
3. Persistence system checks dirty state.

Expected:

```text
No unnecessary database write.
```

---

# 41. Acceptance Test — Lost Confirmation

1. Host sends save.
2. Database successfully commits it.
3. Confirmation is lost.
4. Host retries.

Expected:

```text
No duplicate XP.
No duplicate gold.
No duplicate kills.
No duplicate deaths.
No duplicate playtime.
```

---

# 42. Acceptance Test — Change During Save

1. Player has 100 gold.
2. Revision 10 begins saving.
3. Player earns 50 gold.
4. Current state becomes 150 gold / revision 11.
5. Revision 10 confirms.

Expected:

```text
revision 10 = persisted
revision 11 = still dirty
current gold = 150
```

The next persistence operation eventually stores 150.

The 50 newly earned gold must never disappear.

---

# 43. Acceptance Test — Server Restart

After a confirmed save:

1. Close the game server.
2. Start a new server.
3. Same player authenticates.
4. Load player data.

Expected:

```text
gold preserved
XP preserved
kills preserved
deaths preserved
playtime preserved
```

The values must come from persistent storage rather than an old game-process memory state.

---

# 44. Acceptance Test — Website

After confirmed persistence:

1. Open the player's MiMITA website profile.
2. Read the profile statistics.

Expected:

```text
Website values match confirmed database values.
```

Open another player's profile.

Expected:

```text
Their public values are visible.
```

---

# 45. Acceptance Test — Leaderboards

Create multiple accounts with different persisted totals.

Expected leaderboard ordering:

```text
XP          → highest XP first
Gold        → highest gold first
Playtime    → highest playtime first
Kills       → highest kills first
Deaths      → highest deaths first
```

NPC kills must not inflate the PvP kill leaderboard.

---

# 46. Regression Requirements

Once these behaviors work, they become regressions that future changes must not silently break.

At minimum preserve:

```text
PvP kill gives exactly +1 kill
PvP kill gives exactly +50 gold
PvP kill gives exactly +100 XP

PvP death gives exactly +1 death

NPC kill gives no PvP kill
NPC kill gives 0 gold
NPC kill gives +10 XP

Self kill gives no kill reward
Self kill gives +1 death

Environmental death gives +1 death
Environment receives no progression

Duplicate reward event cannot award twice

Failed save does not clear dirty state

Old save confirmation cannot clear newer dirty state

No-change autosave does not create unnecessary write

Website values reflect persisted database state

Leaderboards use persisted values
```

If future code changes one of these intentionally, update this specification intentionally.

Do not silently change the behavior.

---

# 47. Debugging Requirements

The persistence system should make failures understandable.

Useful debug information includes:

```text
user ID
username
save reason
state revision
batch ID
reward event ID
dirty/clean state
attempt timestamp
confirmation timestamp
success/failure
backend error
retry status
```

Do not expose sensitive authentication credentials or secrets in logs.

The goal is to make questions such as:

```text
Why didn't this player's 50 gold save?
```

answerable without guessing.

---

# 48. Explicitly Out of Scope for V1

Do not expand this implementation into unrelated systems yet.

Not required for this first version:

```text
shops
cosmetics
items
inventory
levels
prestige
battle passes
trading
player-to-player gold transfers
marketplaces
quests
complex achievements
K/D leaderboard
NPC kill leaderboard
complex economy balancing
```

Those systems can use this foundation later.

The goal is first to make this path work extremely reliably:

```text
ACCOUNT
   ↓
JOIN
   ↓
PLAY
   ↓
KILL / DIE
   ↓
REWARD
   ↓
POPUP
   ↓
SERVER STATE
   ↓
DIRTY
   ↓
BATCH SAVE
   ↓
DATABASE
   ↓
PROFILE
   ↓
LEADERBOARD
```

---

# 49. Definition of Done

This system is done when a real player can:

1. Create or sign into a MiMITA account.
2. Join a real multiplayer game.
3. Kill another real player.
4. Immediately see:

```text
+1 kill on Username
+50 gold
+100 XP
```

5. Die and see:

```text
+1 death from Username
```

6. Kill an NPC and see:

```text
+0 gold
+10 XP
```

without increasing PvP kills.

7. Continue playing while progression accumulates in server memory.
8. See the host attempt its periodic autosave.
9. Receive a confirmed-save message only after successful persistence.
10. Close MiMITA.
11. Return later.
12. Still have the same saved progression.
13. Open `mimita.fun`.
14. See the same persistent statistics on their profile.
15. Open another player's profile and see their statistics.
16. Open the public leaderboards and see rankings for:

```text
XP
Gold
Playtime
Kills
Deaths
```

At that point, MiMITA has a complete first persistent progression loop.

---

# 50. Future Expansion Principle

New progression systems should reuse this foundation rather than create parallel persistence architectures.

Later, MiMITA may add:

```text
wins
achievements
quests
cosmetics
inventory
levels
rank
moderation reputation
creator rewards
game-specific statistics
```

The desired pattern remains:

```text
authoritative event
        ↓
server-side state change
        ↓
player feedback
        ↓
dirty-state tracking
        ↓
batched persistence
        ↓
database
        ↓
website / other consumers
```

Build the pipe once.

Then send more kinds of data through the same pipe.

---

# 51. Community Host Trust Policy — approved 09-06-2026

Every authenticated community host may award permanent progression. MiMITA
explicitly accepts that an operator who controls the host executable can invent
results. Host authentication establishes accountability, not tamper-proof
simulation. There is no approved-host allowlist in V1.

An ordinary joining client must not choose another account ID and receive that
account's progression. The backend validates the host login and binds each
remote account to a backend-issued join ticket for the same room. The host's own
account is established by its login. Guests remain playable but have no durable
account progression. Never ship a shared database or master host secret in the
game executable.

# 52. Canonical Persistence Contract

Reuse the existing game_stats table and preserve its data. Public field mapping:

| Public statistic | Database field | Host/API field |
|---|---|---|
| Gold | gold | gold |
| XP | total_xp | totalXp |
| PvP kills | lifetime_player_kills | playerKills |
| Deaths | lifetime_deaths | deaths |
| Playtime | playtime_ticks | playtimeTicks |
| Internal NPC kills | lifetime_npc_kills | npcKills |

The legacy kills/deaths fields are match-era counters and must not be added to
lifetime counters: that could count the same event twice. Preserve legacy
values for compatibility. Convert existing playtime_seconds to ticks exactly
once during migration; ticks are canonical afterward. Persistent counters use
nonnegative signed 64-bit integers. Responses serialize integers as decimal
strings so JavaScript cannot round large totals. Unsafe, fractional, negative,
or out-of-range inputs must be rejected.

game_stats has one row per account. Existing accounts receive missing zero rows
without resetting existing values; new account creation also creates a row.
Host session ownership lives in progression_sessions. Per-session membership,
last accepted revision, and cumulative accepted gains live in progression_players.
Reuse processed_events to retain batch identity and its committed response;
do not add a parallel reward_events table merely to match a conceptual name.

# 53. Host Session and Save API

POST /api/progression/session uses the authenticated host's bearer session.
Input is sessionId, roomCode, and players containing userId and joinTicket for
initial remote registration. The host generates a stable unique sessionId for
the server lifetime. Response includes sessionId and players with username,
revision, global totals, and cumulative session gains. Repeating registration
must not reset accepted state. Existing membership can resume without reusing
an expired admission ticket. One ticket cannot admit a player into unrelated
progression sessions.

POST /api/progression/batch uses the same authenticated host. Input contains
sessionId, batchId, reason, and players. Each player supplies userId, revision,
and the six cumulative SESSION counters above. These are gains since that host
session began; they are not absolute account totals. This representation lets
the host keep loaded totals plus current gains while avoiding overwriting
progression confirmed by another host.

The backend compares each submitted cumulative counter with that session's
last accepted counter and atomically adds only the difference to global totals.
It updates the accepted revision and records the immutable batch result in the
same transaction. The entire batch commits or rolls back. A success response
contains sessionId, batchId, confirmedAt, and accepted player revisions/global
totals. A changed request reusing a batchId is a conflict, not another award.
A stale revision or decreased cumulative value must never overwrite newer data.

Retries resend the same immutable request and identifiers. An old confirmation
advances only the corresponding persisted revision; newly earned gains remain
dirty. Session ownership and membership checks apply on every write. Legacy
player-authenticated stat-write endpoints must not bypass this contract.

# 54. Lifecycle and Failure Rules

State flows through loading, clean, dirty, saving, confirmed or retry-needed.
Dirty state belongs in host memory; the database stores accepted revisions.
Network calls run on a worker rather than the fixed simulation thread. Only
one batch per host is in flight. New gameplay updates continue while it saves.
An empty dirty set creates no persistence request. 3,600 actual server ticks
trigger normal autosave; match end, disconnect, and clean shutdown request the
same path. Retry failures must never discard payloads or silently erase older
unsaved progression to make queue space.

Playtime counts connected, authenticated gameplay participants once per fixed
server tick, including their normal death/respawn time. Disconnected players,
offline play, and menu time do not accrue persistent playtime. A new server
session reloads totals from storage. A process crash may lose unconfirmed
in-memory gains; confirmed progression must survive. Shutdown waits for a
bounded final attempt and reports unconfirmed state honestly on failure.

Attempt messages mean only that a request began. Confirmed messages require the
backend's committed response and identify the player, revision, and time. Local
popup timing and presentation remain governed by sections 8–12 and the existing
hot-reloadable reward HUD config.

# 55. Schema, Website, and Operational Acceptance

Schema changes must be tracked, versioned, transactional, and recorded in a
migration ledger. Adopt the existing tracked db.js bootstrap as the historical
baseline instead of describing its tables as untracked. Migration errors stop
startup/deployment with failure; they cannot be swallowed as successful setup.
Reapplying completed migrations must preserve data. Test fresh setup and
adoption of an existing schema, including an account with nonzero totals.

Both own and public profiles display the five canonical public values. The five
leaderboards order by the corresponding persisted value descending and account
ID ascending for ties. NPC counts never enter the PvP ranking. Loading, failed,
and genuinely zero values remain distinct; a failed request must not show
fabricated zero progression. Large integer formatting remains exact.

In addition to sections 35–45, test simultaneous hosts adding progression to one
account, a mismatched ticket/room/account, a wrong host session owner, a reused
batch ID with changed contents, a rollback halfway through a batch, and safe
rejection of integer overflow. Verify migrations preserve existing users and
stats and create missing rows for new users.

Use docs/operations/vps-audit.md to compare deployed schema and aggregate values
with the repository. Use docs/operations/persistence-recovery.md for backup and
restore checks. Archive listing and nonzero size alone do not prove recovery:
an isolated restore must succeed before declaring the backup recoverable.
Production deployment still requires the reviewed branch/commit and the
existing VPS deployment procedure. Source tests are not proof of the live
two-client acceptance sequence in section 49.
