// 08 17 2026
/* purpose
* Creates and manages the offscreen framebuffer for replay export capture.
* A color texture + depth renderbuffer at capture resolution, so frames can be
* filmed independently of the visible game window.
* Does NOT own export job state, encode backends, or the main render loop.
*/
#include "replay/replay-export-target.h"
#include "game/gl-resource-bridge-module.h"
#include "render/render-command.h"

namespace r = mimita::game::render;

static ReplayExportTarget gTarget;

bool replayExportTargetInit(int w, int h)
{
    replayExportTargetDestroy();
    if (w <= 0 || h <= 0)
        return false;

    gTarget.width = w;
    gTarget.height = h;

    gTarget.fbo = mimita::game::genFramebuffer();
    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, gTarget.fbo);

    gTarget.colorTex = mimita::game::genTexture();
    r::bindTexture(r::TextureTarget::Texture2D, gTarget.colorTex);
    r::texImage2D(r::TextureTarget::Texture2D, 0, r::TextureFormat::RGBA8,
                  w, h, 0, r::PixelFormat::RGBA, r::PixelType::UnsignedByte, nullptr);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MinFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::MagFilter,
                     r::TextureValue::Linear);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapS,
                     r::TextureValue::ClampToEdge);
    r::texParameteri(r::TextureTarget::Texture2D, r::TextureParameter::WrapT,
                     r::TextureValue::ClampToEdge);
    r::framebufferTexture2D(r::TextureTarget::Texture2D,
                            r::FramebufferAttachment::Color0, gTarget.colorTex);

    gTarget.depthRbo = mimita::game::genRenderbuffer();
    r::bindRenderbuffer(gTarget.depthRbo);
    r::renderbufferStorage(r::RenderbufferFormat::Depth24, w, h);
    r::framebufferRenderbuffer(r::FramebufferAttachment::Depth, gTarget.depthRbo);

    const bool complete = r::checkFramebufferStatus();
    r::bindFramebuffer(r::FramebufferTarget::Framebuffer, 0);
    r::bindTexture(r::TextureTarget::Texture2D, 0);
    r::bindRenderbuffer(0);

    if (!complete) {
        replayExportTargetDestroy();
        return false;
    }
    return true;
}

void replayExportTargetDestroy()
{
    if (gTarget.fbo)
        mimita::game::deleteFramebuffer(gTarget.fbo);
    if (gTarget.colorTex)
        mimita::game::deleteTexture(gTarget.colorTex);
    if (gTarget.depthRbo)
        mimita::game::deleteRenderbuffer(gTarget.depthRbo);
    gTarget = ReplayExportTarget{};
}

ReplayExportTarget& replayExportTarget()
{
    return gTarget;
}
