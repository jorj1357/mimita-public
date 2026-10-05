# RMB aimbody stability

Added an editable `right_arm_pointing.stability_multiplier` for RMB-only
post-physics right-arm stabilization. The correction uses the physical
weapon/camera aim target, preserves loose movement sway when RMB is released,
and avoids the old `alpha *= (1.0f - pointingBlend)` zeroing while aiming.

Validation: the affected ragdoll source compiled and the executable link was
reached. The build wrapper failed afterward because the C: drive had no free
space while writing its response. Runtime visual acceptance remains open.
