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
