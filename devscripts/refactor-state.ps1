param(
    [string]$SrcPath = "src"
)

$files = Get-ChildItem -Path $SrcPath -Recurse -Include "*.cpp","*.h" | Where-Object {
    # Exclude server-only files that use ServerPlayer/ServerNpc (not Player)
    $_.FullName -notmatch '\\server-players\.cpp$' -and
    $_.FullName -notmatch '\\server-packets\.cpp$' -and
    $_.FullName -notmatch '\\server-npcs\.cpp$'
}

# ──── p. prefix (Player references by variable name 'p') ────
$pReplacements = @(
    @('p\.onGround\b',                          'p.ground.onGround'),
    @('p\.stableOnGround\b',                    'p.ground.stableOnGround'),
    @('p\.wasOnGround\b',                       'p.ground.wasOnGround'),
    @('p\.hasWorldContact\b',                   'p.ground.hasWorldContact'),
    @('p\.realWorldContactThisFrame\b',         'p.ground.realWorldContactThisFrame'),
    @('p\.groundLostTimer\b',                   'p.ground.groundLostTimer'),
    @('p\.airborneTimer\b',                     'p.ground.airborneTimer'),
    @('p\.landingCooldown\b',                   'p.ground.landingCooldown'),
    @('p\.worldContactLostTimer\b',             'p.ground.worldContactLostTimer'),
    @('p\.didLand\b',                           'p.ground.didLand'),
    @('p\.airJumpsLeft\b',                      'p.jump.airJumpsLeft'),
    @('p\.jumpHeldPrev\b',                      'p.jump.jumpHeldPrev'),
    @('p\.airJumpLocked\b',                     'p.jump.airJumpLocked'),
    @('p\.airJumpArmed\b',                      'p.jump.airJumpArmed'),
    @('p\.jumpIntentTimer\b',                   'p.jump.jumpIntentTimer'),
    @('p\.coyoteTimer\b',                       'p.jump.coyoteTimer'),
    @('p\.jumpConsumed\b',                      'p.jump.jumpConsumed'),
    @('p\.didGroundJump\b',                     'p.jump.didGroundJump'),
    @('p\.didAirJump\b',                        'p.jump.didAirJump'),
    @('p\.jumpSoundTimer\b',                    'p.jump.jumpSoundTimer'),
    @('p\.dashHeldPrev\b',                      'p.dash.dashHeldPrev'),
    @('p\.moveHeldPrev\b',                      'p.dash.moveHeldPrev'),
    @('p\.dashAvailable\b',                     'p.dash.dashAvailable'),
    @('p\.downDashAvailable\b',                 'p.dash.downDashAvailable'),
    @('p\.dashMovementTicks\b',                 'p.dash.dashMovementTicks'),
    @('p\.lastDashQuality\b',                   'p.dash.lastDashQuality'),
    @('p\.didDash\b',                           'p.dash.didDash'),
    @('p\.freezeAvailable\b',                   'p.freeze.freezeAvailable'),
    @('p\.freezeHeldPrev\b',                    'p.freeze.freezeHeldPrev'),
    @('p\.freezeActive\b',                      'p.freeze.freezeActive'),
    @('p\.freezeTimer\b',                       'p.freeze.freezeTimer'),
    @('p\.freezeHoldSoundPlayed\b',             'p.freeze.freezeHoldSoundPlayed'),
    @('p\.didFreeze\b',                         'p.freeze.didFreeze'),
    @('p\.collisionStuckFrames\b',              'p.collision.stuckFrames'),
    @('p\.collisionBounceCooldown\b',           'p.collision.bounceCooldown'),
    @('p\.hasWeaponCollisionCapsule\b',         'p.collision.hasWeaponCollisionCapsule'),
    @('p\.groundReturnCharges\b',               'p.groundReturn.charges'),
    @('p\.groundReturnRechargeTimer\b',         'p.groundReturn.rechargeTimer'),
    @('p\.groundReturnAvailable\b',             'p.groundReturn.available')
)

