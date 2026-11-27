# Survive the Disasters, RPG, and Social Creator Vision

**Date:** 2026-11-26
**Audience:** gameplay, map/editor, NPC, combat, effects, audio/networking, replay, persistence, UI, and moderation agents
**Status:** cross-feature implementation handoff; requested directions below are proposals until reconciled into their owning specifications
**Purpose:** turn the user's combined mode and creator vision into a practical sequence of independently provable slices.

This document does not replace the authoritative specs listed in each section. Before implementation, read the relevant specs and update them through the normal issue and clarification process. Do not silently treat this broad handoff as permission to change unrelated platform architecture, persistent account data, or moderation policy.

## Player and creator vision

MiMITA should make it quick for people to join a friendly shared world, chat, survive an unusual disaster together, and keep playing with friends. Disasters combine authored ingredients into memorable situations: for example, enraged, fast, fire-themed Jason-like monsters chase players through a destructible map while players use toys, spell weapons, bikes, and other creator-made tools. Players earn permanent, non-spendable survival points and can separately earn mode rewards such as gold for RPG shops.

Creators should be able to build maps, monsters, weapons, attacks, animation, and effects in the game, test them live, save them simply, and contribute reusable components to the shared game ecosystem. Start with strong reusable primitives and a small polished slice; expand the content library over time instead of requiring hundreds of unfinished assets before anything is playable.

## Existing authoritative sources and open work

- [Survive the Disasters](../specs/gamemodes/survive-the-disasters.md) currently describes 15-second between-round intermissions, selectable disaster durations, ten-round matches, and local survival points. The requested direction changes this to a continuously cycling social server with 15-second intermissions and 30–60-second active disasters, with an ever-increasing disaster index and points that persist and are never spent. Reconcile the lifecycle and persistence rules in that spec before implementation.
- [RPG](../specs/gamemodes/rpg.md), [Monster and Game Content Authoring](../specs/creators/monster-authoring.md), [Map Entity Editor](../specs/entity-editor/entity-editor.md), [animations](../specs/animations/animations.md), and [NPC behavior](../specs/npc-behavior/npc-behavior.md) own the RPG authoring path. Existing capability records include `issue-5251f107d7c3` (bosses/arenas/animation authoring), `issue-29115dfd03db` (economy/trading/persistent inventory), and `issue-35736d408bd4` (long-horizon progression).
- [Destructible World](../specs/destructible-world/destructible-world.md) owns structure failure and map restoration. Its current global default is disabled. The requested block-and-broken-link destruction is a separate future model to define and measure; do not silently switch the global default or implement per-disaster destruction rules in the gamemode.
- [Voice Chat](../specs/ingame-chat/voice-chat.md) owns player voice controls and moderation requirements. It already calls for push-to-talk, input selection where supported, speaking indicators, per-player mute, room disable, and text availability. [Moderation](../specs/moderation/moderation.md) owns abuse reporting and enforcement.
- [Replay](../specs/replay/replay.md) owns recent capture, audio preservation, playback, and export. Replay export repair is already tracked by `issue-612ce59ed73d`. Voice capture must be designed with explicit participant notice, user control, consent, and retention boundaries; the existing voice spec says conversations are not automatically recorded or published.
- [Monster and Game Content Authoring](../specs/creators/monster-authoring.md) is the common creator project path. [Distribution](../specs/distribution/distribution.md) owns safe packaging and sharing. Reusable community content must remain data/assets, not downloaded native code or unsandboxed scripts.

The router is the index for other relevant specs. Follow it for networking, fixed-tick combat, GUI, maps/world projects, persistence, audio/effects, moderation, and distribution before touching those areas.

## Requested behavior by area

### 1. Social Survive the Disasters

