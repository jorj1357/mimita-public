# issue-04079f42c6f9: Visible melee contact passes through and is not accepted

## Human report (verbatim, immutable)

> like melee hit like the weapon just passes thru the enemy on my screen i want it to be a hit each time i see me as the client i see it hit them 99.99% of the time its a hit prediciton is correct 99.99% of the time


## Triage

### 2026-11-20T02:43:15.402Z — melee-networking

Summary: Add blade-capsule fallback when configured damage boxes miss, preserve same shape in claim, add contact-source evidence, verify with NPC/PvP.
Owner: melee-networking
Priority: high

## Task linked

### 2026-11-20T02:43:16.159Z — melee-networking

Task: melee-visible-contact (step 0)

## Review (needs-evidence)

### 2026-11-20T03:57:06.906Z — melee-networking

Reviewer: melee-networking

Pinned run 20261120_034533 used artifact 2026-11-20T02:29:08.406738Z and game generation 393. Client produced 171 claims/123 batches; server journal had zero melee.contact.batch/claim decisions. First divergence is server packet dispatch/receipt, not final damage. Earlier report artifact was stale; current journal confirms 171 unanswered claims. Need add packet-dispatch evidence and rerun after build.
