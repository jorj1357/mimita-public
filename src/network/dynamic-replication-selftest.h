// 09 14 2026
/* purpose
* Headless self-test for generic dynamic-component replication: envelope
* encode/decode, schema descriptors, upsert/update/remove, payload-size
* rejection, and schema migration on the client apply path.
* Does NOT own simulation, gameplay policy, or the dynamic store.
*/
#pragma once

#include <string>

bool runDynamicReplicationSelfTest(std::string& report);
