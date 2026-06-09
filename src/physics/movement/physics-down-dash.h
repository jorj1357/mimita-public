#pragma once

class Player;

// Down dash: immediate downward impulse via Q key
// Works both airborne and grounded
// - Airborne Q: fast ~100 m/s slam downward
// - Grounded Q: downward force, physics handles result naturally
void doDownDash(
    Player& p,
    bool downDashPressed,
    float dt
);
