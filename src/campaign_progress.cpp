#include "campaign_progress.hpp"
#include <algorithm>
#include <istream>
#include <ostream>

namespace sw2 {
void CampaignProgress::finish(bool won,int seconds,int reward) {
    auto& r=records.at(level);
    r.bestSeconds=r.bestSeconds<0 ? seconds : std::min(r.bestSeconds,seconds);
    r.totalSeconds+=seconds;
    ++r.attempts;
    if (won) {points+=reward; ++level;}
}
void CampaignProgress::write(std::ostream& out) const {
    out<<"DeadWarrior2-Campaign 1\n"<<level<<' '<<points<<' '<<int(difficulty)+1<<' '<<upgrades<<'\n';
    for (const auto& r:records) out<<r.bestSeconds<<' '<<r.totalSeconds<<' '<<r.attempts<<'\n';
}
std::optional<CampaignProgress> CampaignProgress::read(std::istream& in) {
    CampaignProgress p;
    std::string magic,bits;
    int version=0,difficulty=0;
    if (!(in>>magic>>version>>p.level>>p.points>>difficulty>>bits) ||
        magic!="DeadWarrior2-Campaign" || version!=1 || p.level<1 || p.level>levelCount ||
        p.points<0 || p.points>p.level || difficulty<1 || difficulty>3 ||
        bits.size()!=p.upgrades.size() || bits.find_first_not_of("01")!=std::string::npos) return {};
    p.upgrades=std::bitset<upgradeCount+1>(bits);
    if (!p.upgrades[1] || p.upgrades[0]) return {};
    p.difficulty=Difficulty(difficulty-1);
    for (auto& r:p.records) {
        if (!(in>>r.bestSeconds>>r.totalSeconds>>r.attempts) || r.attempts<0 || r.totalSeconds<0 ||
            r.bestSeconds>r.totalSeconds || (r.attempts ? r.bestSeconds<0 : r.bestSeconds!=-1 || r.totalSeconds!=0))
            return {};
    }
    in>>std::ws;
    if (!in.eof()) return {};
    return p;
}
}