- Keep the mode relaxed and social. Cycle forever between a 15-second intermission and a 30–60-second active disaster. Announce the disaster before it starts and show the outcome before the next intermission.
- Number disasters monotonically for the life of a server; do not cap the display at a small fixed round count. Define overflow/storage representation before implementation.
- Keep survival points persistent and non-spendable. They are separate from gold, XP, account-level kills, and any competitive ranking. Define what persistence means (local profile or server/account-backed) in the owning data spec before promising cross-device durability.
- Let players spend a separate, explicitly defined mode currency such as gold on useful/fun RPG equipment or permitted toys. Do not let disaster points be spent.
- Make the shared server support the intended social scale (vision: 16 players), with text chat and visible text bubbles where the GUI/chat specs permit. Friends, presence, messaging, and rejoining friends are platform capabilities and should be planned as separate increments, not hidden prerequisites for the disaster loop.
- Disaster content is composed from compatible ingredients. The user's starter lists include objects (duck, masked slasher, slender creature, zombie, skateboard, basketball, snowball, volcano, asteroid, planets), adjectives (giant, small, aggressive, fire, ice, poisonous, venomous, stinky, enraged, sick), and actions (throwing, punching, stabbing, killing, running, launching). Grow these toward roughly 100 entries per category over time, but do not hardcode a Cartesian product. Use tags, capability checks, weights, and explicit incompatibility rules so combinations are valid, budgeted, understandable, and safe to stop/clean up.
- Example target: a disaster such as “flaming enraged slasher” can spawn fast hostile NPCs with a fire presentation and knife attack, while players use permitted tools and the map responds under the selected destruction model. The generated name alone is not proof: each ingredient must correspond to actual behavior and cleanup.
- Player tools should be fun but not damage other players in this mode by default. A shuriken or ray spell may damage hostile NPCs or disaster objects through the shared authoritative combat path. Define per-mode permissions rather than adding a parallel damage system.
- Drinks, food, toys, dances, sound-making items, bikes, skateboards, and later vehicles are content families. Stage them after the basic disaster loop and moderation controls; vehicles requiring swimming, aircraft, boats, or broad physics work are later expansions.

### 2. RPG combat, world, and editor

- Treat RPG as a creator workflow as well as a playable mode: create/open a large map, edit it live, place player starts and monster spawn areas, author monsters and equipment, save, and immediately Play Test in the same application.
- Author a monster's model/rig, animation, movement, attack timing and hit volume, sounds, attached weapon, and rewards as validated project data. Reuse the existing monster, animation, entity, NPC, map, and combat owners.
- Expand combat beyond simple sword overlap: varied melee moves with readable startup/active/recovery windows, blocking or defensive options, and ranged magic. Keep damage/collision on the fixed 60 Hz authoritative path. Build procedural animation as a staged capability; first define what is procedural (locomotion adaptation, weapon aim, hit reactions, or attack generation) and preserve authorable clips as fallback. Do not promise “no fixed animations” until a concrete method and acceptance target exist.
- Use a fantasy presentation for weapon roles: explosive spell wand for rocket-like attacks, bullet/bolt spell wand for revolver-like attacks, plus mace and sword. Starting equipment should make ranged and melee play possible. Example weapon identities: iron sword (basic), green grass sword, light-blue ice sword with icy light, and void sword with black fire. These are visual/gameplay targets, not permission to bypass shared weapon configuration or balance.
- **Clarified RPG start:** load directly into play at level 1 with 100 gold and no mandatory tutorial or setup quest. Equip a straightforward sword, a bullet staff, and an explosive staff. The bullet staff fires an aimed glowing orb projectile; the explosive staff fires a magical rocket-like projectile. Whether either weapon uses ammunition remains open. The sword should read clearly as a sword and show satisfying, visible slashes. This direction conflicts with the current RPG spec's revolver/shotgun/sword starter kit; reconcile that spec before implementation.
- **Combat presentation:** layer small reusable visuals at projectile launch, travel, impact, and explosion so both staff attacks feel magical and readable. Use the shared effect/particle system with budgets; do not implement one-off per-weapon visual code.
- **Long-horizon loop:** defeat monsters, gain gold and rewards, buy or find stronger equipment, and challenge harder monsters. Support uncapped progression and very large level/value display (the user cites 100, 999, 1,000, 10,000, 100,000 and beyond). Treat those as a scale requirement, not a promised leveling speed. Gold remains spendable; survival points remain a separate non-spendable Survive currency.
- **Procedural collectible combinations:** bosses and sufficiently difficult monsters may award earned rolls or crafting components for cosmetics and weapon variants. Build appearances from simple primitives and modifier data—for example, three orbiting spheres with independent color, size, rotation, and fire parameters. Ordinary rolls can produce awkward combinations; rare alignments can produce especially cohesive looks. Higher-difficulty enemies may improve the odds, but exact rates and whether every monster drops an alignment component need balance decisions. Keep odds and provenance inspectable.
- **Rare-item identity and trading:** the user wants rare combinations to be tradeable and to retain a durable identity/provenance record. A content hash can identify an item record and detect altered bytes; by itself it cannot guarantee ownership, prevent double-spending, or ensure indefinite availability. Use the storage/trading design in `docs/specs/world-projects/world-projects.md` and the existing economy issue. Do not create a cryptocurrency or promise Bitcoin-like trustlessness; those require separate protocol, security, abuse, and platform-value decisions.
- **Cross-mode rewards:** support a future compatibility layer for earned items or currencies to appear in modes that explicitly allow them. Keep world gold/XP separate from any account-wide transferable currency. Mode opt-in, authority, loss/recovery, and competitive fairness rules must be defined before cross-mode transfer is implemented.
- Give monsters gold/rewards through the existing progression owner. Shops should be usable in-game: interact with the keeper, open a GUI and unlock the mouse, show a clear red close button, and provide a panic escape key that safely closes the interaction. Follow the GUI and input-focus specifications.
- Add reusable primitive effects (fire, ice, smoke, acid) and particle systems that can use creator-supplied images/textures, colors, lifetime, motion, and budget limits. A sword's effects should be data-driven and reusable rather than separate hardcoded implementations.
- World editor direction: fast, friendly map creation; large spaces; live placement and save; monster spawn volumes with a bounded roster; clear paths/project organization; later support for blocks and broken structural links. Keep editor work out of gameplay hot loops and preserve validated project/save behavior.

