#include "live-collaboration-client.h"

#include "server-live-collaboration.h"

namespace LiveCollaboration {

RevisionDecision LiveCollaborationClient::propose(
    ResourceRevisionV1 revision, std::uint64_t expectedActiveRevision,
    std::uint32_t clientId)
{
    return ServerLiveCollaboration::instance().propose(
        revision, expectedActiveRevision, clientId);
}

} // namespace LiveCollaboration
