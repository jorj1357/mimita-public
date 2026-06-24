#include "network/multiplayer-context.h"
#include "network/packets.h"
#include "gui/hud/chat-bubble.h"

#include <cstdio>

namespace MimitaNet {

void mpProcessChatPacket(MultiplayerContext& ctx, const ChatPacket* chat)
{
    MultiplayerContext::IncomingChatMessage msg;
    msg.senderName = chat->senderName;
    msg.text = chat->text;
    ctx.incomingChatMessages.push_back(msg);
    printf("[NET CHAT RECV] %s: %s\n", chat->senderName, chat->text);
}

} // namespace MimitaNet
