#include "transform-debug.h"

#include <cstdio>

TransformDebug& TransformDebug::instance()
{
    static TransformDebug dbg;
    return dbg;
}

void TransformDebug::logWrite(const std::string& system, const std::string& entityId,
                               const glm::vec3& oldPos, const glm::vec3& newPos,
                               const glm::vec3& oldVel, const glm::vec3& newVel)
{
    if (!mEnabled) return;
    if (!mTargetEntity.empty() && entityId.find(mTargetEntity) == std::string::npos)
        return;

    TransformWriteEvent ev;
    ev.system = system;
    ev.entityId = entityId;
    ev.oldPos = oldPos;
    ev.newPos = newPos;
    ev.oldVel = oldVel;
    ev.newVel = newVel;
    ev.timestamp = 0.0; // not used for now

    auto& log = mLogs[entityId];
    log.writes.push_back(ev);
    if (log.writes.size() > EntityTransformLog::MAX_HISTORY)
        log.writes.pop_front();

    glm::vec3 delta = newPos - oldPos;
    printf("[TRANSFORM WRITE] entity=%s system=%s "
           "oldPos=(%.2f %.2f %.2f) newPos=(%.2f %.2f %.2f) "
           "delta=(%.2f %.2f %.2f)\n",
           entityId.c_str(), system.c_str(),
           oldPos.x, oldPos.y, oldPos.z,
           newPos.x, newPos.y, newPos.z,
           delta.x, delta.y, delta.z);

    if (glm::length(oldVel) > 0.0f || glm::length(newVel) > 0.0f) {
        printf("[TRANSFORM WRITE] entity=%s system=%s "
               "oldVel=(%.2f %.2f %.2f) newVel=(%.2f %.2f %.2f)\n",
               entityId.c_str(), system.c_str(),
               oldVel.x, oldVel.y, oldVel.z,
               newVel.x, newVel.y, newVel.z);
    }
}

const std::deque<TransformWriteEvent>* TransformDebug::getHistory(const std::string& entityId) const
{
    auto it = mLogs.find(entityId);
    if (it == mLogs.end()) return nullptr;
    return &it->second.writes;
}

void TransformDebug::clear()
{
    mLogs.clear();
}
