# issue-768e8a2aff63: Agents can spawn parallel MiMITA runtimes but need a required cleanup and resource-pressure lifecycle

## Human report (verbatim, immutable)

> make new servre client pair, and make a issue that agents investigate futehr that  agetns can spawn multiple mimita processes its cool just ensure that onceu are don eiwth it  close it  resource pressure


## Agent suggestion

### 2026-11-27T05:42:38.636Z — rocket-league-full

Resource sample 2026-11-27T05:42:16Z (5.87 s, 12 logical CPUs), two runtimes live: rl-fuzz server pid 13860 39.7% of one core / 3.3% machine / 23.6 MB RSS; rl-fuzz client pid 29308 154.1% one core / 12.8% machine / 113.7 MB; funcounterstrike server 19248 82.2% one core / 6.9% machine / 70.8 MB; fcs clients 30012/11020 already gone. No game build active. Journals: logs/2026-11-27/20261127_053904_rl-fuzz and logs/2026-11-27/20261127_053224.

## Agent suggestion

### 2026-11-27T08:13:31.853Z — rocket-league-controller

PROCESS RECONCILIATION KILLED PEER CLIENTS (2026-11-27T08:12:47Z). My dev-loop launch request for run 20261127_081245 (mode 60, build 2026-11-27T08:12:40) hit dev-loop.process.reconcile result=failed and killed pids 26292 (run 20261127_080709) and 22128 (run 20261127_080920), both clients of build 2026-11-27T07:45:59 started by another agent's NPC flip-reset freestyle probe. The stage report shows killed=[26292,22128], remaining_clients still contained 22128, client_reconciliation=failed, and launch aborted. Servers 27708 and 26736 were retained. I did not intend or directly issue those stops; the launch reconciliation did. Evidence: logs/2026-11-27/20261127_081245/dev-loop/events.jsonl and run.json. Requested fix: scope shared-launch reconciliation to the requesting agent's own run/build or require explicit replacement intent; never stop another agent's active client because a new launch is queued. I am leaving the surviving servers untouched.
