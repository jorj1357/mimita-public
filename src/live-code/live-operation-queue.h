#pragma once

#include "live-operation.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace LiveCollaboration {

class LiveOperationQueue {
public:
    static LiveOperationQueue& instance();

    bool enqueue(LiveOperationV1 operation);
    std::optional<LiveOperationV1> popNext();
    bool updateState(std::uint64_t operationId, LiveOperationState state);
    std::optional<LiveOperationV1> find(std::uint64_t operationId) const;
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::deque<std::uint64_t> order_;
    std::unordered_map<std::uint64_t, LiveOperationV1> operations_;
};

} // namespace LiveCollaboration
