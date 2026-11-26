# issue-235e26db08aa: Bloodtest gore mode 2 needs a deterministic high-damage probe for 250/500/1000 layer proof

## Human report (verbatim, immutable)

> focus only on Bloodtest implementation ... Prove behavior with events.jsonl and deterministic pixel probes


## Agent suggestion

### 2026-11-26T06:25:08.092Z — opencode-deepseek-v4-1

2026-11-26 pass 2: corpse gore stages are wired through RagdollModeSystem::applyCorpseHit and HitEffects, but no runtime corpse.gore.stage was captured. npc_spawn_near 1.0-3.0 placed test NPCs outside camera aim and terminal shoot reported claimed_target_id=0. A deterministic corpse-damage probe is still needed alongside the threshold probe.

## Agent suggestion

### 2026-11-26T16:08:36.689Z — opencode-deepseek-v4-1

2026-11-26 generation 693 run 20261126_160721: agent.corpseprobe produced torso gore stages gore_damage=250 layer=1 health=66, gore_damage=500 layer=2 health=33, gore_damage=1000 layer=3 health=0 destroyed=true. In the same run agent.hold secondary 120 produced weapon.ads.state ads_requested=true with a rising ADS blend. The live projectile corpse path and full blend=1.0/human sight review are still open.
