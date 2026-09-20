#pragma once

#include "live-code/live-operation.h"

#include <cstdint>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace LiveCollaboration {

struct RevisionDecision {
    bool accepted = false;
    bool draftPreserved = false;
    const char* reason = "unknown";
    ResourceRevisionV1 revision{};
};

class ServerLiveCollaboration {
public:
    static ServerLiveCollaboration& instance();

    RevisionDecision propose(ResourceRevisionV1 revision,
                              std::uint64_t expectedActiveRevision,
                              std::uint32_t clientId);
    RevisionDecision rollback(std::uint64_t resourceId,
                              std::uint64_t revisionId,
                              std::uint32_t clientId);
    std::uint64_t activeRevision(std::uint64_t resourceId) const;
    std::vector<ResourceRevisionV1> revisions(std::uint64_t resourceId) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, std::vector<ResourceRevisionV1>> revisions_;
    std::unordered_map<std::uint64_t, std::uint64_t> active_;
    std::uint64_t nextRevisionId_ = 1;
};

} // namespace LiveCollaboration
