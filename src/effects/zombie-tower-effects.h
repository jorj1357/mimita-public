#pragma once

// Client-only Zombie Tower visual effects toggle. Gameplay and forced first
// person never read this state.
class ZombieTowerEffects
{
public:
    static ZombieTowerEffects& instance();

    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool enabled() const { return mEnabled; }
    void setFogEnabled(bool enabled) { mFogEnabled = enabled; }
    bool fogEnabled() const { return mFogEnabled; }
    void setRainEnabled(bool enabled) { mRainEnabled = enabled; }
    bool rainEnabled() const { return mRainEnabled; }
    void setTorchEnabled(bool enabled) { mTorchEnabled = enabled; }
    bool torchEnabled() const { return mTorchEnabled; }

private:
    ZombieTowerEffects() = default;
    bool mEnabled = true;
    bool mFogEnabled = false;
    bool mRainEnabled = false;
    bool mTorchEnabled = false;
};

