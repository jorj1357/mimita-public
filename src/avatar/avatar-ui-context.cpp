#include "avatar-ui-context.h"
#include "avatar.h"

#include <cstdio>
#include <glm/glm.hpp>

AvatarUiDataContext& AvatarUiDataContext::instance() {
    static AvatarUiDataContext ctx;
    return ctx;
}

std::string AvatarUiDataContext::getString(const std::string& binding, const std::string& fallback) const {
    auto& av = AvatarSystem::instance();
    if (binding == "avatar_editor.avatar_name")
        return av.hasAvatar() ? av.currentName() : fallback;
    if (binding == "avatar_editor.save_status") {
        const auto& as = av.autosave();
        switch (as.status()) {
            case AvatarAutosave::Status::Saving: return "Saving...";
            case AvatarAutosave::Status::Ok: {
                double secs = as.secondsSinceLastSave();
                if (secs < 0) return "Not saved yet";
                if (secs < 5) return "Autosaved just now";
                if (secs < 60) return "Autosaved " + std::to_string((int)secs) + "s ago";
                return "Autosaved";
            }
            case AvatarAutosave::Status::Failed: return "Could not save - retrying";
            case AvatarAutosave::Status::Recovered: return as.statusMessage();
            default: return fallback;
        }
    }
    if (binding == "avatar_editor.save_path")
        return av.hasAvatar() ? av.autosave().projectFilePath() : fallback;
    if (binding == "avatar_editor.selected_texture")
        return mSelectedTexture.empty() ? fallback : mSelectedTexture;
    if (binding == "avatar_editor.png_count")
        return std::to_string((int)mPngList.size());
    if (binding == "avatar_editor.cosmetic_count")
        return std::to_string((int)mCosmeticList.size());
    return fallback;
}

bool AvatarUiDataContext::getBool(const std::string& binding) const {
    auto& av = AvatarSystem::instance();
    if (binding == "avatar_editor.has_avatar")
        return av.hasAvatar();
    if (binding == "avatar_editor.has_selected_texture")
        return !mSelectedTexture.empty();
    return false;
}

int AvatarUiDataContext::getInt(const std::string& binding) const {
    if (binding == "avatar_editor.selected_tab")
        return mSelectedTab;
    if (binding == "avatar_editor.png_count")
        return (int)mPngList.size();
    return 0;
}

bool AvatarUiDataContext::checkCondition(const std::string& binding, const std::string& op, const std::string& value) const {
    if (op == "equals") {
        if (binding == "avatar_editor.selected_tab")
            return std::to_string(mSelectedTab) == value;
        std::string s = getString(binding);
        return s == value;
    }
    if (op == "not_equals") {
        std::string s = getString(binding);
        return s != value;
    }
    if (op == "exists") {
        return !getString(binding).empty();
    }
    return true;
}