### 3. Voice chat and replay audio

- Support push-to-talk and voice activity as selectable modes, a selectable input device from the available microphone list, a live input/speaking indicator, console configuration, text chat, and per-player mute. Provide room-level disable and moderation/report controls according to the voice and moderation specs.
- First determine supported transport/topology and platform device APIs. The request says client-to-client, but the implementation must use the existing networking/security architecture and define room membership, routing, abuse controls, and failure behavior before choosing peer-to-peer transport.
- Repair replay export independently under `issue-612ce59ed73d`; prove recording, audio sync, playback, and export against one pinned run. Then decide whether the recent-clip ring buffer includes the last 15 seconds of voice the local player heard. This is new recording behavior and requires an explicit, visible capture state and participant notice/consent rules before implementation. Do not silently record voice.

### 4. Creator ecosystem and later platform work

- Encourage feedback and community-made tools/items. Start with validated in-game creator projects and safe data/assets that can be tested locally; then portable sharing and moderation; only later remote catalogs and cross-project composition.
- Friends, online presence, direct messages, friend requests, and joining a friend's activity need privacy, block/mute, reporting, and account/session ownership decisions.
- “Decentralized” durable storage, platform-wide currency, password/account persistence, and eliminating a single database point of failure are architecture/security projects. They are not solved by a gameplay handoff. Do not store passwords in game content or invent a competing identity store; route this separately through the authentication, persistence, security, and operations owners.
- The existing distributed-data recommendation already separates project blobs, signed host reward/edit logs, and encrypted player-owned backups; it allows peer/community copies for public content while retaining optional central services as mirrors rather than the only copy. Reuse and investigate that design. Replication improves availability but does not make a dishonest host honest or a mutable balance trustless.
- Shared movement fundamentals include a configurable step-up/grounding behavior for slopes: some authored surfaces should be treated as walkable ramps, and actors should remain stable on slopes where the mode expects it. Route this through movement/collision ownership and define intended surface tags and measurable acceptance before changing physics.
- Audio/song creation, collaborative music production, broad vehicle families, portals, non-Euclidean maps, and 4D gameplay are future capability directions. Keep them discoverable in creator architecture, but do not block the first playable slices on them.

## Recommended delivery order

Each slice has one owner, one spec/issue, a narrow acceptance target, and its own evidence. Agents should claim existing issues rather than make duplicate records.

