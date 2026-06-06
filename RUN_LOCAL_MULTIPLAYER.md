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

The first multiplayer pass is UDP localhost only. The server is authoritative for
player position, velocity, health/state, world collision, and simple player
separation. Clients send input and render the latest authoritative snapshot.

i put 1357 becusae jorj1357 Heheh ehehe Hehe