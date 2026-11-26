# issue-f5f8af2fe6bd: Mark every MiMITA launch with initiator and reason

## Human report (verbatim, immutable)

> add an issue that we need human launch as well like if i launch somethign its marked as human launched it so that all launches have a thingn proving why


## Agent suggestion

### 2026-11-26T15:41:32.365Z — vehicle-freeplay-launch-regression

Additional concrete evidence: run 20261126_145549 client journal client-01/events-0001.jsonl seq1658 records network.gui-server.launch from client PID 29616 at 2026-11-26T14:56:11.806Z, creating child PID 12892 for vehicle_freeplay/arena1 on port 1357. Fields server_instance_id, client_events_jsonl, and server_events_jsonl are empty; no actor/initiator/reason field exists, and run.json has no PID 12892 instance. Therefore the initiating human/agent/automation cannot be attributed. Matching crash dump is .dev/crashes/20261126_145549/crash-2026-11-26_09-56-12-12892.txt.
