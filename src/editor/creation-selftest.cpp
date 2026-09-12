// 09 12 2026
/* purpose
* Implements the creation/fork self-test. Verifies the non-destructive fork
* hash is deterministic, copy yields new ids, and edit ops accumulate.
* Does NOT touch the running world.
*/
#include "editor/creation-selftest.h"

#include "editor/creation-mode.h"

#include <string>
#include <vector>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

} // namespace

bool runCreationSelfTest(std::string& report)
{
    bool ok = true;

    std::vector<Editor::PatchOp> ops;
    Editor::PatchOp transform;
    transform.kind = Editor::PatchOp::Kind::Transform;
    transform.sourceEntity = 10;
    transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    ops.push_back(transform);

    const std::string base = "sha256:aaaa";
    const std::string first = Editor::CreationMode::forkHashFor(base, ops);
    const std::string second = Editor::CreationMode::forkHashFor(base, ops);
    ok &= check(first == second && !first.empty(), "fork hash deterministic", report);

    Editor::PatchOp deleteOp;
    deleteOp.kind = Editor::PatchOp::Kind::Delete;
    deleteOp.sourceEntity = 10;
    ops.push_back(deleteOp);
    const std::string afterDelete = Editor::CreationMode::forkHashFor(base, ops);
    ok &= check(afterDelete != first, "fork hash changes with a new op", report);

    Editor::CreationMode& mode = Editor::CreationMode::instance();
    mode.clear();

    Editor::WorldObjectRef ref;
    ref.entity = 100;
    ref.position = glm::vec3(4.0f, 0.0f, 0.0f);
    const std::uint64_t copy = mode.duplicate(ref, glm::vec3(0.0f, 0.0f, 5.0f));
    ok &= check(copy != 0 && copy != ref.entity && mode.patch().size() == 1,
                "duplicate creates a new entity and one patch op", report);

    ok &= check(mode.transform(copy, glm::vec3(9.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f), glm::vec3(2.0f)),
                "transform records a patch op", report);
    ok &= check(mode.remove(ref.entity), "delete records a patch op", report);
    ok &= check(mode.patch().size() == 3, "patch accumulates operations", report);

    const std::string fork = mode.forkHash();
    ok &= check(!fork.empty() && fork != mode.baseMapHash(),
                "fork hash differs from base map hash", report);

    mode.clear();
    return ok;
}
