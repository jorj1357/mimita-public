// 07 30 2026, 13 26
/* purpose
* Provides a client-side ring buffer that stores the latest N chat messages.
* Each ChatHistoryEntry carries the server tick, UTC timestamp, sender identity,
* message text, and sender type (Player or Server).
* Used by the 2D chat window for scrolling history and by replay for playback.
* Does NOT own network state, packet handling, or 3D chat bubble rendering.
* Does NOT store typing state, profile data, or per-letter styling.
*/
#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <optional>
#include <vector>
#include "vip/vip-appearance.h"

enum class ChatSenderType : uint8_t
{
    Player,
    Server
};

struct ChatHistoryEntry
{
    uint64_t messageId = 0;
    uint64_t serverTick = 0;
    int64_t utcUnixMilliseconds = 0;
    uint32_t senderEntityId = 0;
    uint32_t senderAccountId = 0;
    ChatSenderType senderType = ChatSenderType::Player;
    uint8_t channel = 0;
    std::string senderName;
    MimitaVip::VipAppearance senderVipAppearance;
    std::string text;
    bool muted = false; // local mute flag, not networked
};

class ChatHistory
{
public:
    static constexpr size_t MAX_MESSAGES = 100;

    void append(const ChatHistoryEntry& entry);
    void clear();

    size_t size() const { return mEntries.size(); }
    bool empty() const { return mEntries.empty(); }
    const ChatHistoryEntry& get(size_t index) const;
    const ChatHistoryEntry& newest() const;
    const ChatHistoryEntry& oldest() const;

    // Returns range [start, end) with newest-first ordering
    std::vector<ChatHistoryEntry> getRange(size_t start, size_t count) const;

    // Return number of entries since the given messageId
    size_t countSince(uint64_t messageId) const;

private:
    std::deque<ChatHistoryEntry> mEntries;
};
