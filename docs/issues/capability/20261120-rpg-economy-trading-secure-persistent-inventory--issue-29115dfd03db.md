# issue-29115dfd03db: RPG economy, trading, secure persistent inventory, and cross-mode rewards

## Human report (verbatim, immutable)

> rpg shoudl have shop and stuff, need to figer out distributed datat so no saving data to central server  ai agents can make this a  issue and work on it frmo time to time liek fighintt out how to  do it beter and geting more info and more evidnece of how we coudl do it
> 
> so in the rpg u can purhcase weapons with gold or a currency
> 
> also shoudl be able to trade itesmw with toher players, and need to ahve a separate mimita currency that idk needs more protection or cetnralziation more security so that  per gammeode might have currency but soem gammemodes cant transfer currencies , but we shoul dmkae it so items cna save to a plauer inventorya and reuse the secure method of decetnralizedd claiming or owning an item, like
> 
> beating a rpg boss should award u cosmetics or awards or
> it shoudl give u gold and u can use that to get items liek cosmetics or weapons
> 
> it shoudl be stuff that u get  that can apply in a bunch of other modes like a cosmetic used everywerhe, a weapon skin used everywerhe, death effect used everwerhe, emotes used everywhere, etc antthing else we add too , making it so teh


## Triage

### 2026-11-20T07:28:04.717Z — codex_disaster_rpg_ideas

Summary: Capture RPG progression/economy/rewards proposal and investigate secure persistent ownership options against the existing account/server-authority design.
Owner: codex_disaster_rpg_ideas
Priority: high

## Clarification requested

### 2026-11-20T07:28:04.918Z — codex_disaster_rpg_ideas

Question for the user:

For the research task, should the first focus be secure cross-mode inventory/cosmetics, RPG gold and shops, player trading, or distributed saving without a central server?

## Agent suggestion

### 2026-11-26T22:52:03.113Z — codex-rpg-progression-followup

2026-11-26 follow-up clarifies an immediate level-1 start with 100 gold and no tutorial; sword plus magical bullet/explosive staffs; layered spell effects; an indefinite kill-reward-upgrade loop with very large uncapped levels; rare primitive-combination cosmetics and weapon variants with item provenance and requested trading; possible cross-mode rewards; and a need to investigate replicated player/world data. The new agent handoff records these as proposed design deltas and points to the existing World Projects distributed-storage recommendation. The older RPG spec's revolver/shotgun/sword starter kit conflicts with the new loadout. Before changing that spec, clarify whether staffs replace those starting weapons or are fantasy skins/variants, and claim a bounded subtask on issue-29115dfd03db rather than duplicating the economy/storage issue.

## Agent suggestion

### 2026-11-26T23:04:41.031Z — codex-durable-account-storage

Architecture proposal for century-scale account durability: do not put passwords, password hashes, recovery secrets, tokens, private keys, or personal data on a public ledger or content network. Separate (1) identity/authentication, (2) authoritative currency transactions, and (3) private profile/recovery data. Authentication stores only salted, slow password verifiers or public-key/passkey credentials; restrict access, rate-limit attempts, and rotate/migrate verifiers safely. Currency uses an append-only, idempotent transaction log with integer amounts, unique event IDs, signed issuer/game/world/version provenance, and quorum-committed replicated copies in independently administered regions; balances are derived snapshots. A host signature proves who submitted a reward, not that gameplay was honest, so custom-world currency stays world-scoped unless an issuance authority can validate it. Sensitive profile backups use envelope encryption; replicate encrypted backups to independent operators, keep wrapping keys separate, provide owner-controlled recovery, and test restoration. Public content blobs may use content-addressed peer storage, but hashes prove integrity, not ownership, confidentiality, availability, or double-spend prevention. For long horizons preserve portable formats, event history and migration tools, rotate cryptography with versioned envelopes, and routinely test restores. No storage design can guarantee availability across centuries without maintained operators, keys, media, formats and funding. This is a proposed research direction, not a decision to implement a blockchain; compare replicated conventional consensus first and multi-operator governance only if MiMITA must survive its own organization disappearing.
