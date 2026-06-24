// C:\important\quiet\n\mimita-priv-v7\src\physics\movement\physics-jump.cpp
// feb 10 2026
/**
 * purpose
 * handle all logic for jumps
 * if plr press jump, jump
 * should expose jump(args) to other files
 * this also handles doublejump
 * so like doublejump(args) as well
 * also handles bools and stuff
 * for audio to not be spamming when we hold jump down
 * maibe that just goes in the audio file itself? u can hold jump all u want, 
 * but cant plau the audio whenevr u want?
 * idk.
 * i think its better to put a cap on hwo man times u can jump like u cant just 
 * jump forever, infinitelu,
 * put liek a 0.2s timer between jumps
 * but u can hold jump fro as long as u can
 * maibe even small, like 0.01s 
 */

#pragma once

class Player;

// Handles jump + double jump
// - Uses physics/config.h
// - Debug heavy
// - No collision logic
// - No audio
// - Caller controls p.ground.onGround before calling
void doJump(
    Player& p,
    bool jumpHeld,
    bool jumpPressed,
    float dt
);
