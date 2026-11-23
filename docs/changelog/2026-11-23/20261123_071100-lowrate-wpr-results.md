# Low-rate WPR CPU capture analysis

- Verified the 10 ms setting reduced Sampled Profile events per second about 10.6x and Sampled Profile stack records about 8.3x.
- Found verbose context-switch/ready-thread stack collection still produced hundreds of thousands of stack records; the ETL remained 369 MiB and MiMITA CPU share was similar to the earlier non-matched trace.
- Confirmed `CPU.light` keeps sampled CPU profiling without the verbose System Stacks listed for `CPU`; recommended it for the next matched low-overhead leaf-function capture.
- Recorded process CPU, host utilization, runtime identity, unknown workload counters, and artifacts in the CPU issue. No code or runtime behavior changed.
