// 09 23 2026
/* purpose
* Declares the headless packet-codec self-test.
* Does NOT own sockets or transport; it exercises the codec dispatch mechanism.
*/
#pragma once

#include <string>

bool runPacketCodecSelfTest(std::string& report);
