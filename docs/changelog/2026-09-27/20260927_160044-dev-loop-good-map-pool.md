# Dev loop uses the good-map pool

Changed the NPC development profile and loop so map startup uses
`config/gamemode-good-maps.json` as its allowed map source.

The profile now selects a random map from the JSON `maps` array for each
automatic launch. The selected map is passed identically to the dedicated
server and room-code client. If the pool file cannot be read or is empty, the
loop fails clearly instead of silently launching an arbitrary map.

The pool file is watched, so changing its allowed list marks the development
state for a fresh build/launch cycle.

Validation:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python devscripts/dev-loop.py --help`: passed.
- Loaded 11 maps from `config/gamemode-good-maps.json` and selected a map from
  that list successfully.
