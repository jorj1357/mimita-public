# World generation mode JSON

Per-mode generator profiles live in `config/worldgen/<mode>.json`. They own the
default grid, chunk set, spacing, and branching policy. A map's
`generation_zones` entry overrides those defaults for a specific region.

```json
{
  "format": 1,
  "mode": "tunnel",
  "grid": {
    "plane": "xy",
    "repeat_axes": [1, 1, 0],
    "origin": [0, 0, 0],
    "cell_size": [10, 10, 10],
    "chunk_size": [10, 10, 4],
    "stream_radius": [2, 2, 0],
    "spacing_mode": "connector",
    "connector_gap": 0.0
  },
  "chunk_files": [
    "config/worldgen/chunks/chunk_straight.json",
    "config/worldgen/chunks/chunk_corner.json",
    "config/worldgen/chunks/chunk_cross.json",
    "config/worldgen/chunks/chunk_deadend.json"
  ],
  "max_branch_depth": 12,
  "allow_cycles": true,
  "allow_crossings": true
}
```

## Grid fields

- `plane`: `xy` (default), `xz`, `yz`, or `all`.
- `repeat_axes`: which axes tile. `[1,1,0]` = X/Y, Z fixed.
- `origin`: grid origin.
- `cell_size`: center-to-center repeat distance per axis.
- `chunk_size`: authored content size used for centering.
- `stream_radius`: how many cells stream around the player per axis.
- `spacing_mode`: `center` (uniform grid) or `connector` (port-aligned).
- `connector_gap`: extra distance between an exit and the next entry.

## Chunk library

Chunks live in `config/worldgen/chunks/<id>.json` and are referenced by
`chunk_files`. A chunk is a container of primitive parts plus connectors.

```json
{
  "format": 1,
  "id": "chunk_straight",
  "bounds_min": [-5, -5, 0],
  "bounds_max": [5, 5, 4],
  "parts": [
    {
      "id": "floor",
      "kind": "floor",
      "position": [0, 0, -0.25],
      "size": [10, 10, 0.5],
      "material": "stone_floor",
      "faces": ["", "", "", "", "stone_floor", ""],
      "collision": true,
      "visible": true
    }
  ],
  "connectors": [
    { "id": "west_entry", "side": "west", "orientation": "tunnel",
      "role": "entry", "position": [-5, 0, 2], "tags": ["tunnel"] },
    { "id": "east_exit", "side": "east", "orientation": "tunnel",
      "role": "exit", "position": [5, 0, 2], "tags": ["tunnel"] }
  ],
  "tags": ["straight"],
  "enabled": true
}
```

## Part kinds

`box`, `wall`, `floor`, `ceiling`, `tunnel`, `doorway`, `prop`.

Face material order: `+X`, `-X`, `+Y`, `-Y`, `+Z`, `-Z`. Empty means the part
default `material`.

## Connectors

- `side`: `east`, `west`, `north`, `south`, `up`, `down`.
- `orientation`: `tunnel`, `doorway`, `open`, custom.
- `role`: `entry`, `exit`, `bidirectional`, `none`.
- `tags`: compatibility tags; connectors match when their tags intersect.

## Precedence

```text
built-in defaults < per-mode JSON < per-zone map/project override
```
