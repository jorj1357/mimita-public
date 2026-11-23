# Audio mode and lower-overhead CPU profiling findings

- Confirmed the current spatial audio configuration is `legacy`; later events in the active run also report `legacy`.
- Clarified that miniaudio resampling/mixing runs in ordinary playback and does not prove the optional 3D filter/reverb path was active.
- Recorded that the prior WPR stack-walk itself materially added CPU and documented a lower-rate sampling plus unprofiled process CPU baseline plan.
- No code, build, test, or runtime behavior was changed.