# ──── player. prefix ────
$playerReplacements = @(
    @('player\.onGround\b',                          'player.ground.onGround'),
    @('player\.stableOnGround\b',                    'player.ground.stableOnGround'),
    @('player\.wasOnGround\b',                       'player.ground.wasOnGround'),
    @('player\.hasWorldContact\b',                   'player.ground.hasWorldContact'),
    @('player\.realWorldContactThisFrame\b',         'player.ground.realWorldContactThisFrame'),
    @('player\.groundLostTimer\b',                   'player.ground.groundLostTimer'),
    @('player\.airborneTimer\b',                     'player.ground.airborneTimer'),
    @('player\.landingCooldown\b',                   'player.ground.landingCooldown'),
    @('player\.worldContactLostTimer\b',             'player.ground.worldContactLostTimer'),
    @('player\.didLand\b',                           'player.ground.didLand'),
    @('player\.airJumpsLeft\b',                      'player.jump.airJumpsLeft'),
    @('player\.jumpHeldPrev\b',                      'player.jump.jumpHeldPrev'),
    @('player\.airJumpLocked\b',                     'player.jump.airJumpLocked'),
    @('player\.airJumpArmed\b',                      'player.jump.airJumpArmed'),
    @('player\.jumpIntentTimer\b',                   'player.jump.jumpIntentTimer'),
    @('player\.coyoteTimer\b',                       'player.jump.coyoteTimer'),
    @('player\.jumpConsumed\b',                      'player.jump.jumpConsumed'),
    @('player\.didGroundJump\b',                     'player.jump.didGroundJump'),
    @('player\.didAirJump\b',                        'player.jump.didAirJump'),
    @('player\.jumpSoundTimer\b',                    'player.jump.jumpSoundTimer'),
    @('player\.dashHeldPrev\b',                      'player.dash.dashHeldPrev'),
    @('player\.moveHeldPrev\b',                      'player.dash.moveHeldPrev'),
    @('player\.dashAvailable\b',                     'player.dash.dashAvailable'),
    @('player\.downDashAvailable\b',                 'player.dash.downDashAvailable'),
    @('player\.dashMovementTicks\b',                 'player.dash.dashMovementTicks'),
    @('player\.lastDashQuality\b',                   'player.dash.lastDashQuality'),
    @('player\.didDash\b',                           'player.dash.didDash'),
    @('player\.freezeAvailable\b',                   'player.freeze.freezeAvailable'),
    @('player\.freezeHeldPrev\b',                    'player.freeze.freezeHeldPrev'),
    @('player\.freezeActive\b',                      'player.freeze.freezeActive'),
    @('player\.freezeTimer\b',                       'player.freeze.freezeTimer'),
    @('player\.freezeHoldSoundPlayed\b',             'player.freeze.freezeHoldSoundPlayed'),
    @('player\.didFreeze\b',                         'player.freeze.didFreeze'),
    @('player\.collisionStuckFrames\b',              'player.collision.stuckFrames'),
    @('player\.collisionBounceCooldown\b',           'player.collision.bounceCooldown'),
    @('player\.hasWeaponCollisionCapsule\b',         'player.collision.hasWeaponCollisionCapsule'),
    @('player\.groundReturnCharges\b',               'player.groundReturn.charges'),
    @('player\.groundReturnRechargeTimer\b',         'player.groundReturn.rechargeTimer'),
    @('player\.groundReturnAvailable\b',             'player.groundReturn.available')
)

# ──── actor. prefix (Player in death-system.cpp) ────
$actorReplacements = @(
    @('actor\.onGround\b',                          'actor.ground.onGround'),
    @('actor\.didLand\b',                           'actor.ground.didLand'),
    @('actor\.wasOnGround\b',                       'actor.ground.wasOnGround')
)

# ──── rp. prefix (remote player in engine-tick.cpp) ────
$rpReplacements = @(
    @('rp\.onGround\b',                             'rp.ground.onGround')
)

# ──── kv\.second\. prefix (remote player in network-commands.cpp) ────
$kvSecondReplacements = @(
    @('kv\.second\.onGround\b',                     'kv.second.ground.onGround')
)

# ──── -> prefix (Player pointer accesses) ────
$arrowReplacements = @(
    @('->onGround\b',                               '->ground.onGround'),
    @('->stableOnGround\b',                         '->ground.stableOnGround'),
    @('->wasOnGround\b',                            '->ground.wasOnGround')
)

# ──── self. prefix (if any) ────
$selfReplacements = @(
    @('self\.onGround\b',                           'self.ground.onGround')
)

Write-Host "Refactoring state variable references in $($files.Count) files..."

$totalChanges = 0
foreach ($f in $files) {
    $content = Get-Content $f.FullName -Raw
    $original = $content
    $relPath = Resolve-Path -LiteralPath $f.FullName -Relative

    # Apply p. prefix replacements
    foreach ($r in $pReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply player. prefix replacements
    foreach ($r in $playerReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply actor. prefix replacements
    foreach ($r in $actorReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply rp. prefix replacements
    foreach ($r in $rpReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply kv.second. prefix replacements
    foreach ($r in $kvSecondReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply -> prefix replacements
    foreach ($r in $arrowReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    # Apply self. prefix replacements
    foreach ($r in $selfReplacements) {
        $content = $content -replace $r[0], $r[1]
    }

    if ($content -ne $original) {
        $changes = @()
        # Count changes by comparing line by line
        $origLines = $original -split "`r`n|`n"
        $newLines = $content -split "`r`n|`n"
        for ($i = 0; $i -lt [Math]::Min($origLines.Count, $newLines.Count); $i++) {
            if ($origLines[$i] -ne $newLines[$i]) {
                $changes += "  Line $($i+1): $($newLines[$i].Trim())"
            }
        }
        Write-Host "  $relPath - $($changes.Count) changes"
        foreach ($c in $changes) {
            Write-Host $c
        }
        Set-Content $f.FullName -Value $content -NoNewline
        $totalChanges++
    }
}

Write-Host "`nDone. Modified $totalChanges files."
