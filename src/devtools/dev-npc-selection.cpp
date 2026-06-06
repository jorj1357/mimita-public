#include "dev-npc-selection.h"
#include "npc/npc.h"
#include "debug/debug-visuals.h"
#include <algorithm>
#include <cstdio>

NpcSelectionManager& NpcSelectionManager::instance() {
    static NpcSelectionManager sInstance;
    return sInstance;
}

void NpcSelectionManager::select(uint32_t npcId) {
    if (mSelectedIds.insert(npcId).second) {
        mSelectionOrder.push_back(npcId);
        printf("[NPC SELECTION] Selected NPC %u (total: %zu)\n", npcId, mSelectedIds.size());
    }
}

void NpcSelectionManager::deselect(uint32_t npcId) {
    if (mSelectedIds.erase(npcId)) {
        mSelectionOrder.erase(
            std::remove(mSelectionOrder.begin(), mSelectionOrder.end(), npcId),
            mSelectionOrder.end()
        );
        printf("[NPC SELECTION] Deselected NPC %u (total: %zu)\n", npcId, mSelectedIds.size());
    }
}

void NpcSelectionManager::toggle(uint32_t npcId) {
    if (isSelected(npcId)) deselect(npcId);
    else select(npcId);
}

void NpcSelectionManager::selectAll(const NpcSystem& npcSystem) {
    mSelectedIds.clear();
    mSelectionOrder.clear();
    
    for (const Npc& npc : npcSystem.all()) {
        mSelectedIds.insert(npc.id);
        mSelectionOrder.push_back(npc.id);
    }
    printf("[NPC SELECTION] Selected all %zu NPCs\n", mSelectedIds.size());
}

void NpcSelectionManager::clear() {
    mSelectedIds.clear();
    mSelectionOrder.clear();
    printf("[NPC SELECTION] Cleared selection\n");
}

bool NpcSelectionManager::isSelected(uint32_t npcId) const {
    return mSelectedIds.find(npcId) != mSelectedIds.end();
}

std::vector<const Npc*> NpcSelectionManager::getSelected(const NpcSystem& npcSystem) const {
    std::vector<const Npc*> out;
    out.reserve(mSelectedIds.size());
    for (const Npc& npc : npcSystem.all()) {
        if (isSelected(npc.id)) out.push_back(&npc);
    }
    return out;
}

const Npc* NpcSelectionManager::getSingleSelected(const NpcSystem& npcSystem) const {
    if (mSelectedIds.size() != 1) return nullptr;
    uint32_t id = *mSelectedIds.begin();
    for (const Npc& npc : npcSystem.all()) {
        if (npc.id == id) return &npc;
    }
    return nullptr;
}

void NpcSelectionManager::selectNext(const NpcSystem& npcSystem) {
    const auto& allNpcs = npcSystem.all();
    if (allNpcs.empty()) return;
    
    if (mSelectionOrder.empty()) {
        select(allNpcs[0].id);
        return;
    }
    
    uint32_t currentId = mSelectionOrder.back();
    auto it = std::find_if(allNpcs.begin(), allNpcs.end(), [currentId](const Npc& n) { return n.id == currentId; });
    
    if (it != allNpcs.end()) {
        ++it;
        if (it == allNpcs.end()) it = allNpcs.begin();
        clear();
        select(it->id);
    } else {
        clear();
        select(allNpcs[0].id);
    }
}

void NpcSelectionManager::selectPrev(const NpcSystem& npcSystem) {
    const auto& allNpcs = npcSystem.all();
    if (allNpcs.empty()) return;
    
    if (mSelectionOrder.empty()) {
        select(allNpcs.back().id);
        return;
    }
    
    uint32_t currentId = mSelectionOrder.back();
    auto it = std::find_if(allNpcs.begin(), allNpcs.end(), [currentId](const Npc& n) { return n.id == currentId; });
    
    if (it != allNpcs.end()) {
        if (it == allNpcs.begin()) it = allNpcs.end();
        --it;
        clear();
        select(it->id);
    } else {
        clear();
        select(allNpcs.back().id);
    }
}

void NpcSelectionManager::drawSelection(const NpcSystem& npcSystem, const Camera& camera) const {
    if (mSelectedIds.empty()) return;
    
    const glm::vec4 selectColor{1.0f, 1.0f, 0.0f, 1.0f};
    const glm::vec4 textColor{1.0f, 1.0f, 0.5f, 1.0f};
    
    for (const Npc& npc : npcSystem.all()) {
        if (!isSelected(npc.id)) continue;
        
        const glm::vec3 pos = npc.body.pos;
        const float radius = 1.2f;
        
        DebugVis::drawWireSphere(camera, pos, radius, selectColor);
        DebugVis::drawWireSphere(camera, pos, radius * 1.3f, {1.0f, 1.0f, 0.0f, 0.5f});
        
        char label[64];
        snprintf(label, sizeof(label), "NPC %u [SELECTED]", npc.id);
        DebugVis::drawWorldLabel(pos + glm::vec3(0.0f, 0.0f, 2.5f), label, textColor);
        
        DebugVis::drawLine(camera, pos, pos + glm::vec3(0, 0, 3.0f), selectColor);
    }
}

void NpcSelectionManager::update() {
    // Future: hover selection, box selection, etc.
}
