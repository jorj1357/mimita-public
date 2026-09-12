// 09 12 2026
/* purpose
* Implements the project-primitives self-test.
* Does NOT own history or builds; it drives the primitives directly.
*/
#include "project/project-selftest.h"

#include "project/blob-store.h"
#include "project/dependency-graph.h"
#include "project/project-history.h"
#include "project/project-tree.h"
#include "project/state-schema.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool check(bool condition, const char* name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

void writeFile(const std::filesystem::path& path, const std::string& content)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

bool hasOp(const Project::ChangeSet& changes, Project::ChangeOp op, const std::string& path)
{
    for (const auto& change : changes.changes)
        if (change.op == op && change.path == path)
            return true;
    return false;
}

bool migrationAddOne(const void* oldState, std::size_t oldSize, void* newState,
                     std::size_t newSize)
{
    if (!oldState || !newState || oldSize < sizeof(int) || newSize < sizeof(int))
        return false;
    int value = 0;
    std::memcpy(&value, oldState, sizeof(int));
    value += 1;
    std::memcpy(newState, &value, sizeof(int));
    return true;
}

} // namespace

bool runProjectSelfTest(std::string& report)
{
    bool ok = true;
    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) / "mimita_project_selftest";
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "src", error);

    Project::TreeFilter filter;
    filter.includePrefixes = {"src/"};
    filter.includeExtensions = {".cpp", ".h"};

    // ── Blob store dedupe ──────────────────────────────────────────
    {
        Project::BlobStore store(root / ".mimita" / "store");
        const std::string a = "hello";
        const std::string b = "hello";
        Project::ContentId idA = Project::ContentId::fromString(a);
        Project::ContentId idB = Project::ContentId::fromString(b);
        store.putBytes(idA, a.data(), a.size());
        store.putBytes(idB, b.data(), b.size());
        ok &= check(idA.str() == idB.str() && store.blobCount() == 1,
                    "content id dedupe and blob store", report);
    }

    // ── Tree scan + diff add/modify/delete/rename ──────────────────
    {
        writeFile(root / "src" / "a.cpp", "int a() { return 1; }\n");
        writeFile(root / "src" / "b.h", "#pragma once\n");

        Project::ProjectHistory history(root, root / ".mimita", filter);
        history.load();
        Project::ProjectTree first;
        ok &= check(history.scanCurrent(first) && first.entries.size() == 2,
                    "tree scan finds files", report);

        Project::ProjectTree second;
        writeFile(root / "src" / "a.cpp", "int a() { return 2; }\n");
        history.scanCurrent(second);
        Project::ChangeSet changes;
        Project::ProjectTreeScanner::diff(first, second, changes);
        ok &= check(hasOp(changes, Project::ChangeOp::Modify, "src/a.cpp"),
                    "tree diff detects modify", report);

        writeFile(root / "src" / "c.cpp", "int c() { return 3; }\n");
        Project::ProjectTree third;
        history.scanCurrent(third);
        Project::ProjectTreeScanner::diff(second, third, changes);
        ok &= check(hasOp(changes, Project::ChangeOp::Add, "src/c.cpp"),
                    "tree diff detects add", report);

        std::filesystem::remove(root / "src" / "b.h", error);
        Project::ProjectTree fourth;
        history.scanCurrent(fourth);
        Project::ProjectTreeScanner::diff(third, fourth, changes);
        ok &= check(hasOp(changes, Project::ChangeOp::Delete, "src/b.h"),
                    "tree diff detects delete", report);

        std::filesystem::rename(root / "src" / "a.cpp", root / "src" / "a2.cpp", error);
        Project::ProjectTree fifth;
        history.scanCurrent(fifth);
        Project::ProjectTreeScanner::diff(fourth, fifth, changes);
        ok &= check(hasOp(changes, Project::ChangeOp::Rename, "src/a2.cpp"),
                    "tree diff detects rename by content", report);
    }

    // ── History record/undo/redo/checkpoint/restore ────────────────
    {
        std::filesystem::remove_all(root / "src", error);
        std::filesystem::create_directories(root / "src", error);
        std::filesystem::remove_all(root / ".mimita", error);
        writeFile(root / "src" / "v.cpp", "int v() { return 1; }\n");

        Project::ProjectHistory history(root, root / ".mimita", filter);
        history.load();
        Project::ChangeSet changes;
        ok &= check(history.record(changes), "history records first version", report);
        const std::string v1 = history.current()->versionHash;

        writeFile(root / "src" / "v.cpp", "int v() { return 2; }\n");
        history.record(changes);
        const std::string v2 = history.current()->versionHash;
        ok &= check(v1 != v2 && history.canUndo(), "history records second version", report);

        ok &= check(history.undo(), "history undo", report);
        ok &= check(history.current()->versionHash == v1 && history.canRedo(),
                    "history undo moved to parent", report);
        ok &= check(history.redo() && history.current()->versionHash == v2,
                    "history redo returns to child", report);

        ok &= check(history.checkpoint("checkpoint-2"), "history checkpoint", report);
        ok &= check(history.restore(v1), "history restore", report);
        ok &= check(history.current()->versionHash == v1,
                    "history restore moved to target", report);
    }

    // ── .d dependency graph ────────────────────────────────────────
    {
        const std::filesystem::path dep = root / "src" / "unit.d";
        writeFile(dep, "build/obj/unit.o: src/unit.cpp src/unit.h src/shared.h \\\n");
        Project::DependencyGraph graph;
        ok &= check(graph.addCompilerDepFile(dep), "dependency graph parses .d", report);
        const std::set<std::string> affected =
            graph.affected({"src/shared.h"});
        ok &= check(affected.count("src/unit.cpp") == 1,
                    "dependency graph affected set", report);
    }

    // ── State schema registry + migration ──────────────────────────
    {
        Project::StateSchemaRegistry& registry = Project::StateSchemaRegistry::instance();
        Project::StateSchema v1;
        v1.typeId = 7;
        v1.version = 1;
        v1.name = "TestState";
        v1.canonical = "int value;";
        registry.registerSchema(v1);
        Project::StateSchema v2 = v1;
        v2.version = 2;
        v2.canonical = "int value; int extra;";
        registry.registerSchema(v2);
        registry.registerMigration(7, 1, 2, &migrationAddOne);

        int oldValue = 41;
        int newValue = 0;
        const bool migrated = registry.migrate(7, 1, 2, &oldValue, sizeof(oldValue),
                                               &newValue, sizeof(newValue));
        ok &= check(migrated && newValue == 42,
                    "state schema migration runs", report);
        ok &= check(registry.find(7, 2) != nullptr && registry.schemaCount() >= 2,
                    "state schema registry lookup", report);
    }

    std::filesystem::remove_all(root, error);
    return ok;
}
