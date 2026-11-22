# issue-49ded3edbe47: Tech debt audit: duplicate special contact paths, hardcoding, strange names, unused files, and wasteful workflows

## Human report (verbatim, immutable)

> for all ai agents look for and if u find things like that that are like duplicated or unnecessarily hardcoded or named weird not generically flag that as an issue and explain why is a issue and explain a suggested fix for the issue then leave for review


## Review (needs-review)

### 2026-11-20T04:26:28.072Z — graph-coordination

Reviewer: graph-coordination

Spec and scanner are present. Initial scan: 1537 files, 122 candidates (38 special contact paths, 14 direct health writes mostly host adapter/serialization, 4 weapon-name branches, 63 stale TODOs, 3 large files). Focused issues and exemptions still need review.

## Scheduled independent review

### 2026-11-22T04:15:48.148Z — build-wait-reviewer

A bounded independent review was queued as wake wake-45e91846d4b1 after 239 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-22T04:27:53.356Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review (issue-review-sweeper, wake-45e91846d4b1, 2026-11-22T04:26Z), read-only; no source edit, build, close, or fix claimed.

OBSERVED FACTS
1. Brief/readiness: kind=tech-debt, state=in_review, owner=graph-coordination, priority=high, created 2026-11-20T04:20Z. issue readiness returns ready=false, score 1/8; missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, risk. issue brief shows tasks=[], builds=[], runs=[], leases=[], and the only review is #4 by graph-coordination (the owner) 2026-11-20T04:26Z, verdict needs-review. The 2026-11-22T04:15Z doc entry is a scheduling note, not a review.
2. Proof: proof status contains no row referencing issue-49ded3edbe47 or a tech-debt task; no exemption is recorded for this issue.
3. Scanner re-run today (`python devscripts/tech-debt-audit.py`, read-only): files_scanned=1545, finding_count=122; direct_health_write 14, large_file 3, special_contact_path 38, stale_todo 63, weapon_name_branch 4. This is the same 122 distribution the owner recorded on 2026-11-20 (1537 files); file count is +8, classified findings delta is 0.
4. large_file: src/mapeditor/map-editor.cpp (275688 B), src/network/server-gamemode.cpp (538106 B), devscripts/dev-loop.py (396318 B).
5. The scanner reports itself and non-debt text: 4 special_contact_path hits are its own regex literals at devscripts/tech-debt-audit.py:47-50; 3 hits are in generated src/host/generated/packet-registry-v1.h; 3 are PACKET_* enum declarations in src/network/packets.h. Of 4 weapon_name_branch hits, 2 are false positives (src/combat/spyknife-damage.cpp:2 is an #include; src/network/server-contact.h:4 is a doc comment); only src/network/confirmed-damage-presentation.cpp:137-138 are real weapon-name string branches. So at least 6 of 122 are self/doc false positives, before generated-file noise.
6. Candidate disposition: zero focused issues exist from the 122. issue list shows only 2 tech-debt issues (this one and issue-f643532871ac, wake-envelope duplication, unrelated) and 0 issues of kind duplication, though docs/specs/dev-loop/agent-self-improvement.md:227 says findings are filed as tech-debt or duplication. There is no suppression/allowlist and no exemption for the 14 direct_health_write hits.
7. Periodicity: the only references to tech-debt-audit.py in the repository are docs/changelog text (agent-self-improvement.md:244-248 and the 2026-11-20 changelog). No dev-loop hook, scheduled task, or agent task invokes it. The documented periodic audit is a manual command with no execution evidence.
8. Delivered portion does exist: generic-owner gate at AGENTS.md:106 (0d) and agent-self-improvement.md:212-248, and the scanner is read-only in behavior (it reads files and prints; it writes nothing).

INFERENCE / UNCERTAINTY
- The request has two halves. The gate + spec + read-only scanner half is delivered. The "periodically run, file findings as issues, leave for review" half has no execution evidence: 122 candidates are unchanged across ~48h and 0 were triaged.
- Because the scanner has no suppression list and no baseline/delta, a raw run restates the same 122 including known false positives; a reviewer cannot separate new debt from noise. That is a concrete cause of non-convergence, not merely missing agent effort.
- Uncertain: no activity event shows an owner attempted triage, so "never assigned" vs "assigned and stalled" cannot be distinguished from the record. Generated directories are excluded only for the large_file check, not for the contact/health checks.

CONTRARY EVIDENCE
- The generic-owner rule and scanner are real and load-bearing (AGENTS.md:106; spec 212-248), so this issue is not empty. Several candidate classes are plausibly legitimate by design (host adapter health mirroring, server-gamemode resets, packet enum declarations); their presence is not proof of debt. The missing artifact is triage, not findings.

NEXT DISCRIMINATING CHECK
1. Owner graph-coordination backfills the 7 missing readiness fields and defines umbrella acceptance, e.g. "each candidate is either filed as a focused tech-debt/duplication issue with owner+evidence, or recorded in a reviewed suppression file with a reason"; record a process/docs proof exemption with a reason. This is the decisive missing artifact.
2. Smallest tooling experiment: exclude the scanner's own file, src/host/generated/, and include/comment-only matches; add a baseline/delta mode so a run reports new/removed counts. Then run once and file the top-3 genuine findings.
3. Re-measure: focused-issue count > 0, duplication issues > 0 or a recorded "no real duplication" decision, and a reported findings delta versus baseline.

VERDICT: needs-evidence (owner readiness + triage execution + periodic-run wiring); not closeable.
