// 07 30 2026, 13 26
/* purpose
* Implements the chat history ring buffer.
* Stores up to MAX_MESSAGES (100) entries in a deque.
* Newest entry is at the back. Oldest is at the front.
* Does NOT own packet handling, network state, or chat window rendering.
*/
#include "chat-history.h"
#include <algorithm>

void ChatHistory::append(const ChatHistoryEntry& entry)
{
    if (mEntries.size() >= MAX_MESSAGES)
        mEntries.pop_front();
    mEntries.push_back(entry);
}

void ChatHistory::clear()
{
    mEntries.clear();
}

const ChatHistoryEntry& ChatHistory::get(size_t index) const
{
    return mEntries[index];
}

const ChatHistoryEntry& ChatHistory::newest() const
{
    return mEntries.back();
}

const ChatHistoryEntry& ChatHistory::oldest() const
{
    return mEntries.front();
}

std::vector<ChatHistoryEntry> ChatHistory::getRange(size_t start, size_t count) const
{
    std::vector<ChatHistoryEntry> result;
    size_t end = std::min(start + count, mEntries.size());
    for (size_t i = start; i < end; ++i)
        result.push_back(mEntries[i]);
    return result;
}

size_t ChatHistory::countSince(uint64_t messageId) const
{
    size_t count = 0;
    for (auto it = mEntries.rbegin(); it != mEntries.rend(); ++it)
    {
        if (it->messageId == messageId)
            break;
        ++count;
    }
    return count;
}
