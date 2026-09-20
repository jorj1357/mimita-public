#include "resource-revision.h"

namespace LiveCollaboration {

const char* revisionStateName(RevisionState state)
{
    switch (state) {
    case RevisionState::Draft: return "draft";
    case RevisionState::Proposed: return "proposed";
    case RevisionState::Active: return "active";
    case RevisionState::Rejected: return "rejected";
    case RevisionState::Archived: return "archived";
    }
    return "unknown";
}

} // namespace LiveCollaboration
