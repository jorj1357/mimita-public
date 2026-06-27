#include "avatar-menu.h"

#include <cstdio>
#include <cstring>

// ─── Popup text input handling ──────────────────────────────────────
void avatarMenuHandleChar(unsigned int codepoint)
{
    if (gSavePopupOpen) {
        size_t len = strlen(gSaveNameBuf);
        if (codepoint == '\b' || codepoint == 127) {
            if (len > 0) gSaveNameBuf[len - 1] = '\0';
        } else if (codepoint >= 32 && codepoint < 127 && len < sizeof(gSaveNameBuf) - 1) {
            gSaveNameBuf[len] = (char)codepoint;
            gSaveNameBuf[len + 1] = '\0';
        }
        return;
    }
    if (gRenamePopupOpen) {
        size_t len = strlen(gRenameBuf);
        if (codepoint == '\b' || codepoint == 127) {
            if (len > 0) gRenameBuf[len - 1] = '\0';
        } else if (codepoint >= 32 && codepoint < 127 && len < sizeof(gRenameBuf) - 1) {
            gRenameBuf[len] = (char)codepoint;
            gRenameBuf[len + 1] = '\0';
        }
        return;
    }
}

void avatarMenuHandleKey(int key, int action)
{
    (void)key;
    (void)action;
}
