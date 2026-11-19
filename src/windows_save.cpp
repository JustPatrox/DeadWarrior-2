#include <windows.h>
#include <shlobj.h>
#include "windows_save.hpp"
#include <fstream>
#include <stdexcept>

namespace sw2 {
namespace fs=std::filesystem;
fs::path defaultSaveDirectory() {
    PWSTR base=nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&base)))
        throw std::runtime_error("Cannot find the local campaign save directory");
    fs::path dir(base);
    CoTaskMemFree(base);
    return dir/L"DeadWarrior 2";
}
std::optional<CampaignProgress> loadProgress(const fs::path& dir) {
    std::ifstream file(dir/L"campaign.sav");
    return CampaignProgress::read(file);
}
bool saveProgress(const CampaignProgress& p,const fs::path& dir) {
    std::error_code ec;
    fs::create_directories(dir,ec);
    if (ec) return false;
    wchar_t tmp[MAX_PATH]{};
    if (!GetTempFileNameW(dir.c_str(),L"DW2",0,tmp)) return false;
    std::ofstream stream(tmp,std::ios::trunc);
    p.write(stream); stream.close();
    if (stream && MoveFileExW(tmp,(dir/L"campaign.sav").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(tmp);
    return false;
}
bool deleteProgress(const fs::path& dir) {
    if (DeleteFileW((dir/L"campaign.sav").c_str())) return true;
    return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND;
}
}
