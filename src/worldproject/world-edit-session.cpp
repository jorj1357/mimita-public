// 2026-11-05
#include "worldproject/world-edit-session.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "worldproject/world-project-sha256.h"

namespace MimitaWorldProject {
namespace {

constexpr uint32_t kCapabilityBit(WorldEditCapability capability)
{
    return static_cast<uint32_t>(capability);
}

std::string encodeValue(const WorldEditValue& value)
{
    std::ostringstream out;
    out << static_cast<int>(value.type) << ':';
    switch (value.type) {
    case WorldEditValueType::Bool:
        out << (value.boolValue ? '1' : '0');
        break;
    case WorldEditValueType::Int:
        out << value.intValue;
        break;
    case WorldEditValueType::Float:
        out.precision(17);
        out << value.floatValue;
        break;
    case WorldEditValueType::Vec3:
        out.precision(17);
        out << value.vec3Value.x << ',' << value.vec3Value.y << ','
            << value.vec3Value.z;
        break;
    case WorldEditValueType::String:
        out << value.stringValue;
        break;
    default:
        break;
    }
    return out.str();
}

} // namespace

WorldEditValue WorldEditValue::boolean(bool value)
{
    WorldEditValue out;
    out.type = WorldEditValueType::Bool;
    out.boolValue = value;
    return out;
}

WorldEditValue WorldEditValue::integer(int64_t value)
{
    WorldEditValue out;
    out.type = WorldEditValueType::Int;
    out.intValue = value;
    return out;
}

WorldEditValue WorldEditValue::number(double value)
{
    WorldEditValue out;
    out.type = WorldEditValueType::Float;
    out.floatValue = value;
    return out;
}

WorldEditValue WorldEditValue::vector(double x, double y, double z)
{
    WorldEditValue out;
    out.type = WorldEditValueType::Vec3;
    out.vec3Value = {x, y, z};
    return out;
}

WorldEditValue WorldEditValue::text(std::string value)
{
    WorldEditValue out;
    out.type = WorldEditValueType::String;
    out.stringValue = std::move(value);
    return out;
}

bool WorldEditValue::operator==(const WorldEditValue& other) const
{
    if (type != other.type) return false;
    switch (type) {
    case WorldEditValueType::Bool: return boolValue == other.boolValue;
    case WorldEditValueType::Int: return intValue == other.intValue;
    case WorldEditValueType::Float: return floatValue == other.floatValue;
    case WorldEditValueType::Vec3: return vec3Value == other.vec3Value;
    case WorldEditValueType::String: return stringValue == other.stringValue;
    default: return true;
    }
}

void WorldEditSession::begin(const std::string& projectId,
                             const std::string& projectVersion,
                             const std::string& ownerAccountId)
{
    reset();
    mActive = true;
    mProjectId = projectId;
    mProjectVersion = projectVersion;
    mOwnerAccountId = ownerAccountId;
    mCapabilities[ownerAccountId] = WorldEditCapability::All;
}

void WorldEditSession::reset()
{
    mActive = false;
    mProjectId.clear();
    mProjectVersion.clear();
    mOwnerAccountId.clear();
    mSequence = 0;
    mRevision = 0;
    mCapabilities.clear();
    mPropertyRevisions.clear();
    mPropertyValues.clear();
    mAcknowledgements.clear();
    mLog.clear();
    mLogLimit = 2048;
    mTargetExists = nullptr;
}

void WorldEditSession::setCapabilities(const std::string& accountId,
                                       WorldEditCapability mask)
{
    if (accountId == mOwnerAccountId) {
        mCapabilities[accountId] = WorldEditCapability::All;
        return;
    }
    mCapabilities[accountId] = mask;
}

void WorldEditSession::grantCapabilities(const std::string& accountId,
                                         WorldEditCapability mask)
{
    if (accountId == mOwnerAccountId) return;
    mCapabilities[accountId] = mCapabilities[accountId] | mask;
}

void WorldEditSession::revokeCapabilities(const std::string& accountId,
                                          WorldEditCapability mask)
{
    if (accountId == mOwnerAccountId) return;
    const auto it = mCapabilities.find(accountId);
    if (it == mCapabilities.end()) return;
    it->second = static_cast<WorldEditCapability>(
        kCapabilityBit(it->second) & ~kCapabilityBit(mask));
}

WorldEditCapability WorldEditSession::capabilities(
    const std::string& accountId) const
{
    if (accountId == mOwnerAccountId) return WorldEditCapability::All;
    const auto it = mCapabilities.find(accountId);
    return it == mCapabilities.end() ? WorldEditCapability::None : it->second;
}

bool WorldEditSession::authorized(const std::string& accountId,
                                  WorldEditCapability required) const
{
    return (kCapabilityBit(capabilities(accountId)) &
            kCapabilityBit(required)) == kCapabilityBit(required);
}

bool WorldEditSession::hasCapabilityForTarget(
    const std::string& accountId,
    WorldEditCapability declared,
    WorldEditTargetKind target) const
{
    WorldEditCapability required = declared;
    if (required == WorldEditCapability::None) {
        switch (target) {
        case WorldEditTargetKind::NpcPreset:
            required = WorldEditCapability::Npcs;
            break;
        case WorldEditTargetKind::GenerationZone:
            required = WorldEditCapability::Chunks;
            break;
        case WorldEditTargetKind::Interaction:
            required = WorldEditCapability::Interactions;
            break;
        case WorldEditTargetKind::World:
            required = WorldEditCapability::Geometry;
            break;
        default:
            required = WorldEditCapability::Geometry;
            break;
        }
    }
    return authorized(accountId, required);
}

std::string WorldEditSession::propertyKey(WorldEditTargetKind target,
                                          const std::string& objectId,
                                          uint16_t propertyId) const
{
    return std::to_string(static_cast<int>(target)) + "|" + objectId + "|" +
           std::to_string(propertyId);
}

uint32_t WorldEditSession::propertyRevision(const std::string& key) const
{
    const auto it = mPropertyRevisions.find(key);
    return it == mPropertyRevisions.end() ? 0 : it->second;
}

uint32_t WorldEditSession::propertyRevisionFor(WorldEditTargetKind target,
                                               const std::string& objectId,
                                               uint16_t propertyId) const
{
    return propertyRevision(propertyKey(target, objectId, propertyId));
}

bool WorldEditSession::acceptedBySequence(uint32_t sequence,
                                          WorldEditAccepted& out) const
{
    for (const auto& accepted : mLog) {
        if (accepted.sequence == sequence) {
            out = accepted;
            return true;
        }
    }
    return false;
}

WorldEditResult WorldEditSession::reject(const std::string& reason,
                                         bool conflict,
                                         uint32_t propertyRevisionValue) const
{
    WorldEditResult result;
    result.accepted = false;
    result.conflict = conflict;
    result.reason = reason;
    result.sequence = mSequence;
    result.revision = mRevision;
    result.currentPropertyRevision = propertyRevisionValue;
    return result;
}

WorldEditResult WorldEditSession::submit(const std::string& authorAccountId,
                                         const WorldEditOperation& operation)
{
    if (!mActive)
        return reject("session-inactive", false);
    if (operation.opId == 0)
        return reject("missing-operation-id", false);
    if (authorAccountId.empty())
        return reject("missing-author", false);

    for (const auto& accepted : mLog) {
        if (accepted.opId == operation.opId)
            return reject("duplicate-operation-id", false);
    }

    if (!hasCapabilityForTarget(authorAccountId, operation.capability,
                                operation.target)) {
        return reject("capability-denied", false);
    }

    if (operation.kind != WorldEditOpKind::Create && mTargetExists &&
        !mTargetExists(operation.target, operation.objectId)) {
        return reject("target-missing", false);
    }

    const std::string key = propertyKey(operation.target, operation.objectId,
                                        operation.propertyId);
    const uint32_t currentRevision = propertyRevision(key);
    if (operation.baseRevision != currentRevision) {
        return reject("stale-property-revision", true, currentRevision);
    }

    WorldEditAccepted accepted;
    accepted.sequence = ++mSequence;
    accepted.revision = ++mRevision;
    accepted.opId = operation.opId;
    accepted.authorAccountId = authorAccountId;
    accepted.capability = operation.capability == WorldEditCapability::None
        ? WorldEditCapability::Geometry : operation.capability;
    accepted.kind = operation.kind;
    accepted.target = operation.target;
    accepted.objectId = operation.objectId;
    accepted.propertyId = operation.propertyId;
    accepted.value = operation.value;
    accepted.undoSequence = operation.undoSequence;
    const auto previous = mPropertyValues.find(key);
    accepted.previousExists = previous != mPropertyValues.end();
    if (accepted.previousExists)
        accepted.previousValue = previous->second;
    else if (operation.hasPrevious) {
        accepted.previousExists = true;
        accepted.previousValue = operation.previousValue;
    }

    applyAccepted(accepted);

    WorldEditResult result;
    result.accepted = true;
    result.sequence = accepted.sequence;
    result.revision = accepted.revision;
    result.currentPropertyRevision = accepted.revision;
    return result;
}

WorldEditResult WorldEditSession::submitUndo(
    const std::string& authorAccountId, uint32_t targetSequence)
{
    if (!mActive)
        return reject("session-inactive", false);

    const WorldEditAccepted* original = nullptr;
    for (const auto& accepted : mLog) {
        if (accepted.sequence == targetSequence) {
            original = &accepted;
            break;
        }
    }
    if (!original)
        return reject("undo-target-not-retained", false);
    if (authorAccountId != mOwnerAccountId &&
        authorAccountId != original->authorAccountId) {
        return reject("undo-not-owned", false);
    }
    if (authorAccountId != mOwnerAccountId &&
        !authorized(authorAccountId, WorldEditCapability::Undo)) {
        return reject("capability-denied", false);
    }
    if (original->kind == WorldEditOpKind::Undo)
        return reject("cannot-undo-undo", false);

    WorldEditOpKind inverseKind = WorldEditOpKind::Set;
    WorldEditValue inverseValue = original->previousValue;
    if (original->kind == WorldEditOpKind::Create) {
        inverseKind = WorldEditOpKind::Delete;
    } else if (original->kind == WorldEditOpKind::Delete ||
               !original->previousExists) {
        if (!original->previousExists)
            inverseKind = WorldEditOpKind::Delete;
        else
            inverseKind = WorldEditOpKind::Create;
    }

    if (inverseKind == WorldEditOpKind::Create &&
        !original->previousExists) {
        return reject("undo-has-no-previous-state", false);
    }

    const std::string key = propertyKey(original->target, original->objectId,
                                        original->propertyId);
    const uint32_t currentRevision = propertyRevision(key);

    WorldEditAccepted accepted;
    accepted.sequence = ++mSequence;
    accepted.revision = ++mRevision;
    accepted.opId = (uint64_t(mSequence) << 32) | mRevision;
    accepted.authorAccountId = authorAccountId;
    accepted.capability = WorldEditCapability::Undo;
    accepted.kind = inverseKind;
    accepted.target = original->target;
    accepted.objectId = original->objectId;
    accepted.propertyId = original->propertyId;
    accepted.value = inverseValue;
    accepted.undoSequence = targetSequence;
    accepted.previousExists = mPropertyValues.find(key) != mPropertyValues.end();
    if (accepted.previousExists)
        accepted.previousValue = mPropertyValues[key];

    applyAccepted(accepted);

    WorldEditResult result;
    result.accepted = true;
    result.sequence = accepted.sequence;
    result.revision = accepted.revision;
    result.currentPropertyRevision = accepted.revision;
    return result;
}

void WorldEditSession::applyAccepted(const WorldEditAccepted& accepted)
{
    const std::string key = propertyKey(accepted.target, accepted.objectId,
                                        accepted.propertyId);
    mPropertyRevisions[key] = accepted.revision;
    if (accepted.kind == WorldEditOpKind::Delete)
        mPropertyValues.erase(key);
    else
        mPropertyValues[key] = accepted.value;
    mLog.push_back(accepted);
    trimLog();
}

void WorldEditSession::trimLog()
{
    while (mLog.size() > mLogLimit)
        mLog.erase(mLog.begin());
}

bool WorldEditSession::acknowledge(const std::string& accountId,
                                   uint32_t sequence)
{
    if (!mActive || accountId.empty() || sequence > mSequence)
        return false;
    uint32_t& current = mAcknowledgements[accountId];
    if (sequence > current) current = sequence;
    return true;
}

uint32_t WorldEditSession::acknowledged(const std::string& accountId) const
{
    const auto it = mAcknowledgements.find(accountId);
    return it == mAcknowledgements.end() ? 0 : it->second;
}

bool WorldEditSession::replay(const std::string&, uint32_t afterSequence,
                              std::vector<WorldEditAccepted>& out) const
{
    out.clear();
    if (!mActive) return false;
    if (afterSequence >= mSequence) return true;
    if (!mLog.empty() && afterSequence + 1 < mLog.front().sequence)
        return false;
    for (const auto& accepted : mLog) {
        if (accepted.sequence > afterSequence)
            out.push_back(accepted);
    }
    return true;
}

bool WorldEditSession::snapshot(WorldEditSnapshot& out) const
{
    out = WorldEditSnapshot{};
    out.lastSequence = mSequence;
    out.revision = mRevision;

    std::vector<std::string> keys;
    keys.reserve(mPropertyValues.size());
    for (const auto& entry : mPropertyValues)
        keys.push_back(entry.first);
    std::sort(keys.begin(), keys.end());

    std::string canonical;
    for (const std::string& key : keys) {
        const auto valueIt = mPropertyValues.find(key);
        if (valueIt == mPropertyValues.end()) continue;
        const std::string encoded = encodeValue(valueIt->second);
        canonical += key + "=" + encoded + "\n";

        WorldEditAccepted state;
        state.sequence = 0;
        const auto revisionIt = mPropertyRevisions.find(key);
        state.revision = revisionIt == mPropertyRevisions.end()
            ? 0 : revisionIt->second;
        state.kind = WorldEditOpKind::Set;
        state.value = valueIt->second;
        state.previousExists = true;
        state.previousValue = valueIt->second;

        const size_t first = key.find('|');
        const size_t second = key.find('|', first == std::string::npos ? first : first + 1);
        if (first != std::string::npos) {
            state.target = static_cast<WorldEditTargetKind>(
                std::atoi(key.substr(0, first).c_str()));
        }
        if (second != std::string::npos) {
            state.objectId = key.substr(first + 1, second - first - 1);
            state.propertyId = static_cast<uint16_t>(
                std::atoi(key.substr(second + 1).c_str()));
        }
        out.properties.push_back(std::move(state));
    }
    out.stateHash = sha256Hex(canonical);
    return true;
}

void WorldEditSession::setTargetExistsCallback(
    std::function<bool(WorldEditTargetKind, const std::string&)> callback)
{
    mTargetExists = std::move(callback);
}

} // namespace MimitaWorldProject
