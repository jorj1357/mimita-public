// 09 22 2026
/* purpose
* Self-test for the shared authoritative hitscan consequence pipeline: a trace
* aggregate must apply damage to the victim and drive the shot/confirmed-event
* pipeline. Proves the bridge the hot behavior now depends on.
* Does NOT own weapons or networking.
*/
#pragma once

#include <string>

bool runHitscanOutcomeSelfTest(std::string& report);
