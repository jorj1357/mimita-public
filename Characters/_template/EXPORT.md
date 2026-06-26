# Blender Export Guide

## Export Settings (glTF 2.0 Binary)

| Setting | Value |
|---------|-------|
| Format | **glTF Binary (.glb)** |
| Include | **Selected Objects** → OFF |
| Transform | **+Y Up** → OFF (keep Z up) |
| Transform | **Apply Unity Axis** → OFF |
| Mesh | **Export Materials** → ON |
| Mesh | **Images → Copy** → ON |
| Mesh | **Triangulate** → ON |
| Mesh | **Export Morph Targets** → OFF |
| Mesh | **Export Skins** → OFF |
| Mesh | **Export All Vertex Colors** → OFF |
| Mesh | **Compression** → OFF |
| Geometry | **Loose Edges/Points** → OFF |
| Animation | **Export Animations** → OFF |

## Object Requirements

| Name | Type | Required |
|------|------|----------|
| plrOrigin | Empty | YES |
| head | Mesh | YES |
| torso | Mesh | YES |
| leftArm | Mesh | YES |
| rightArm | Mesh | YES |
| leftLeg | Mesh | YES |
| rightLeg | Mesh | YES |

## Hierarchy

```
plrOrigin (root)
  +-- head
  +-- torso
  +-- leftArm
  +-- rightArm
  +-- leftLeg
  +-- rightLeg
```

- Z axis = UP
- Y axis = FORWARD
- Rest pose = T-pose
- 1 unit = 1 game unit
