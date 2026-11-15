2026 11 05 1643 jorj 

explain like

all inputs are bindable to difrent things

like

w = walkforward
s = walkbackward

left mouse = fire button

these are old 

so it should bel ike
expand and base it on accessbililti C:\mimita-v9\docs\architecture\accessibility\accessibility.md

this

but u can do it in the game with settings 


## Current ownership (2026-11-15)

`config/input/binds.json` is the canonical KBM binding file. It carries
`version` and `input_version`. Runtime input is normalized by
`docs/specs/input/input-router.md`; bindings map normalized tokens to actions.
Terminal `input.bind` changes the runtime; `input.save` writes the JSON. JSON
reload wins. Account-specific bind overrides are reserved under `accounts` and
are future work.
