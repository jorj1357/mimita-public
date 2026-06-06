#pragma once

#include "dev-types.h"
#include <unordered_set>
#include <vector>

class Npc;
class NpcSystem;
class Camera;

class NpcSelectionManager {
public:
    static NpcSelectionManager& instance();
    
    void select(uint32_t npcId);
    void deselect(uint32_t npcId);
    void toggle(uint32_t npcId);
    void selectAll(const NpcSystem& npcSystem);
    void clear();
    
    bool isSelected(uint32_t npcId) const;
    const std::unordered_set<uint32_t>& selectedIds() const { return mSelectedIds; }
    std::vector<const Npc*> getSelected(const NpcSystem& npcSystem) const;
    const Npc* getSingleSelected(const NpcSystem& npcSystem) const;
    size_t count() const { return mSelectedIds.size(); }
    
    void selectNext(const NpcSystem& npcSystem);
    void selectPrev(const NpcSystem& npcSystem);
    
    void drawSelection(const NpcSystem& npcSystem, const Camera& camera) const;
    
    void update(); // For hover selection, etc.
    
private:
    NpcSelectionManager() = default;
    std::unordered_set<uint32_t> mSelectedIds;
    std::vector<uint32_t> mSelectionOrder;
};
