#include "live-operation-queue.h"

#include "debug/structured-log.h"

#include <algorithm>

namespace LiveCollaboration {

namespace {

void logOperation(const LiveOperationV1& op, const char* event)
{
    debug::Event e;
    e.category = "LIVE_COLLABORATION";
    e.name = event;
    e.level = debug::Level::Info;
    e.fields = {
        {"operation_id", op.operationId},
        {"resource_id", op.resourceId},
        {"revision_id", op.revisionId},
        {"operation_type", operationTypeName(op.type)},
        {"operation_state", operationStateName(op.state)},
        {"priority", op.priority},
        {"queue_source", "live-operation-queue"}
    };
    debug::logEvent(e);
}

} // namespace

const char* operationStateName(LiveOperationState state)
{
    switch (state) {
    case LiveOperationState::Received: return "received";
    case LiveOperationState::Validated: return "validated";
    case LiveOperationState::Queued: return "queued";
    case LiveOperationState::Started: return "started";
    case LiveOperationState::Completed: return "completed";
    case LiveOperationState::Rejected: return "rejected";
    case LiveOperationState::Superseded: return "superseded";
    case LiveOperationState::Cancelled: return "cancelled";
    }
    return "unknown";
}

const char* operationTypeName(LiveOperationType type)
{
    switch (type) {
    case LiveOperationType::AddCode: return "add_code";
    case LiveOperationType::UpdateCode: return "update_code";
    case LiveOperationType::RemoveCode: return "remove_code";
    case LiveOperationType::SetProperty: return "set_property";
    case LiveOperationType::AddAsset: return "add_asset";
    case LiveOperationType::RemoveAsset: return "remove_asset";
    case LiveOperationType::ActivateRevision: return "activate_revision";
    case LiveOperationType::RollbackRevision: return "rollback_revision";
    }
    return "unknown";
}

LiveOperationQueue& LiveOperationQueue::instance()
{
    static LiveOperationQueue queue;
    return queue;
}

bool LiveOperationQueue::enqueue(LiveOperationV1 operation)
{
    if (operation.operationId == 0)
        return false;
    std::lock_guard lock(mutex_);
    if (operations_.find(operation.operationId) != operations_.end())
        return false;
    operation.state = LiveOperationState::Queued;
    operations_.emplace(operation.operationId, operation);
    order_.push_back(operation.operationId);
    logOperation(operation, "operation_queued");
    return true;
}

std::optional<LiveOperationV1> LiveOperationQueue::popNext()
{
    std::lock_guard lock(mutex_);
    if (order_.empty())
        return std::nullopt;

    auto best = std::max_element(order_.begin(), order_.end(),
        [&](std::uint64_t a, std::uint64_t b) {
            return operations_.at(a).priority < operations_.at(b).priority;
        });
    const std::uint64_t id = *best;
    order_.erase(best);
    auto it = operations_.find(id);
    if (it == operations_.end())
        return std::nullopt;
    it->second.state = LiveOperationState::Started;
    logOperation(it->second, "operation_started");
    return it->second;
}

bool LiveOperationQueue::updateState(std::uint64_t operationId,
                                     LiveOperationState state)
{
    std::lock_guard lock(mutex_);
    auto it = operations_.find(operationId);
    if (it == operations_.end())
        return false;
    it->second.state = state;
    logOperation(it->second,
        state == LiveOperationState::Completed ? "operation_completed" :
        state == LiveOperationState::Rejected ? "operation_rejected" :
        "operation_state_changed");
    return true;
}

std::optional<LiveOperationV1> LiveOperationQueue::find(std::uint64_t id) const
{
    std::lock_guard lock(mutex_);
    auto it = operations_.find(id);
    return it == operations_.end() ? std::nullopt
                                   : std::optional<LiveOperationV1>(it->second);
}

std::size_t LiveOperationQueue::size() const
{
    std::lock_guard lock(mutex_);
    return order_.size();
}

} // namespace LiveCollaboration
