// 2026-10-27
/* purpose
* Register the generalized constraint terminal surface: create, tune, list,
* delete, and spawn the two-block hinge/spring demo.
* Commands call the shared ConstraintSystem and PhysicalEntitySystem actions.
* Does NOT own constraint math or entity collision.
*/
#pragma once

void registerConstraintCommands();
