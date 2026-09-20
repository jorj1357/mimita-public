#pragma once

#include <cstdint>
#include <string>

namespace LiveCollaboration {

enum class ResourceKind : std::uint32_t {
    Code = 1,
    Json = 2,
    Model = 3,
    Animation = 4,
    Sound = 5,
    Texture = 6,
    Other = 255,
};

enum class RevisionState : std::uint32_t {
    Draft = 1,
    Proposed = 2,
    Active = 3,
    Rejected = 4,
    Archived = 5,
};

struct ResourceRevisionV1 {
    std::uint64_t resourceId = 0;
    std::uint64_t revisionId = 0;
    std::uint64_t parentRevisionId = 0;
    std::uint64_t contentHash = 0;
    std::uint64_t createdAtUtcMs = 0;
    std::uint64_t createdTick = 0;
    std::uint32_t authorClientId = 0;
    ResourceKind resourceKind = ResourceKind::Other;
    RevisionState state = RevisionState::Draft;
};

const char* revisionStateName(RevisionState state);

} // namespace LiveCollaboration
