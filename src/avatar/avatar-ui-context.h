#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

class AvatarSystem;
class Player;

// Provides data binding values for the Avatar Editor UI.
// Resolves named bindings like "avatar_editor.avatar_name" to actual runtime values.
class AvatarUiDataContext {
public:
    static AvatarUiDataContext& instance();

    // Resolve a binding to a string value
    std::string getString(const std::string& binding, const std::string& fallback = "") const;

    // Resolve a binding to a boolean
    bool getBool(const std::string& binding) const;

    // Resolve a binding to an integer
    int getInt(const std::string& binding) const;

    // Check a visibility condition
    bool checkCondition(const std::string& binding, const std::string& op, const std::string& value) const;

    // These are set by the avatar editor each frame
    void setSelectedTab(int tab) { mSelectedTab = tab; }
    int selectedTab() const { return mSelectedTab; }
    void setSelectedTexture(const std::string& tex) { mSelectedTexture = tex; }
    const std::string& selectedTexture() const { return mSelectedTexture; }

    // PNG data source for dynamic lists
    struct PngEntry {
        std::string filename;
        std::string fullPath;
        bool selected = false;
    };
    void setPngList(const std::vector<PngEntry>& list) { mPngList = list; }
    const std::vector<PngEntry>& pngList() const { return mPngList; }

    // Cosmetic data source
    struct CosmeticEntry {
        std::string id;
        std::string name;
        std::string category;
        bool equipped = false;
    };
    void setCosmeticList(const std::vector<CosmeticEntry>& list) { mCosmeticList = list; }
    const std::vector<CosmeticEntry>& cosmeticList() const { return mCosmeticList; }

    // Outfit list
    using OutfitEntry = std::string;
    void setOutfitList(const std::vector<OutfitEntry>& list) { mOutfitList = list; }
    const std::vector<OutfitEntry>& outfitList() const { return mOutfitList; }

    // Face entries for the face grid
    struct FaceEntry {
        std::string partKey;    // "head", "torso", etc.
        std::string faceKey;    // "front", "back", etc.
        std::string partLabel;
        std::string faceLabel;
        int partIndex;
        int faceIndex;
        bool checked = false;
    };
    void setFaceList(const std::vector<FaceEntry>& list) { mFaceList = list; }
    const std::vector<FaceEntry>& faceList() const { return mFaceList; }

private:
    AvatarUiDataContext() = default;
    int mSelectedTab = 0;
    std::string mSelectedTexture;
    std::vector<PngEntry> mPngList;
    std::vector<CosmeticEntry> mCosmeticList;
    std::vector<OutfitEntry> mOutfitList;
    std::vector<FaceEntry> mFaceList;
};
