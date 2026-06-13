#pragma once

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

#include <glm/glm.hpp>

// A single transform write event
struct TransformWriteEvent {
    std::string system;       // "Physics", "Animation", "Network", "SkeletonSync", "Ragdoll"
    std::string entityId;
    glm::vec3 oldPos{};
    glm::vec3 newPos{};
    glm::vec3 oldVel{};
    glm::vec3 newVel{};
    double timestamp = 0.0;
};

// Per-entity write history
struct EntityTransformLog {
    static constexpr size_t MAX_HISTORY = 64;
    std::deque<TransformWriteEvent> writes;
};

// Global transform debug tracker
class TransformDebug {
public:
    static TransformDebug& instance();

    // Enable/disable per entity
    void setEnabled(bool e) { mEnabled = e; }
    bool isEnabled() const { return mEnabled; }

    void setTargetEntity(const std::string& id) { mTargetEntity = id; }
    const std::string& targetEntity() const { return mTargetEntity; }
    bool hasTarget() const { return !mTargetEntity.empty(); }

    bool matches(const std::string& entityId) const {
        return mEnabled && (mTargetEntity.empty() || entityId.find(mTargetEntity) != std::string::npos);
    }

    // Log a transform write
    void logWrite(const std::string& system, const std::string& entityId,
                  const glm::vec3& oldPos, const glm::vec3& newPos,
                  const glm::vec3& oldVel = glm::vec3(0), const glm::vec3& newVel = glm::vec3(0));

    // Get history for an entity
    const std::deque<TransformWriteEvent>* getHistory(const std::string& entityId) const;

    // Clear all history
    void clear();

private:
    TransformDebug() = default;
    bool mEnabled = false;
    std::string mTargetEntity;
    std::unordered_map<std::string, EntityTransformLog> mLogs;
};
