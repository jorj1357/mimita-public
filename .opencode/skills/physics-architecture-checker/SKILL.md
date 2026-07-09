---
name: physics-architecture-checker
description: Enforce physics hierarchy. Verify main.cpp does not contain movement, collision, rendering, or networking logic.
---

# Physics Architecture Checker

## Purpose
Enforce physics hierarchy.

## Desired Structure
```
main
-> doPhysics()

doPhysics
-> doWalk()
-> doJump()
-> doDash()
-> doSlide()
-> doCollision()
```

## Forbidden
- main.cpp contains movement logic
- main.cpp contains collision logic
- main.cpp contains rendering logic
- main.cpp contains terminal command registration
- main.cpp contains networking logic

## Report
Report any violations of the desired structure.

## Action
Report only. Do not edit code.
