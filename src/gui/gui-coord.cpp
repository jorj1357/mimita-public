#include "gui-coord.h"
#include "ui-system.h"

GuiCoordinateSystem& GuiCoordinateSystem::instance()
{
    static GuiCoordinateSystem sys;
    return sys;
}

void GuiCoordinateSystem::update(int fbW, int fbH, int winW, int winH)
{
    mScreenW = fbW > 0 ? fbW : (int)DESIGN_W;
    mScreenH = fbH > 0 ? fbH : (int)DESIGN_H;
    mWindowW = winW > 0 ? winW : mScreenW;
    mWindowH = winH > 0 ? winH : mScreenH;
    mScaleX = (float)mScreenW / DESIGN_W;
    mScaleY = (float)mScreenH / DESIGN_H;
}

void GuiCoordinateSystem::pushTranslate(float designY)
{
    mTranslateY += designY * mScaleY;
    mTranslateStack++;
}

void GuiCoordinateSystem::popTranslate()
{
    if (mTranslateStack > 0)
    {
        mTranslateY = 0.0f;
        mTranslateStack = 0;
    }
}

float GuiCoordinateSystem::designToScreenX(float designX) const
{
    return designX * mScaleX;
}

float GuiCoordinateSystem::designToScreenY(float designY) const
{
    return designY * mScaleY - mTranslateY;
}

UIRect GuiCoordinateSystem::designToScreen(const UIRect& designRect) const
{
    return {
        designRect.x * mScaleX,
        designRect.y * mScaleY,
        designRect.w * mScaleX,
        designRect.h * mScaleY
    };
}

float GuiCoordinateSystem::screenToDesignX(float screenX) const
{
    return mScaleX > 0.0f ? screenX / mScaleX : screenX;
}

float GuiCoordinateSystem::screenToDesignY(float screenY) const
{
    return mScaleY > 0.0f ? screenY / mScaleY : screenY;
}

UIRect GuiCoordinateSystem::screenToDesign(const UIRect& screenRect) const
{
    float invSx = mScaleX > 0.0f ? 1.0f / mScaleX : 1.0f;
    float invSy = mScaleY > 0.0f ? 1.0f / mScaleY : 1.0f;
    return {
        screenRect.x * invSx,
        screenRect.y * invSy,
        screenRect.w * invSx,
        screenRect.h * invSy
    };
}

void GuiCoordinateSystem::cursorWindowToScreen(double winX, double winY, double& outFbX, double& outFbY) const
{
    outFbX = mWindowW > 0 ? winX * (double)mScreenW / (double)mWindowW : winX;
    outFbY = mWindowH > 0 ? winY * (double)mScreenH / (double)mWindowH : winY;
}
