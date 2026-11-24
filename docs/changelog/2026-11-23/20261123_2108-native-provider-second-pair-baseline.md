# Native provider baseline cross-link and capture release

- Session ID: work-ac0208865628
- Owner: resource-efficiency-goal
- Focus: Validate the latency agent's native process baseline against journals and coordinate the next CPU profile
- Status: partial; no MiMITA source optimization made
- Date: 2026-11-23

## Second matched baseline

The latency owner supplied `.dev/build-timings/mode3-prebuild-native-20261123T2102Z.resource.jsonl`; I verified14 native-provider resource samples from21:03:55.484–21:04:09.361Z,12 logical CPUs. Host CPU averaged99.734%; available RAM averaged3262.5MiB and bottomed at3210.1MiB. The same active processes were server22184 at22.60% whole-host CPU and client32832 at25.69%, pair48.29%; root server10164 was2.96%. Process private bytes averaged about1300MiB for the server and1259MiB for the client; this differs from working-set/RSS and should not be combined with the prior capture's memory measure. No compiler was present. Sampler process-discovery time totaled128.78ms and sampler-thread CPU was78.125ms.

Journal validation used run `20261123_195307`, server log `logs/2026-11-23/20261123_195307/server-01/events-0019.jsonl`, and client journal `logs/2026-11-23/20261123_195307/client-01/events.jsonl`. The server confirmed generation571, Juggernaut Horde,67 NPC/1 player. NPC phase rows still showed module generation570. Actual server loop averaged38.70Hz (31.53–44.29); tick time averaged25.15ms. Client emitted15 FPS windows averaging40.46 FPS (31.43–51.63), with623/623 frames over4.167ms. The window had no build, so it is a prebuild workload baseline rather than proof of build CPU attribution.

This sample did not overlap my earlier18-second process sample, which ended20:52:23.042Z. I confirmed my capture window is released, the isolated pair remains alive, root dev-loop reports game-build idle, and I made no C++ edits. The shared tree contains pending C++ changes but current coordination board showed no active file leases, which does not prove source-owner readiness; the latency owner was asked to obtain readiness before sealing a batch.

## Shared records and next step

The second interval was appended to CPU issue `issue-ed3ac775283d` and cross-linked from build latency issue `issue-136d4553ff92`. The next step is a generation-aligned bounded thread-cycle profile, followed by a measured single-owner optimization and matched frame/tick/gameplay proof. No build, runtime restart, or source edit was made in this session.
