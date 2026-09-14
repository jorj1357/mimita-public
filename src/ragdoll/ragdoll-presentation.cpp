// 09 12 2026
/* purpose
* Implements remote ragdoll presentation buffering and interpolation.
* Derived-only: never writes authoritative components and never feeds the solver.
*/
#include "ragdoll/ragdoll-presentation.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "entities/player.h"
#include "ragdoll/ragdoll-body.h"
#include "ragdoll/ragdoll-mode-config.h"
#include "telemetry/telemetry.h"

namespace Ragdoll {

namespace {
constexpr std::size_t kMaxBufferedFrames = 64;
constexpr double kStaleSeconds = 0.5;
constexpr double kMaxExtrapolationSeconds = 0.1;
constexpr double kTickHz = 60.0;

std::uint64_t nowMs()
{
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

RagdollPresentation& RagdollPresentation::instance()
{
    static RagdollPresentation presentation;
    return presentation;
}

void RagdollPresentation::pushFrame(std::uint32_t ownerActorId, const Snapshot& snapshot)
{
    Owner& owner = owners_[ownerActorId];
    PresentationFrame frame;
    frame.sourceTick = (std::uint32_t)snapshot.tick;
    frame.receivedMs = nowMs();
    frame.limbCount = std::min((std::uint32_t)snapshot.limbCount,
                               (std::uint32_t)kMaxSnapshotLimbs);
    for (std::uint32_t i = 0; i < frame.limbCount; ++i) {
        frame.position[i] = glm::vec3(snapshot.limbs[i].position[0],
                                      snapshot.limbs[i].position[1],
                                      snapshot.limbs[i].position[2]);
        frame.orientation[i] = glm::quat(snapshot.limbs[i].rotation[0],
                                         snapshot.limbs[i].rotation[1],
                                         snapshot.limbs[i].rotation[2],
                                         snapshot.limbs[i].rotation[3]);
    }

    if (owner.buffer.empty()) {
        owner.firstReceivedMs = frame.receivedMs;
        owner.firstTick = frame.sourceTick;
    }

    if (owner.buffer.empty() || frame.sourceTick >= owner.buffer.back().sourceTick) {
        if (!owner.buffer.empty() && frame.sourceTick == owner.buffer.back().sourceTick)
            owner.buffer.back() = frame;
        else
            owner.buffer.push_back(frame);
    } else {
        auto it = owner.buffer.begin();
        while (it != owner.buffer.end() && it->sourceTick < frame.sourceTick)
            ++it;
        if (it != owner.buffer.end() && it->sourceTick == frame.sourceTick)
            *it = frame;
        else
            owner.buffer.insert(it, frame);
    }

    while (owner.buffer.size() > kMaxBufferedFrames)
        owner.buffer.pop_front();

    owner.lastReceivedMs = frame.receivedMs;

    Telemetry::EntityCounters counters;
    counters.networkUpdates = 1;
    counters.networkBytes = sizeof(Snapshot);
    counters.lastTouchedTick = frame.sourceTick;
    Telemetry::Registry::instance().addEntityCounter(ownerActorId, counters);
}

bool RagdollPresentation::present(std::uint32_t ownerActorId, Player& player,
                                  double delaySeconds)
{
    auto it = owners_.find(ownerActorId);
    if (it == owners_.end())
        return false;
    Owner& owner = it->second;
    if (owner.buffer.size() < 2)
        return false;
    MIMITA_TELEMETRY_SCOPE("RagdollPresentationInterp");
    const std::uint64_t now = nowMs();
    if (now - owner.lastReceivedMs > (std::uint64_t)(kStaleSeconds * 1000.0)) {
        ++owner.stale;
        return false;
    }

    // Build the per-owner template once, from the remote player's model. The
    // static limb->mesh mapping comes from the local model; only transforms are
    // authored by the network.
    if (!owner.templateReady) {
        if (player.physicalBody.parts.empty() || player.perfectPoseSkeleton.nodes.empty())
            return false;
        Ragdoll::buildBody(player, RagdollModeConfig::instance().data(), owner.body);
        owner.templateLimbCount = (std::uint32_t)owner.body.parts.size();
        owner.templateReady = owner.templateLimbCount > 0;
        if (!owner.templateReady)
            return false;
        owner.hasRenderTick = false;
    }

    // Presentation clock: anchor at the first received frame and advance on the
    // wall clock, delayed by the configured interpolation delay. This is a
    // derived read-only view; it never feeds authoritative state.
    if (!owner.hasRenderTick) {
        owner.firstTick = owner.buffer.front().sourceTick;
        owner.firstReceivedMs = owner.buffer.front().receivedMs;
        owner.renderTick = (double)owner.firstTick;
        owner.hasRenderTick = true;
    }
    owner.renderTick = (double)owner.firstTick
        + ((double)now - (double)owner.firstReceivedMs) / 1000.0 * kTickHz
        - delaySeconds * kTickHz;

    const PresentationFrame& oldest = owner.buffer.front();
    const PresentationFrame& newest = owner.buffer.back();

    auto applyFrame = [&](std::size_t index, double alpha) {
        const PresentationFrame& a = owner.buffer[index];
        const PresentationFrame& b = owner.buffer[index + 1];
        owner.lastAlpha = alpha;
        const std::size_t n = std::min({owner.body.parts.size(),
                                        (std::size_t)a.limbCount,
                                        (std::size_t)b.limbCount});
        for (std::size_t i = 0; i < n; ++i) {
            owner.body.parts[i].body.position =
                glm::mix(a.position[i], b.position[i], (float)alpha);
            owner.body.parts[i].body.orientation = glm::normalize(
                glm::slerp(a.orientation[i], b.orientation[i], (float)alpha));
        }
    };

    if ((double)owner.renderTick <= (double)oldest.sourceTick) {
        const std::size_t n = std::min(owner.body.parts.size(), (std::size_t)oldest.limbCount);
        owner.lastAlpha = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            owner.body.parts[i].body.position = oldest.position[i];
            owner.body.parts[i].body.orientation = oldest.orientation[i];
        }
    } else if ((double)owner.renderTick >= (double)newest.sourceTick) {
        // Extrapolate from the last two frames, capped and presentation-only.
        const PresentationFrame& a = owner.buffer[owner.buffer.size() - 2];
        const PresentationFrame& b = newest;
        const double tickDelta = (double)b.sourceTick - (double)a.sourceTick;
        double dt = (owner.renderTick - (double)b.sourceTick) / kTickHz;
        dt = std::clamp(dt, 0.0, kMaxExtrapolationSeconds);
        ++owner.extrapolations;
        owner.lastAlpha = 1.0;
        const std::size_t n = std::min({owner.body.parts.size(),
                                        (std::size_t)a.limbCount,
                                        (std::size_t)b.limbCount});
        for (std::size_t i = 0; i < n; ++i) {
            glm::vec3 velocity(0.0f);
            if (tickDelta > 0.0)
                velocity = (b.position[i] - a.position[i]) * (float)(kTickHz / tickDelta);
            owner.body.parts[i].body.position = b.position[i] + velocity * (float)dt;
            owner.body.parts[i].body.orientation = b.orientation[i];
        }
    } else {
        std::size_t index = 0;
        for (std::size_t i = 0; i + 1 < owner.buffer.size(); ++i) {
            if ((double)owner.buffer[i].sourceTick <= owner.renderTick &&
                owner.renderTick <= (double)owner.buffer[i + 1].sourceTick) {
                index = i;
                break;
            }
        }
        const double span = (double)owner.buffer[index + 1].sourceTick
                          - (double)owner.buffer[index].sourceTick;
        const double alpha = span > 0.0
            ? (owner.renderTick - (double)owner.buffer[index].sourceTick) / span
            : 0.0;
        applyFrame(index, std::clamp(alpha, 0.0, 1.0));
    }

    player.ragdollModeActive = true;
    Ragdoll::applyBodyToPlayer(player, owner.body, RagdollModeConfig::instance().data());
    player.updateModelWorldTransforms();
    return true;
}

bool RagdollPresentation::active(std::uint32_t ownerActorId) const
{
    auto it = owners_.find(ownerActorId);
    if (it == owners_.end())
        return false;
    if (it->second.buffer.size() < 2)
        return false;
    return nowMs() - it->second.lastReceivedMs
        <= (std::uint64_t)(kStaleSeconds * 1000.0);
}

void RagdollPresentation::clear(std::uint32_t ownerActorId)
{
    owners_.erase(ownerActorId);
}

void RagdollPresentation::clearAll()
{
    owners_.clear();
}

std::size_t RagdollPresentation::bufferDepth(std::uint32_t ownerActorId) const
{
    auto it = owners_.find(ownerActorId);
    return it == owners_.end() ? 0 : it->second.buffer.size();
}

double RagdollPresentation::lastAlpha(std::uint32_t ownerActorId) const
{
    auto it = owners_.find(ownerActorId);
    return it == owners_.end() ? 0.0 : it->second.lastAlpha;
}

std::uint64_t RagdollPresentation::extrapolationCount(std::uint32_t ownerActorId) const
{
    auto it = owners_.find(ownerActorId);
    return it == owners_.end() ? 0 : it->second.extrapolations;
}

std::uint64_t RagdollPresentation::staleCount(std::uint32_t ownerActorId) const
{
    auto it = owners_.find(ownerActorId);
    return it == owners_.end() ? 0 : it->second.stale;
}

} // namespace Ragdoll
