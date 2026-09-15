// 09 15 2026
/* purpose
* Declares the headless real-transport generation self-test: serializes the
* artifact/code-generation packets and sends them over an actual loopback UDP
* socket, receiving/decoding them and feeding the real ArtifactStreamer /
* ArtifactReceiver. Proves bytes traverse the transport, not direct calls.
* Does NOT own the game server loop, activation, or the loader.
*/
#pragma once

#include <string>

bool runTransportGenerationSelfTest(std::string& report);
