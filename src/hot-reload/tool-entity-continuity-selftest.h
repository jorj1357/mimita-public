// 09 15 2026
/* purpose
* Declares the headless real-ECS-Tool-Entity resource-continuity self-test: a
* tool entity created through the generic capability path keeps the same
// EntityId, actor ownership/equip relationships, logical mesh id, and gameplay
// state while the logical resource version changes A -> B (and B -> D after a
// code generation change) through the canonical ContentArtifact ->
// PresentationResourceProvider path observed on the real production render path.
*/
#pragma once

#include <string>

bool runToolEntityContinuitySelfTest(std::string& report);
