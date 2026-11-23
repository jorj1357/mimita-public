# Separate WPR CPU attribution cross-link

## Finding

The CPU-reduction agent reported a read-only WPR module attribution from a retained runtime trace with the 67-NPC workload. The sampled client rows were `ntoskrnl.exe` 7.71% host, `game-g546.dll` 4.57%, `ntdll.dll` 1.32%, and NVIDIA OpenGL `nvoglv64.dll` 1.13%. Server top rows included `ntoskrnl.exe` 4.36% and the game DLL 2.40%. Audio-related rows were small: `AudioSes.dll` 0.02%, `IntcAudioBus.sys` 0.01%, and the `audiodg` process 0.20% host.

This sample gives no evidence that audio dominates CPU. Kernel and driver attribution is a stronger next lead for CPU profiling. The percentages are sampled module attribution, not inclusive stack cost or total process CPU shares.

## Relationship to the build-latency goal

This WPR capture is from a separate runtime trace and does not overlap the generation 554 cleanup build window. Do not combine its percentages with the coarse 08:55 host/process sample, or treat it as build-time CPU attribution or proof that a kernel, GPU-driver, or audio component caused compilation latency. The separate build attempt remained 43.7s total and 46.4s to server activation; its detailed timings are recorded in `20261123_0902-mode3-pch-off-and-generation554.md` and `.dev/build-timings/game-dll-20261123T085515045094Z-a48a01.jsonl`.

The finding and method limitations are recorded in CPU issue `issue-ed3ac775283d` and the CPU agent's `20261123_0846-wpr-workload-runtime-alignment.md`. It is cross-linked in build-latency issue `issue-136d4553ff92`. No source, build, or runtime change was made for this report.
