# Generation 581 compile and link timing

Re-read the retained game-DLL summary and timeline for `game-dll-20261124T043451389265Z-b1bf97` to resolve what its “changed-source preflight” measured.

That phase invokes `run_compile_batch(to_build)`: it is the changed translation unit's actual compilation. For the one changed source `src/gui/ui-system.cpp`, that compile took 15.388 seconds. The later `compile` phase took 2.076 seconds for the generated module-identity provider. LLD linked 642 objects (486,753,124 bytes) in 12.605 seconds; object-cache scan took 5.464 milliseconds; publish took 0.447 seconds. Total attempt time was 49.506 seconds, with 2 units compiled and 641 skipped.

The automatic sidecar measured host CPU at 99.885% mean and 100% peak, with 469,856,256 bytes minimum available RAM. The same-run server journal showed Juggernaut Horde, 67 NPCs and 0 players; no client was connected and resource alignment was unavailable. This is not paired runtime proof and does not show that resource pressure caused the compile or link time.

The evidence and next optimization priority are appended to the existing DLL latency and CPU issues. No build, source edit, runtime process control, or new sample was performed.
