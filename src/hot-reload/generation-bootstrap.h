// 09 15 2026
/* purpose
* Late-join generation bootstrap state machine. A newly connected peer must NOT
* participate in ordinary simulation until its LOCAL active generation equals the
* server's ACTIVE generation, proven by artifact hash + manifest verification.
* This is not the READY path: READY means "prepared for a FUTURE coordinated
* switch"; bootstrap means "become ACTIVE on the generation the server is already
* running". No fake SWITCH is scheduled for a joining peer.
*
* Reuses ArtifactCache identity + verifyGeneration. Owns no transport and does no
* file IO. A fresh joiner with no prior local world state does NOT run an F->G
* migration; it loads/activates G then synchronizes authoritative G world state.
*/
#pragma once

#include <cstdint>

#include "hot-reload/generation-verify.h"

namespace MimitaRuntime {

enum class BootstrapState : std::uint32_t {
    Idle = 0,
    AwaitingMetadata = 1,
    Acquiring = 2,
    Verifying = 3,
    Ready = 4,
    Failed = 5,
};

inline const char* bootstrapStateName(BootstrapState s)
{
    switch (s) {
    case BootstrapState::Idle: return "idle";
    case BootstrapState::AwaitingMetadata: return "awaiting-metadata";
    case BootstrapState::Acquiring: return "acquiring";
    case BootstrapState::Verifying: return "verifying";
    case BootstrapState::Ready: return "ready";
    case BootstrapState::Failed: return "failed";
    }
    return "unknown";
}

// Bootstrap-specific failures beyond VerifyFailure (stored in `failure` as a
// VerifyFailure when verification fails, or these otherwise).
enum class BootstrapFailure : std::uint32_t {
    None = 0,
    NoTarget = 1,
    NotLoaded = 2,
};

// Bounded: one target generation at a time. Any packet tagged with a different
// generation cannot complete the current bootstrap.
struct GenerationBootstrapV1 {
    std::uint64_t targetGeneration = 0;
    std::uint64_t manifestArtifactHash = 0;  // identity of the target manifest
    BootstrapState state = BootstrapState::Idle;
    std::uint32_t failure = 0;               // VerifyFailure or BootstrapFailure

    bool active() const
    {
        return state == BootstrapState::AwaitingMetadata ||
            state == BootstrapState::Acquiring ||
            state == BootstrapState::Verifying;
    }

    // Server advertises the generation it is already running.
    void begin(std::uint64_t serverActiveGeneration)
    {
        targetGeneration = serverActiveGeneration;
        manifestArtifactHash = 0;
        failure = 0;
        state = serverActiveGeneration == 0 ? BootstrapState::Failed
                                            : BootstrapState::AwaitingMetadata;
        if (serverActiveGeneration == 0)
            failure = (std::uint32_t)BootstrapFailure::NoTarget;
    }

    // Manifest for the target arrives (identity must match the target).
    void onMetadata(std::uint64_t generation, std::uint64_t artifactHash,
                    bool artifactCached)
    {
        if (generation != targetGeneration)
            return;  // stale/superseded packet cannot advance this bootstrap
        manifestArtifactHash = artifactHash;
        state = artifactCached ? BootstrapState::Verifying
                               : BootstrapState::Acquiring;
    }

    // Artifact bytes fully received (or served from cache).
    void onArtifactAcquired(std::uint64_t generation)
    {
        if (generation != targetGeneration)
            return;
        if (state == BootstrapState::Acquiring ||
            state == BootstrapState::AwaitingMetadata)
            state = BootstrapState::Verifying;
    }

    // Verify the received manifest against REAL local facts and confirm the code
    // is loaded. Verification is required even for a cache hit.
    void complete(std::uint64_t generation, bool codeLoaded,
                  const GenerationManifestV1& manifest,
                  const GenerationLocalFactsV1& facts)
    {
        if (generation != targetGeneration)
            return;  // late packet for an old target: no activation
        if (state != BootstrapState::Verifying && state != BootstrapState::Acquiring)
            return;
        const VerifyFailure vf = verifyGeneration(manifest, facts);
        if (vf != VerifyFailure::None) {
            failure = (std::uint32_t)vf;
            state = BootstrapState::Failed;
            return;
        }
        if (!codeLoaded) {
            failure = (std::uint32_t)BootstrapFailure::NotLoaded;
            state = BootstrapState::Failed;
            return;
        }
        state = BootstrapState::Ready;
        failure = 0;
    }

    // The server's active generation changed while bootstrapping. Re-target; the
    // cached artifact stays useful, but nothing from the old target may activate.
    void onServerActiveChanged(std::uint64_t serverActiveGeneration)
    {
        if (serverActiveGeneration == targetGeneration)
            return;
        if (serverActiveGeneration == 0)
            return;
        begin(serverActiveGeneration);
    }

    // World participation requires: bootstrap done for the target AND the local
    // active generation equal to the server's ACTIVE generation.
    bool worldParticipationAllowed(std::uint64_t serverActiveGeneration,
                                   std::uint64_t localActiveGeneration) const
    {
        return state == BootstrapState::Ready &&
            targetGeneration == serverActiveGeneration &&
            localActiveGeneration == serverActiveGeneration;
    }
};

} // namespace MimitaRuntime
