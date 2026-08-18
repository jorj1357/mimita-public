// 08 17 2026
/* purpose
* Creates and manages the offscreen framebuffer for replay export capture.
* A color texture + depth renderbuffer at capture resolution, so frames can be
* filmed independently of the visible game window.
* Does NOT own export job state, encode backends, or the main render loop.
*/
#include "replay/replay-export-target.h"

#include <glad/glad.h>

static ReplayExportTarget gTarget;

bool replayExportTargetInit(int w, int h)
{
    replayExportTargetDestroy();
    if (w <= 0 || h <= 0)
        return false;

    gTarget.width = w;
    gTarget.height = h;

    glGenFramebuffers(1, &gTarget.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gTarget.fbo);

    glGenTextures(1, &gTarget.colorTex);
    glBindTexture(GL_TEXTURE_2D, gTarget.colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gTarget.colorTex, 0);

    glGenRenderbuffers(1, &gTarget.depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, gTarget.depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, gTarget.depthRbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        replayExportTargetDestroy();
        return false;
    }
    return true;
}

void replayExportTargetDestroy()
{
    if (gTarget.fbo)
        glDeleteFramebuffers(1, &gTarget.fbo);
    if (gTarget.colorTex)
        glDeleteTextures(1, &gTarget.colorTex);
    if (gTarget.depthRbo)
        glDeleteRenderbuffers(1, &gTarget.depthRbo);
    gTarget = ReplayExportTarget{};
}

ReplayExportTarget& replayExportTarget()
{
    return gTarget;
}
