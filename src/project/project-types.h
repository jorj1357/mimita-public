// 09 12 2026
/* purpose
* Define the versioned-project primitives: content identity, change sets,
* project versions, and state schemas.
* These are the data vocabulary for live project mutation and history; the
* runtime generation is derived from a ProjectVersion.
* Does NOT scan the filesystem, build packages, or load code.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Project {

// Algorithm-agile content identity. Identity is the pair, never one digest.
struct ContentId {
    std::string algorithm;  // e.g. "sha256"
    std::string digest;     // lowercase hex

    bool valid() const { return !algorithm.empty() && !digest.empty(); }
    std::string str() const { return algorithm + ":" + digest; }

    static ContentId fromBytes(const void* data, std::size_t size);
    static ContentId fromString(const std::string& value);
    static ContentId fromFile(const std::string& path);
};

enum class ChangeOp : std::uint8_t {
    Add = 0,
    Delete = 1,
    Modify = 2,
    Rename = 3,
    Move = 4,
};

inline const char* changeOpName(ChangeOp op)
{
    switch (op) {
    case ChangeOp::Add: return "add";
    case ChangeOp::Delete: return "delete";
    case ChangeOp::Modify: return "modify";
    case ChangeOp::Rename: return "rename";
    case ChangeOp::Move: return "move";
    }
    return "unknown";
}

// One path-level mutation. `oldPath` is set for rename/move.
struct ChangeEntry {
    ChangeOp op = ChangeOp::Modify;
    std::string path;
    std::string oldPath;
    ContentId before;
    ContentId after;
};

// A unit of project mutation. Source history and runtime intent live together.
struct ChangeSet {
    std::string parentTreeHash;
    std::string newTreeHash;
    std::vector<ChangeEntry> changes;
    // Runtime intent (may be filled by the caller/future phases).
    std::vector<std::string> touchedModules;
    bool schemaChange = false;
    std::string changeId;

    bool empty() const { return changes.empty(); }
};

// An immutable project version in the history chain.
struct ProjectVersion {
    std::string versionHash;   // identity of this version (content-addressed)
    std::string parentHash;
    std::string treeHash;
    std::string changeSetHash;
    std::string changeId;
    std::string label;         // optional checkpoint name
    std::uint64_t timestampMs = 0;
};

// A declared state schema for migration. Hash is the canonical serialization.
struct StateSchema {
    std::uint32_t typeId = 0;
    std::uint32_t version = 0;
    std::string name;
    std::string canonical;
    std::string hash;
};

} // namespace Project
