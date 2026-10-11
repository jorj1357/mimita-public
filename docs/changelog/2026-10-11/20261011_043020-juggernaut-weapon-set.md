# Juggernaut weapon set expansion

Updated `config/weaponsets.json` weapon set 7 (`juggernaut`) to allow:

- Rocket Launcher
- Shotgun
- Heavy Shotgun
- Large Machine Gun
- Force Punch

Logical slots are now 1 through 5 in that order, followed by Nothing in slot
6. This is a configuration-only change; no C++ rebuild was required. `git
diff --check` passed apart from existing line-ending warnings in unrelated
working-tree files.
