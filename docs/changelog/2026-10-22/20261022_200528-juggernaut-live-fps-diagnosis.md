# Juggernaut FPS live diagnosis

- Time: 2026-10-22T20:05:28Z (2026-10-22 16:05:28 EDT)
- Branch / HEAD: `20261011zt` / `cbfa4b0df4e56b6f217c45dbe986b49df599b50a`
- Result: source and journal investigation only; the 240 FPS goal remains unmet and no human gameplay acceptance is claimed.
- Active run at inspection: `20261022_195815`, build 2845, Juggernaut/Dust2CyberiaV4, client/server builds match, Fighter role confirmed, 68 actors in server collision profiles.
- Measured 142 60-frame windows (8,520 frames over 56 seconds): 5,967 frames (70.04%) over budget; mean 6.482 ms/frame (about 154 FPS), main-thread CPU 6.101 ms/frame, worst frame 2,409.1 ms. GPU timers were disabled.
- Peak 60-frame allocation window requested 7,576,093,385 bytes through global `operator new`; the corresponding broad `Rendering` scope accounted for 7,530,185,584 bytes. Working set rose from 212 MB to 4,336 MB in the journal; later process sample was about 3 GiB. Allocation ownership is not proven because counters are global and child render scopes did not identify the large request.
- Server 67-NPC tick window averaged 3.354 ms total (maximum 17.532 ms); sampled 68-actor collision average was 0.091 ms. Client `Rendering` averaged 3.291 ms/frame, `Networking` 0.986, `Replay` 0.828, and `Simulation::SimulateTick` 0.625.
- Commits reviewed: `85336642`, `c1fe8acd`, `cbfa4b0d`. Build 2843 predates `cbfa4b0d`; build 2845 is the current source-generation-30 run. No controlled commit-to-commit FPS gain is established.
- Journal stopped receiving events at 2026-10-22T19:59:18Z while the dev-loop state and Windows processes still reported the run alive/responding at 20:04Z. Current FPS after the last event is unknown.
- Follow-up inspection confirmed the run state had moved to `command_queued`, but the run metadata still named build 2845 and its journal remained unchanged since 19:59:18Z. Current live FPS is unknown. No gameplay source, build, test, or process-control action was performed. Existing working-tree changes were preserved.
