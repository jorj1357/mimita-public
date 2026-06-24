# Single Responsibility Checker

## Purpose
Prevent duplicate implementations. Encourage one canonical implementation per concept.

## Rules
One idea = one implementation.

## Examples of violations

### Duplicate Systems
- CollisionSolver + CollisionHelper + CollisionUtils + CollisionMath + CollisionCore — all partially solving collision
- Three movement systems when one canonical one exists
- Three replay exporters when one is sufficient
- Multiple competing ownership/authority systems

### Duplicate State
- `canJump` + `jumpEnabled` + `allowJump` + `jumpAllowed` + `enableJump` — all representing the same idea

### Good
- One `CollisionSolver` containing all collision solving
- One canonical `struct PlayerMovementSettings { bool canJump; }`
- One canonical movement implementation

## Report
For each system, report:
- concept name
- number of implementations
- file locations
- recommended canonical implementation

## Severity
Flag **HIGH RISK** when more than one implementation of the same concept exists.

Flag **MEDIUM** when multiple files claim ownership of the same responsibility.

## Action
Report only. Do not edit code.
