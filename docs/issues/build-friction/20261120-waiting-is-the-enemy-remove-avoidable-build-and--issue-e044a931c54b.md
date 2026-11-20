# issue-e044a931c54b: Waiting is the enemy: remove avoidable build and test waits

## Human report (verbatim, immutable)

> ensure waiting is marked as an issue like a principle to get wait times down as close to 0 sec as possible


## Assignment

### 2026-11-20T03:17:28.752Z — build-runtime-subagents

Owner: build-runtime-subagents

## Agent suggestion

### 2026-11-20T06:04:03.845Z — root-build-wait-efficiency

Add the user's specific >60s trigger: when an agent estimates or observes a build/test wait over one minute, log duration/ETA and blocker, create or link a build-friction issue, and investigate the current queue/worker phase immediately. Reuse this issue for the same root cause and append evidence rather than duplicate tasks.

## User quote

### 2026-11-20T06:04:04.653Z — root-build-wait-efficiency

Via root-build-wait-efficiency; supports unspecified:

> every time an agent has this , like, talking about build and how it coudl take N minutes, if its like more than a minute, that is a issue, and should ahve a issue and begin invesitgating furhter why, like this should  be there like  waiting issue time issue need to get  these times tracked and super aware
