// 09 14 2026
/* purpose
* Implements the generic generation-aware presentation resource provider.
* Does NOT own GPU calls, file IO, or rendering policy; loaders do.
*/
#include "project/presentation-resource.h"

namespace MimitaRuntime {

PresentationResourceProvider& PresentationResourceProvider::instance()
{
    static PresentationResourceProvider provider;
    return provider;
}

void PresentationResourceProvider::clear()
{
    for (auto& entry : entries_) {
        if (entry.second.state.valid && entry.second.state.handle &&
            entry.second.retire)
            entry.second.retire(entry.second.user, entry.second.state.handle);
    }
    entries_.clear();
}

void PresentationResourceProvider::setLoader(std::uint64_t logicalId,
                                             ResourceLoadFn load,
                                             ResourceRetireFn retire, void* user)
{
    if (logicalId == 0)
        return;
    Entry& entry = entries_[logicalId];
    entry.load = load;
    entry.retire = retire;
    entry.user = user;
    entry.state.logicalId = logicalId;
}

void PresentationResourceProvider::adopt(std::uint64_t logicalId,
                                         std::uint64_t contentHash, void* handle)
{
    if (logicalId == 0)
        return;
    Entry& entry = entries_[logicalId];
    entry.state.logicalId = logicalId;
    entry.state.contentHash = contentHash;
    entry.state.handle = handle;
    entry.state.valid = handle != nullptr;
    if (entry.state.generation == 0 && handle)
        entry.state.generation = 1;
}

bool PresentationResourceProvider::apply(std::uint64_t logicalId,
                                         std::uint64_t contentHash,
                                         std::string* error)
{
    auto it = entries_.find(logicalId);
    if (it == entries_.end())
        return false;
    Entry& entry = it->second;

    // Same content: no reload, no generation change.
    if (entry.state.valid && entry.state.contentHash == contentHash)
        return true;

    if (!entry.load) {
        entry.state.lastError = "no loader registered";
        if (error)
            *error = entry.state.lastError;
        return false;
    }

    void* candidate = nullptr;
    if (!entry.load(entry.user, &candidate) || !candidate) {
        // Last-good is preserved: generation, hash, and handle stay unchanged.
        entry.state.lastError = "resource load failed";
        ++entry.state.failureCount;
        if (error)
            *error = entry.state.lastError;
        return false;
    }

    void* previous = entry.state.handle;
    const bool hadPrevious = entry.state.valid && previous != nullptr;
    entry.state.handle = candidate;
    entry.state.contentHash = contentHash;
    ++entry.state.generation;
    entry.state.valid = true;
    entry.state.lastError.clear();

    // The swap is the safe boundary: retire the old generation immediately.
    if (hadPrevious && entry.retire)
        entry.retire(entry.user, previous);
    return true;
}

const ResourceGeneration* PresentationResourceProvider::current(
    std::uint64_t logicalId) const
{
    auto it = entries_.find(logicalId);
    return it == entries_.end() ? nullptr : &it->second.state;
}

void* PresentationResourceProvider::handleOf(std::uint64_t logicalId) const
{
    const ResourceGeneration* state = current(logicalId);
    return state && state->valid ? state->handle : nullptr;
}

std::uint32_t PresentationResourceProvider::generationOf(
    std::uint64_t logicalId) const
{
    const ResourceGeneration* state = current(logicalId);
    return state ? state->generation : 0;
}

} // namespace MimitaRuntime
