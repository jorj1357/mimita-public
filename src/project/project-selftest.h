// 09 12 2026
/* purpose
* Headless self-test for the project primitives: content/blobs, tree scan and
* diff (add/modify/delete/rename), history undo/redo/checkpoint/restore,
* .d dependency graph, and state schema migration.
* Does NOT touch the running world.
*/
#pragma once

#include <string>

bool runProjectSelfTest(std::string& report);
