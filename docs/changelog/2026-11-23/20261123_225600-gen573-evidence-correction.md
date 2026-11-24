# Generation 573 evidence corrections

- Session ID: work-630bc9ded09d
- Date: 2026-11-23
- Scope: reconstruct the complete gen573 translation-unit invalidation set from the timeline, correct prior reports, and append a no-build resource sample.

## Evidence corrections

The build summary retains only32 cache invalidation reasons, but the attempt timeline has all69 `cache.invalidation` events for69 unique dependent translation units. Counts are43 dependencies on `src/gamemode/gamemode.h`,17 on `src/render/post-fx.h`, and9 on `src/entities/aimbody-config.h`. The other three compiled units are the two edited `.cpp` files plus the generated module-identity provider. This accounts for all72 compiled units;571/643 were skipped. The summary presentation is truncated; the underlying timeline is complete.

The earlier `.dev/perf-captures/postgen572-mode3-20261123T2211Z.csv` label was corrected: its22:11:54–22:12:08Z interval overlapped the gen573 build during/near changed-source preflight. It is an in-build sample, not a post-build baseline.

## New resource evidence

Resource-profiler artifact `.dev/perf-captures/runtime-gen573-horde-20261123T2245Z.csv` covers22:46:29.683–22:46:43.197Z, a no-build period on12 logical CPUs. Host CPU was100% throughout; available RAM mean~678MiB/min~549MiB. Server22184/client32832 together used31.148% host CPU; root Bloodtest server10164 used4.582%. RobloxPlayerBeta PID26216 averaged19.756% host, FL64 PID32712 11.185%, SNAPOS64 PID7384 9.692%. Client journal in-window confirms Juggernaut Horde/67 remote NPCs; no server tick/workload row is present in that exact window, and later22:48:29.636 evidence is only nearby corroboration. This is not a matched causal before/after to the build; the machine was saturated both during and after the build at separate times. Resource sidecar gaps remain: matched pre-build baseline, full journal alignment, host commit limit, and disk I/O.

## Current state and validation

At inspection, root dev-loop had no active build and eight pending game inputs from other owners; the isolated client/server PIDs remained alive despite stale runtime registry state. No source edits, compiler invocation, build request, process launch, or restart was made. Shared latency issue: `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`. The <=10-second edit-to-live acceptance remains unmet.
