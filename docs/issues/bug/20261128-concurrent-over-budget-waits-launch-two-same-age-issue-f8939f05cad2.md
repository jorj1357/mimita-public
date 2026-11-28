# issue-f8939f05cad2: Concurrent over-budget waits launch two same-agent wakes that collide at begin/end

## Human report (verbatim, immutable)

> Two build-wait-reviewer wake requests launched in the same second (2026-11-28T08:20:38Z): wake-63b15223f7f1 (issue-30a5dbfd79be) and wake-5406ee6c4908 (issue-04ffbd3f90ed). One session's 'agent-coord.py end --agent build-wait-reviewer' closed work session work-df54984fab3f (notes 'agent end', 08:29:47Z) although it belonged to the issue-04ffbd3f90ed reviewer, while the issue-30a5dbfd79be reviewer remained active.

