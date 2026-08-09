# Local Multiplayer Test Mode

Run each command from this repo folder after building `mimita.exe`.

Terminal 1:

```powershell
.\mimita.exe --server
```

Terminal 2:

```powershell
.\mimita.exe --client --name client1 --connect 127.0.0.1:1357
```

Terminal 3:

```powershell
.\mimita.exe --client --name client2 --connect 127.0.0.1:1357
```

Terminal 4:

```powershell
.\mimita.exe --client --name client3 --connect 127.0.0.1:1357
```

No arguments keep the normal Mimita menu/single-player behavior.

If `--name` is omitted, the client uses `LocalProfileSystem`:

- `config/current-profile.json` when signed in
- otherwise a process-unique fallback such as `player4821`

The server makes duplicate names unique (`admin`, `admin(2)`, `admin(3)`).

Protocol v3 snapshots contain all connected players and server NPCs. The server
is authoritative for player registry, player transforms, NPC transforms, NPC
spawn/despawn, health/state, world collision, and simple player separation.
Clients send input/profile names and auto-create visual replicas for unknown
Player and NPC entity IDs.

While connected:

- `F2` requests a server-authoritative NPC spawn.
- Hold `TAB` for the server-approved player list.
- Press `F3` for replication diagnostics.

To bind a test server to another localhost port:

```powershell
.\mimita.exe --server --connect 127.0.0.1:2357
```

i put 1357 becusae jorj1357 Heheh ehehe Hehe

## badconn (per-client network simulation)

In any GUI client, open the console (`~`) and use `badconn`:

| Command | What it does |
|---|---|
| `badconn list` | Lists every preset from `config/badconnconfig.json` |
| `badconn <N>` | Activates preset N for THIS client only (e.g. `badconn 2` = high latency) |
| `badconn 0` | Disables all simulated problems and flushes queues |
| `badconn` | Shows current preset + counters |
| `badconn reload` | Re-reads `config/badconnconfig.json` |

Behavior notes:

- Presets only affect the client process where the command was typed. The server
  and other clients are untouched. If the host runs `badconn`, only the host's own
  client side is impaired.
- Presets reset on every launch — nothing is persisted.
- The `~` console and `badconn` only exist in the graphical client, not in the
  headless `--client` debug path.
- Preset 7 (`15 second blackout`) is silence on both directions. A player is now
  NOT kicked after ~10s: the server marks them stale at `stale_packet_threshold_ms`
  (~2.5s), freezes their body (invulnerable), keeps the slot alive for the
  `reconnect_grace_ms` window (60s), and notifies other clients via
  `PACKET_PLAYER_CONNECTION_STATE` so observers see the red reconnect effect.
  A 15s blackout therefore reconnects automatically when packets resume.
  Use preset 6 (`5 second blackout`) to test choppy/frozen remote movement
  without a disconnect.
- The client shows honest connection states in the F3 debug overlay and HUD
  (`connstate <status|weak|reconnect|disconnect|recover>` forces states for
  testing; `netstats` shows reconnect + packet-age info).
- Press `F3` while a preset is active to see a `BADCONN` summary label.
