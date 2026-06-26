#pragma once

#include "ui-system.h"

// Single owner of ALL coordinate transforms in the GUI system.
// Design space is always 1920x1080.
// Screen space is the current framebuffer resolution.
// Every GUI subsystem uses this class — nobody computes their own scaling.
class GuiCoordinateSystem {
public:
    static GuiCoordinateSystem& instance();

    static constexpr float DESIGN_W = 1920.0f;
    static constexpr float DESIGN_H = 1080.0f;

    // Call each frame with current framebuffer and window sizes.
    void update(int fbW, int fbH, int winW, int winH);

    // -- Conversions -------------------------------------------------------

    float designToScreenX(float designX) const;
    float designToScreenY(float designY) const;
    UIRect designToScreen(const UIRect& designRect) const;

    float screenToDesignX(float screenX) const;
    float screenToDesignY(float screenY) const;
    UIRect screenToDesign(const UIRect& screenRect) const;

    // Convert cursor from window coordinates to framebuffer (screen) coordinates.
    void cursorWindowToScreen(double winX, double winY, double& outFbX, double& outFbY) const;

    // -- Scroll / Translation Stack ----------------------------------------

    void pushTranslate(float designY);
    void popTranslate();

    // -- Accessors ---------------------------------------------------------

    float screenW() const { return (float)mScreenW; }
    float screenH() const { return (float)mScreenH; }
    float scaleX() const { return mScaleX; }
    float scaleY() const { return mScaleY; }

private:
    GuiCoordinateSystem() = default;

    int mScreenW = (int)DESIGN_W;
    int mScreenH = (int)DESIGN_H;
    int mWindowW = (int)DESIGN_W;
    int mWindowH = (int)DESIGN_H;
    float mScaleX = 1.0f;
    float mScaleY = 1.0f;
    float mTranslateY = 0.0f;
    int mTranslateStack = 0;
};
