#pragma once
#include <filesystem>
#include <string_view>

namespace sw2 {
void initializeLog(const std::filesystem::path& executableDirectory);
void logEvent(std::string_view event) noexcept;
}
