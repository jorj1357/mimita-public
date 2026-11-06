// 2026-11-05
#include "worldproject/world-edit-session.h"

#include <string>
#include <vector>

namespace MimitaWorldProject {
namespace {

void appendResult(std::string& report, bool ok, const std::string& name,
                  const std::string& detail = {})
{
    report += std::string(ok ? "PASS " : "FAIL ") + name;
    if (!detail.empty()) report += " (" + detail + ")";
    report += "\n";
}

WorldEditOperation makeOp(uint64_t id, uint32_t baseRevision,
                          WorldEditCapability capability,
                          WorldEditOpKind kind,
                          WorldEditTargetKind target,
                          const std::string& objectId,
                          uint16_t propertyId,
                          WorldEditValue value,
                          uint32_t undoSequence = 0)
{
    WorldEditOperation op;
    op.opId = id;
    op.baseRevision = baseRevision;
    op.capability = capability;
    op.kind = kind;
    op.target = target;
    op.objectId = objectId;
    op.propertyId = propertyId;
    op.value = std::move(value);
    op.undoSequence = undoSequence;
    return op;
}

bool findSnapshotValue(const WorldEditSnapshot& snapshot,
                       WorldEditTargetKind target,
                       const std::string& objectId,
                       uint16_t propertyId,
                       WorldEditValue& out)
{
    for (const auto& entry : snapshot.properties) {
        if (entry.target == target && entry.objectId == objectId &&
            entry.propertyId == propertyId) {
            out = entry.value;
            return true;
        }
    }
    return false;
}

} // namespace

bool worldEditSessionSelfTest(std::string& report)
{
    report.clear();
    bool ok = true;
    auto check = [&](bool condition, const std::string& name,
                     const std::string& detail = {}) {
        appendResult(report, condition, name, detail);
        ok = ok && condition;
    };

    WorldEditSession session;
    session.begin("project", "version-1", "owner");
    session.grantCapabilities("editor",
        WorldEditCapability::Geometry | WorldEditCapability::Npcs |
        WorldEditCapability::Undo);
    session.setTargetExistsCallback([](WorldEditTargetKind,
                                       const std::string& objectId) {
        return objectId == "e1" || objectId == "e2" || objectId == "npc1" ||
               objectId == "zone1" || objectId == "interaction1";
    });

    auto first = session.submit("editor", makeOp(
        1, 0, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e1", 1,
        WorldEditValue::number(1.0)));
    check(first.accepted && first.sequence == 1 && first.revision == 1,
          "independent property A accepted");

    auto second = session.submit("editor", makeOp(
        2, 0, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e2", 1,
        WorldEditValue::number(2.0)));
    check(second.accepted && second.sequence == 2,
          "independent property B merges");

    auto third = session.submit("editor", makeOp(
        3, 1, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e1", 1,
        WorldEditValue::number(3.0)));
    check(third.accepted && third.sequence == 3,
          "same property advances from base revision");

    auto duplicate = session.submit("editor", makeOp(
        3, 3, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e1", 1,
        WorldEditValue::number(9.0)));
    check(!duplicate.accepted && duplicate.reason == "duplicate-operation-id",
          "duplicate operation id rejected", duplicate.reason);

    auto stale = session.submit("editor", makeOp(
        4, 1, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e1", 1,
        WorldEditValue::number(4.0)));
    check(!stale.accepted && stale.conflict &&
              stale.reason == "stale-property-revision" &&
              stale.currentPropertyRevision == 3,
          "same-property conflict reported", stale.reason);

    auto unauthorized = session.submit("guest", makeOp(
        5, 0, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "e1", 2,
        WorldEditValue::number(5.0)));
    check(!unauthorized.accepted &&
              unauthorized.reason == "capability-denied",
          "unauthorized edit rejected", unauthorized.reason);

    auto npcAllowed = session.submit("editor", makeOp(
        6, 0, WorldEditCapability::Npcs, WorldEditOpKind::Set,
        WorldEditTargetKind::NpcPreset, "npc1", 1,
        WorldEditValue::text("wander")));
    check(npcAllowed.accepted, "npc capability accepted");

    auto zoneDenied = session.submit("editor", makeOp(
        7, 0, WorldEditCapability::Chunks, WorldEditOpKind::Set,
        WorldEditTargetKind::GenerationZone, "zone1", 1,
        WorldEditValue::integer(7)));
    check(!zoneDenied.accepted && zoneDenied.reason == "capability-denied",
          "zone capability denied", zoneDenied.reason);

    auto missing = session.submit("editor", makeOp(
        8, 0, WorldEditCapability::Geometry, WorldEditOpKind::Set,
        WorldEditTargetKind::Entity, "missing", 1,
        WorldEditValue::number(8.0)));
    check(!missing.accepted && missing.reason == "target-missing",
          "missing target rejected", missing.reason);

    auto ownUndo = session.submitUndo("editor", 3);
    check(ownUndo.accepted, "own-edit undo accepted", ownUndo.reason);
    WorldEditSnapshot snapshot;
    session.snapshot(snapshot);
    WorldEditValue value;
    check(findSnapshotValue(snapshot, WorldEditTargetKind::Entity, "e1", 1,
                            value) &&
              value.type == WorldEditValueType::Float &&
              value.floatValue == 1.0,
          "own undo restored previous value");

    auto ownerUndo = session.submitUndo("owner", 2);
    check(ownerUndo.accepted, "owner undo of another author accepted",
          ownerUndo.reason);
    session.snapshot(snapshot);
    check(!findSnapshotValue(snapshot, WorldEditTargetKind::Entity, "e2", 1,
                             value),
          "owner undo removed the later property");

    auto foreignUndo = session.submitUndo("editor", 1);
    check(foreignUndo.accepted, "own first edit can be undone by its author",
          foreignUndo.reason);

    check(session.acknowledge("editor", 2) &&
              session.acknowledged("editor") == 2,
          "acknowledgement advances");

    std::vector<WorldEditAccepted> replay;
    check(session.replay("editor", 2, replay) && !replay.empty() &&
              replay.front().sequence > 2,
          "retained replay returns later operations");

    session.setLogLimitForTesting(3);
    for (uint64_t i = 100; i < 106; ++i) {
        session.submit("editor", makeOp(
            i, 0, WorldEditCapability::Geometry, WorldEditOpKind::Set,
            WorldEditTargetKind::Entity, "e2", static_cast<uint16_t>(i),
            WorldEditValue::number(double(i))));
    }
    check(!session.replay("editor", 0, replay),
          "unretained gap reports snapshot fallback");
    session.snapshot(snapshot);
    check(snapshot.revision == session.revision() &&
              snapshot.lastSequence == session.lastSequence() &&
              !snapshot.stateHash.empty() &&
              !snapshot.properties.empty(),
          "snapshot carries revision, sequence, hash, and properties");

    return ok;
}

} // namespace MimitaWorldProject
