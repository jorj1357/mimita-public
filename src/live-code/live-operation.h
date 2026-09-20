#pragma once

#include "resource-revision.h"

#include <cstdint>
#include <string>
#include <vector>

namespace LiveCollaboration {

enum class LiveOperationType : std::uint32_t {
    AddCode = 1,
    UpdateCode = 2,
    RemoveCode = 3,
    SetProperty = 4,
    AddAsset = 5,
    RemoveAsset = 6,
    ActivateRevision = 7,
    RollbackRevision = 8,
};

enum class LiveOperationState : std::uint32_t {
    Received = 1,
    Validated = 2,
    Queued = 3,
    Started = 4,
    Completed = 5,
    Rejected = 6,
    Superseded = 7,
    Cancelled = 8,
};

struct LiveOperationV1 {
    std::uint64_t operationId = 0;
    std::uint64_t resourceId = 0;
    std::uint64_t revisionId = 0;
    std::uint64_t baseRevisionId = 0;
    std::uint32_t authorClientId = 0;
    LiveOperationType type = LiveOperationType::SetProperty;
    LiveOperationState state = LiveOperationState::Received;
    std::uint32_t priority = 0;
    std::uint64_t requestedAtTick = 0;
    std::uint64_t scheduledTick = 0;
    std::vector<std::uint8_t> payload;
};

const char* operationStateName(LiveOperationState state);
const char* operationTypeName(LiveOperationType type);

} // namespace LiveCollaboration
