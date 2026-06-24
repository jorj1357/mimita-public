"""Exact string replacement for Player state refactoring.
Uses str.replace() not regex to avoid corruption.
Only targets files known to reference Player class members.
"""

import os
import glob

SRC = "src"

# Map of old -> new exact string replacements
REPLACEMENTS = {
    # p. prefix
    "p.onGround": "p.ground.onGround",
    "p.stableOnGround": "p.ground.stableOnGround",
    "p.wasOnGround": "p.ground.wasOnGround",
    "p.hasWorldContact": "p.ground.hasWorldContact",
    "p.realWorldContactThisFrame": "p.ground.realWorldContactThisFrame",
    "p.groundLostTimer": "p.ground.groundLostTimer",
    "p.airborneTimer": "p.ground.airborneTimer",
    "p.landingCooldown": "p.ground.landingCooldown",
    "p.worldContactLostTimer": "p.ground.worldContactLostTimer",
    "p.didLand": "p.ground.didLand",
    "p.airJumpsLeft": "p.jump.airJumpsLeft",
    "p.jumpHeldPrev": "p.jump.jumpHeldPrev",
    "p.airJumpLocked": "p.jump.airJumpLocked",
    "p.airJumpArmed": "p.jump.airJumpArmed",
    "p.jumpIntentTimer": "p.jump.jumpIntentTimer",
    "p.coyoteTimer": "p.jump.coyoteTimer",
    "p.jumpConsumed": "p.jump.jumpConsumed",
    "p.didGroundJump": "p.jump.didGroundJump",
    "p.didAirJump": "p.jump.didAirJump",
    "p.jumpSoundTimer": "p.jump.jumpSoundTimer",
    "p.dashHeldPrev": "p.dash.dashHeldPrev",
    "p.moveHeldPrev": "p.dash.moveHeldPrev",
    "p.dashAvailable": "p.dash.dashAvailable",
    "p.downDashAvailable": "p.dash.downDashAvailable",
    "p.dashMovementTicks": "p.dash.dashMovementTicks",
    "p.lastDashQuality": "p.dash.lastDashQuality",
    "p.didDash": "p.dash.didDash",
    "p.freezeAvailable": "p.freeze.freezeAvailable",
    "p.freezeHeldPrev": "p.freeze.freezeHeldPrev",
    "p.freezeActive": "p.freeze.freezeActive",
    "p.freezeTimer": "p.freeze.freezeTimer",
    "p.freezeHoldSoundPlayed": "p.freeze.freezeHoldSoundPlayed",
    "p.didFreeze": "p.freeze.didFreeze",
    "p.collisionStuckFrames": "p.collision.stuckFrames",
    "p.collisionBounceCooldown": "p.collision.bounceCooldown",
    "p.hasWeaponCollisionCapsule": "p.collision.hasWeaponCollisionCapsule",
    "p.groundReturnCharges": "p.groundReturn.charges",
    "p.groundReturnRechargeTimer": "p.groundReturn.rechargeTimer",
    "p.groundReturnAvailable": "p.groundReturn.available",
    # player. prefix
    "player.onGround": "player.ground.onGround",
    "player.stableOnGround": "player.ground.stableOnGround",
    "player.wasOnGround": "player.ground.wasOnGround",
    "player.hasWorldContact": "player.ground.hasWorldContact",
    "player.realWorldContactThisFrame": "player.ground.realWorldContactThisFrame",
    "player.groundLostTimer": "player.ground.groundLostTimer",
    "player.airborneTimer": "player.ground.airborneTimer",
    "player.landingCooldown": "player.ground.landingCooldown",
    "player.worldContactLostTimer": "player.ground.worldContactLostTimer",
    "player.didLand": "player.ground.didLand",
    "player.airJumpsLeft": "player.jump.airJumpsLeft",
    "player.jumpHeldPrev": "player.jump.jumpHeldPrev",
    "player.airJumpLocked": "player.jump.airJumpLocked",
    "player.airJumpArmed": "player.jump.airJumpArmed",
    "player.jumpIntentTimer": "player.jump.jumpIntentTimer",
    "player.coyoteTimer": "player.jump.coyoteTimer",
    "player.jumpConsumed": "player.jump.jumpConsumed",
    "player.didGroundJump": "player.jump.didGroundJump",
    "player.didAirJump": "player.jump.didAirJump",
    "player.jumpSoundTimer": "player.jump.jumpSoundTimer",
    "player.dashHeldPrev": "player.dash.dashHeldPrev",
    "player.moveHeldPrev": "player.dash.moveHeldPrev",
    "player.dashAvailable": "player.dash.dashAvailable",
    "player.downDashAvailable": "player.dash.downDashAvailable",
    "player.dashMovementTicks": "player.dash.dashMovementTicks",
    "player.lastDashQuality": "player.dash.lastDashQuality",
    "player.didDash": "player.dash.didDash",
    "player.freezeAvailable": "player.freeze.freezeAvailable",
    "player.freezeHeldPrev": "player.freeze.freezeHeldPrev",
    "player.freezeActive": "player.freeze.freezeActive",
    "player.freezeTimer": "player.freeze.freezeTimer",
    "player.freezeHoldSoundPlayed": "player.freeze.freezeHoldSoundPlayed",
    "player.didFreeze": "player.freeze.didFreeze",
    "player.collisionStuckFrames": "player.collision.stuckFrames",
    "player.collisionBounceCooldown": "player.collision.bounceCooldown",
    "player.hasWeaponCollisionCapsule": "player.collision.hasWeaponCollisionCapsule",
    "player.groundReturnCharges": "player.groundReturn.charges",
    "player.groundReturnRechargeTimer": "player.groundReturn.rechargeTimer",
    "player.groundReturnAvailable": "player.groundReturn.available",
    # actor. prefix (death-system.cpp - actor is Player)
    "actor.onGround": "actor.ground.onGround",
    "actor.didLand": "actor.ground.didLand",
    "actor.wasOnGround": "actor.ground.wasOnGround",
    # rp. prefix (engine-tick.cpp - remote player)
    "rp.onGround": "rp.ground.onGround",
    # kv.second. prefix (network-commands.cpp)
    "kv.second.onGround": "kv.second.ground.onGround",
    # -> prefix (Player pointer)
    "->onGround": "->ground.onGround",
    "->stableOnGround": "->ground.stableOnGround",
    "->wasOnGround": "->ground.wasOnGround",
    # npc.body. prefix (npc.body is a Player)
    "npc.body.onGround": "npc.body.ground.onGround",
    "npc.body.stableOnGround": "npc.body.ground.stableOnGround",
    "npc.body.downDashAvailable": "npc.body.dash.downDashAvailable",
    "npc.body.didDash": "npc.body.dash.didDash",
}

# Files to EXCLUDE (these use ServerPlayer/ServerNpc, not Player)
EXCLUDE = [
    "server-players.cpp",
    "server-packets.cpp",
    "server-npcs.cpp",
]

total_files = 0
total_replacements = 0

for root, dirs, files in os.walk(SRC):
    for fname in files:
        if not (fname.endswith(".cpp") or fname.endswith(".h")):
            continue
        if fname in EXCLUDE:
            continue
        fpath = os.path.join(root, fname)
        # Skip player.h itself (already done)
        if fpath.endswith("player.h"):
            continue
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            print(f"SKIP {fpath}: {e}")
            continue

        original = content
        file_changes = 0
        for old, new in REPLACEMENTS.items():
            count = content.count(old)
            if count > 0:
                content = content.replace(old, new)
                file_changes += count
                total_replacements += count

        if content != original:
            with open(fpath, "w", encoding="utf-8") as f:
                f.write(content)
            total_files += 1
            # Show a few sample changes
            print(f"  {fpath} ({file_changes} changes)")

print(f"\nDone. Modified {total_files} files with {total_replacements} total replacements.")
