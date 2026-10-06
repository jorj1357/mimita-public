# Juggernaut arcade health and presentation

- Scope: keep Juggernauts at 5,000 health and make this mode's combat presentation explicitly arcade-visible.
- Added the presentation-only juggernaut_arcade actor preset. It enables damage numbers, hit effects, world impacts, blood, muzzle flashes, hitmarkers, and hit sounds without selecting a weapon set or changing movement or health ownership.
- Updated the Juggernaut mode to enable its visible presentation flags, including healthbars and player outlines.
- Added mode-scoped green friendly outlines and red enemy outlines through the existing player-renderer outline owner.
- Source evidence: the Juggernaut role remains health 5,000 in config/roles.json; role health remains the authority for spawn/max-health setup.
- Validation: all touched JSON files parsed successfully; explicit assertions passed for 5,000 health, the arcade actor preset, and green/red outline colors; the canonical build compiled the touched C++ translation units. The existing checked-in mimita.exe could not run on this host, so connected-client visual acceptance is not claimed here.
