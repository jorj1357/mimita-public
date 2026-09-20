#include "server-live-collaboration.h"

#include "debug/structured-log.h"

#include <algorithm>
#include <mutex>

namespace LiveCollaboration {

namespace {

void logRevision(const char* event, const ResourceRevisionV1& revision,
                 const char* reason, bool draftPreserved)
{
    debug::Event e;
    e.category = "LIVE_COLLABORATION";
    e.name = event;
    e.level = debug::Level::Info;
    e.fields = {
        {"resource_id", revision.resourceId},
        {"revision_id", revision.revisionId},
        {"parent_revision_id", revision.parentRevisionId},
        {"content_hash", revision.contentHash},
        {"author_client_id", revision.authorClientId},
        {"state", revisionStateName(revision.state)},
        {"reason", reason ? reason : ""},
        {"draft_preserved", draftPreserved}
    };
    debug::logEvent(e);
}

} // namespace

ServerLiveCollaboration& ServerLiveCollaboration::instance()
{
    static ServerLiveCollaboration server;
    return server;
}

RevisionDecision ServerLiveCollaboration::propose(ResourceRevisionV1 revision,
                                                   std::uint64_t expectedActive,
                                                   std::uint32_t clientId)
{
    std::lock_guard lock(mutex_);
    revision.authorClientId = clientId;
    revision.revisionId = nextRevisionId_++;
    revision.state = RevisionState::Proposed;

    auto& list = revisions_[revision.resourceId];
    const auto current = active_[revision.resourceId];
    if (expectedActive != current) {
        revision.state = RevisionState::Draft;
        list.push_back(revision);
        logRevision("live_operation_rejected", revision,
                    "stale_base_revision", true);
        return {false, true, "stale_base_revision", revision};
    }

    revision.state = RevisionState::Active;
    list.push_back(revision);
    active_[revision.resourceId] = revision.revisionId;
    logRevision("live_revision_activated", revision, "accepted", false);
    return {true, false, "accepted", revision};
}

RevisionDecision ServerLiveCollaboration::rollback(std::uint64_t resourceId,
                                                   std::uint64_t revisionId,
                                                   std::uint32_t clientId)
{
    std::lock_guard lock(mutex_);
    auto listIt = revisions_.find(resourceId);
    if (listIt == revisions_.end())
        return {false, false, "resource_not_found", {}};
    auto it = std::find_if(listIt->second.begin(), listIt->second.end(),
        [&](const ResourceRevisionV1& r) { return r.revisionId == revisionId; });
    if (it == listIt->second.end())
        return {false, false, "revision_not_found", {}};
    it->state = RevisionState::Active;
    it->authorClientId = clientId;
    active_[resourceId] = revisionId;
    logRevision("live_revision_rollback", *it, "rollback", false);
    return {true, false, "rollback", *it};
}

std::uint64_t ServerLiveCollaboration::activeRevision(std::uint64_t resourceId) const
{
    std::lock_guard lock(mutex_);
    auto it = active_.find(resourceId);
    return it == active_.end() ? 0 : it->second;
}

std::vector<ResourceRevisionV1> ServerLiveCollaboration::revisions(
    std::uint64_t resourceId) const
{
    std::lock_guard lock(mutex_);
    auto it = revisions_.find(resourceId);
    return it == revisions_.end() ? std::vector<ResourceRevisionV1>{} : it->second;
}

} // namespace LiveCollaboration
