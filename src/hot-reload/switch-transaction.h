// 09 15 2026
/* purpose
* Explicit switch-boundary validation for the hot-generation transaction. Before
* a committed candidate G may become active it must be proven that the prepared
* migration plan matches the ACTUAL current source generation F and the candidate
* G. A missing, stale, or invalid plan is an explicit rejection: G is not
* activated and F (last-good) stays active. No best-effort migration happens at
* the boundary.
*
* This is pure validation; it owns no transport, loading, or state mutation. The
* actual migration commit remains the existing atomic schema update.
*/
#pragma once

#include <cstdint>

#include "hot-reload/migration-prep.h"

namespace MimitaRuntime {

enum class SwitchRejection : std::uint32_t {
    None = 0,
    NoCandidate = 1,   // nothing to switch to
    PlanMissing = 2,   // migration must be prepared explicitly, even when no-op
    PlanStale = 3,     // prepared for a different source/target generation
    PlanInvalid = 4,   // preparation reported a hard failure
};

inline const char* switchRejectionName(SwitchRejection r)
{
    switch (r) {
    case SwitchRejection::None: return "none";
    case SwitchRejection::NoCandidate: return "no-candidate";
    case SwitchRejection::PlanMissing: return "plan-missing";
    case SwitchRejection::PlanStale: return "plan-stale";
    case SwitchRejection::PlanInvalid: return "plan-invalid";
    }
    return "unknown";
}

struct SwitchTransactionFactsV1 {
    std::uint64_t activeGeneration = 0;
    std::uint64_t candidateGeneration = 0;
    const MigrationPlanV1* plan = nullptr;  // may be null
};

// Initial load (no active source) needs no migration. Otherwise a plan for the
// exact F -> G transition is mandatory, and a no-op plan is still validated.
inline SwitchRejection validateSwitchTransaction(const SwitchTransactionFactsV1& f)
{
    if (f.candidateGeneration == 0)
        return SwitchRejection::NoCandidate;
    if (f.activeGeneration == 0)
        return SwitchRejection::None;
    if (f.plan == nullptr || f.plan->outcome == MigrationOutcome::Failed)
        return f.plan == nullptr ? SwitchRejection::PlanMissing
                                 : SwitchRejection::PlanInvalid;
    if (!f.plan->valid)
        return SwitchRejection::PlanInvalid;
    if (f.plan->fromGeneration != f.activeGeneration ||
        f.plan->toGeneration != f.candidateGeneration)
        return SwitchRejection::PlanStale;
    return SwitchRejection::None;
}

} // namespace MimitaRuntime
