---
name: duplicate-state-checker
description: Detect duplicate state variables and competing sources of truth (e.g. grounded vs onGround, canDash vs dashReady).
---

# Duplicate State Checker

## Purpose
Detect duplicate state and competing sources of truth.

## Rules
One concept = one owner.

## Examples of Duplicate State
- `grounded` / `onGround` / `stableOnGround` / `groundedThisFrame`
- `dashAvailable` / `dashReady` / `canDash`
- `jumpAvailable` / `jumpReady` / `canJump`

## Report
For each duplicate found, report:
- variable name
- file
- line
- who writes it
- who reads it

## Severity
Flag **HIGH RISK** when multiple variables represent the same truth.

## Action
Report only. Do not edit code.
