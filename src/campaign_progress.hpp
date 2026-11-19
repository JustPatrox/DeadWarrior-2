#pragma once
#include "campaign_menu.hpp"
#include <bitset>
#include <iosfwd>

namespace sw2 {
struct LevelRecord {
    int bestSeconds{-1},totalSeconds{},attempts{};
};

struct CampaignProgress {
    static constexpr int levelCount=14,upgradeCount=20;
    static constexpr int playableLevelCount=8;
    int level{},points{};
    Difficulty difficulty{Difficulty::Normal};
    bool autoSave{};
    // Indices correspond to CampaignUpgradeScreen's button1 ... button20.
    // Cloak_BASIC is an implicit initial tech with no button in the tree.
    std::bitset<upgradeCount+1> upgrades{1u<<1};
    std::array<LevelRecord,levelCount> records{};
    void finish(bool won,int seconds,int reward);
    void write(std::ostream&) const;
    static std::optional<CampaignProgress> read(std::istream&);
};
}
