#pragma once
#include "campaign_progress.hpp"

namespace sw2 {
std::filesystem::path defaultSaveDirectory();
std::optional<CampaignProgress> loadProgress(const std::filesystem::path& directory);
bool saveProgress(const CampaignProgress&,const std::filesystem::path& directory);
bool deleteProgress(const std::filesystem::path& directory);
}
