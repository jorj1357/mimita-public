# NPC behavior hot policy

## Changed

- Extended the `npc-behavior` module decision contract with pursuit-retention and down-dash policy outputs.
- A stuck NPC with a current or remembered target now asks the hot module whether to keep pursuing before entering the legacy random-walk fallback.
- Down-dash opportunity and configured chance are supplied to the module each fixed NPC update. The host still validates and executes the movement ability.
- Optional long-run dash selection now uses the same hot behavior module. Required gap traversal stays with the host navigator.
- Increased `fun_cs_rush` optional dash chance to 0.35, easy rush to 0.12, and global down-dash chance to 0.25 so the abilities are actually used when movement conditions allow.

## Build and runtime evidence

- Per request, no build is requested and no gameplay run or `events.jsonl` inspection is performed.
