# issue-be1adb51b6ac: Fun Counter Strike NPC objective behavior regressed: CTs stuck at spawn

## Human report (verbatim, immutable)

> can u see if npc behavoor is a issue and expand it with  how the npc beahvior in fun counter strike mode is broken, it behaced good but broke, need to track regression and show that  it was good like
> 
> terrorist npcs all pick a site and rush to it instatnly, CT npcs like  split or look for either site and look for terrorists, and then once u spot a terrorist  as a CT npc, all CT npcs know wher that last spotted posisiton is, and so they keep fighitng
> 
> right now npcs CT npcs just get  stuck in the spawn conrer and dont leave it, they just dash over and over into the corner, need to get  net movmemnt up higher and figure out regrsison why and how to fix


## Triage

### 2026-11-20T23:31:04.492Z — npc-fun-counter-strike-regression

Summary: User reports recurrence of Counter-Strike NPC spawn trapping specifically in Fun Counter Strike, with CT NPCs repeatedly dashing into a spawn corner. Prior-good behavior is a user-reported baseline that aligns with the current FCS section 12 contract; live proof of the historical baseline is still missing. Investigate movement progress and coordination through a pinned FCS runtime before tuning.
Owner: npc-fun-counter-strike-regression
Priority: high

## Task linked

### 2026-11-20T23:31:20.720Z — npc-fun-counter-strike-regression

Task: fcs-npc-regression-repro (step 0)

## Context

### 2026-11-20T23:42:38.911Z — fcs-npc-live-fix

reproduction: Current FCS run 20261120_232200 client journal shows repeated movement-progress-failed episodes and NPCs dashing; correlate by actor and tick after a pinned rebuilt run.
expected_behavior: FCS NPCs preserve objective travel and only request optional rush dashes when a valid unblocked navigation intent exists and no stuck recovery owns movement. Stuck recovery moves out using its selected open direction without adding repeated dash impulses.
acceptance: Pinned post-build FCS journal shows fewer/no dash events during npc.stuck or failed navigation, while site travel and CT report response continue with positive net movement.
paths: ['src/npc/npc.cpp']
related_build: pending
related_run: 20261120_232200
evidence: {'text': 'run.json says FCS, but client journal is stale and its PID differs from manifest; use only as diagnosis evidence, not after-fix proof'}
risk: Removing recovery dashes could reduce corner escape on a route gap; retain traversal-required dash decisions made by the navigator.
proof_expectation: Post-build pinned live FCS server/client journals and visual observation correlate dash/recovery events with route state, net displacement, site rush and CT last-seen response.
context_id: ctx-6caeb230a3c8

## Assignment

### 2026-11-20T23:43:44.947Z — fcs-npc-live-fix

Owner: fcs-npc-live-fix
