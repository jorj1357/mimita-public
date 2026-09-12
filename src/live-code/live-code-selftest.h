// 09 12 2026
/* purpose
* Declares the headless live-code self-test.
* Verifies the time utility, code hashing, JSONL journal, and the full GameAPI
* load + ABI + deterministic self-test path without opening a window.
* Does NOT own the pipeline state machine or gameplay behavior.
*/
#pragma once

#include <string>

bool runLiveCodeSelfTest(std::string& report);
