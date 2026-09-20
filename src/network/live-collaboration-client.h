#pragma once

#include "server-live-collaboration.h"

#include <cstdint>

namespace LiveCollaboration {

class LiveCollaborationClient {
public:
    RevisionDecision propose(ResourceRevisionV1 revision,
                             std::uint64_t expectedActiveRevision,
                             std::uint32_t clientId);
};

} // namespace LiveCollaboration