1. **Reconcile specs and boundaries.** Update or clarify the Survive lifecycle/points, RPG-first-playable equipment/combat, voice recording consent, and replay-export issue. Decide which currency is spendable, where permanent points persist, and the disaster-index lifetime. No gameplay code until these conflicts are recorded in the owning specs/issues.
2. **Prove the social round loop.** One map, a small built-in disaster roster, 15-second intermission, 30–60-second disaster, survivor result, monotonic server index, and persistent non-spendable points. Exercise repeated cycles and reconnect/persistence behavior; verify exact run journals.
3. **Make one creator map/monster loop usable.** In-game map create/edit/save/reopen/Play Test, one spawn volume, one monster with authored movement and an attack volume, and one observable reward. Use existing authoring paths and prove edits apply to the project being tested.
4. **Add one complete combat/effects set.** Start with the clarified level-1/100-gold loadout: one visible sword, one bullet staff, one explosive staff, one hostile NPC target, and layered projectile/impact effects. Prove startup/hit/recovery, damage, cooldown, and cleanup. Add further variants only after the shared data path works.
5. **Make one safe shop interaction.** Keeper interaction, mouse release, close X, panic escape, one purchase with explicit currency, and no accidental player damage. Verify focus is restored after every close path.
6. **Deliver voice controls and moderation.** Device list, selected mic, push-to-talk, voice activity option, input indicator, per-player mute, room disable, and report path. Validate with multiple clients and human listening review before acceptance.
7. **Restore replay export, then extend voice capture only after consent design.** First export gameplay and existing audio successfully. Separately verify any voice ring buffer's visible consent state, participant policy, retention, and playback behavior.
8. **Expand combinations and creator sharing.** Add ingredient data, compatibility/budget limits, author tooling, feedback, packaging, moderation, and community distribution in separate increments.
9. **Plan platform-scale capabilities separately.** Friends/presence/messaging, distributed ownership and trading, cross-mode currency, collaborative audio creation, broad vehicles, and non-Euclidean/4D work each need their own architecture decisions, owners, issues, and measurable slices.

## Agent implementation contract

- Before code edits, read the router-selected specs and exact owning source. Trace input through config/project loading, authority, simulation, presentation, and journal events.
- Search for existing owners before adding state, packets, handlers, combat formulas, item registries, or editors. Prefer data-driven reuse; do not build a second parallel RPG, damage, effects, or chat system.
- Work in the shared checkout and current branch. Coordinate disjoint file ownership. The root dev-loop alone builds and runs; follow the current agent coordination and build authority rules.
- For each user-visible behavior, state a falsifiable hypothesis, record before/after evidence where relevant, and prove the exact feature in the pinned runtime's `events.jsonl`. A build or synthetic test alone does not prove gameplay. Voice, multiplayer, GUI, visual effects, and replay need their appropriate real-client/human acceptance.
- Label unknowns explicitly. Do not claim persistent cross-session data, moderation, voice privacy, live editing, multiplayer, or export works without direct evidence.
- Create or update a complete issue for newly actionable work, preserve source user wording through the coordination context process, and leave unclaimed follow-ups unowned. Do not resolve ambiguity by silently editing the authoritative spec.

## Open design questions for the owning issues

1. Do survival points persist on the device, account, or server profile, and what does “stick” mean across reinstall/device changes?
2. Does the disaster index reset on a server restart, and what representation/rollover policy is required?
3. Is the 15-second gap measured after the previous disaster ends, or does survivor/result presentation occur within it?
4. Which small initial disaster roster is fun, physically possible, and safe to clean up? What budget limits cap NPC count, particles, and destruction?
5. Which monster attacks and player tools are permitted against hostile NPCs, hazards, and players in each mode?
6. Which pieces of animation should be procedural first, and what visible result would prove improvement over authored clips?
7. What is the spendable RPG currency and what progression store owns it? Is it separate from non-spendable survival points?
8. What voice transport is supported by the current network architecture, and what is the initial room/proximity policy?
9. What participant notice/consent rules apply to the last-15-seconds voice buffer, and is capture per client or per room?
10. Which first map and first monster make the editor useful without expanding into every requested platform capability?
11. Which RPG currencies are world scoped, which (if any) can transfer across compatible modes, and which rewards are permitted in competitive modes?
12. Are rare-item drops generated per monster, per harder enemy tier, or only from bosses? Are random rolls tradeable, and what exact ownership/duplicate prevention guarantees are required?
13. Should the RPG start-kit clarification supersede the current RPG spec's revolver/shotgun/sword loadout, or should those conventional weapons remain available alongside their fantasy staff equivalents?
14. Which world-project distributed-storage properties should be researched first: reward-log availability, encrypted profile backup recovery, or double-spend-resistant tradable ownership?

## Evidence status

This is a documentation handoff based on existing specs and recorded capability issues. No code, configuration, build, or runtime was changed or validated here. It does not claim that the requested features are implemented or accepted.
