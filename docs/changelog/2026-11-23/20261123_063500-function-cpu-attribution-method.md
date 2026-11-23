# Per-function CPU attribution method

- Explained how to combine same-interval process CPU measurements with exclusive sampled stack weights to estimate each MiMITA function's share of whole-machine CPU.
- Distinguished exclusive function cost from inclusive caller cost and identified symbols, process boundaries, and sampler overhead as required evidence.
- No code, build, test, or runtime behavior was changed.
