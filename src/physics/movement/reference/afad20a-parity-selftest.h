// 09 22 2026
/* purpose
* Declares the afad20a oracle-vs-hot movement parity harness and the fixture
* generator. Does NOT own movement policy.
*/
#pragma once

#include <string>

namespace MimitaAfad20a {

// Compares the live hot movement kernel against the frozen afad20a oracle for
// collision-free movement math (fall, air-strafe, dash, down-dash, freeze).
bool runAfad20aParitySelfTest(std::string& report);

// Writes deterministic afad20a oracle traces to tests/fixtures/movement/afad20a.
bool generateAfad20aFixtures(std::string& report);

} // namespace MimitaAfad20a
