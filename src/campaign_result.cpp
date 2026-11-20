#include "campaign_tutorial.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace sw2 {
void CampaignTutorial::recordArmies() {
    Sample sample{0,0,population_,enemyPopulation_};
    for (const auto& u:units_) if (u.hp>0 && isWorker(u.symbol))
        ++(u.enemy ? sample.enemyWorkers : sample.playerWorkers);
    history_.push_back(sample);
}
bool CampaignTutorial::showingResults() const {return step_>=Step::Victory && counter_>=88;}
bool CampaignTutorial::cardHit(Point p) const {
    // The hit frame of button 333 is a 102 x 42 rectangle at (-51, -21).
    const Point origin=won() ? Point{424.05,490.10} : Point{426.10,417.05};
    return showingResults() && resultCard_ && Rect{origin.x-51,origin.y-21,102,42}.contains(p);
}
CampaignTutorial::ResultAction CampaignTutorial::resultHit(Point p) const {
    if (!showingResults() || resultCard_) return ResultAction::None;
    if (sprite(301,1,{168,633.6}).target.contains(p)) return ResultAction::Online;
    if (sprite(301,1,{485.95,633.6}).target.contains(p)) return won() ? ResultAction::Continue : ResultAction::Retry;
    return ResultAction::None;
}
CampaignTutorial::ResultAction CampaignTutorial::takeResultAction() {
    return std::exchange(resultAction_,ResultAction::None);
}
void CampaignTutorial::tickOutcome() {
    if (!showingResults()) {
        const auto& lost=statues_[won()];
        targetCamera_+=(lost.position.x-legacyWidth/2-targetCamera_)*.3;
        targetCamera_=std::clamp(targetCamera_,-Viewport::left,length_-Viewport::right);
        camera_+=int((targetCamera_-camera_)*rules_.at("screenScrollGain"));
        if (++counter_==88) {
            resultCard_=!won() || mission_<4 || mission_==5; cardAlpha_=won() ? 0. : 1.;
            if (won() && resultCard_) sound("UnitUnlock",{},false);
        }
    } else if (resultCard_) {
        if (closingCard_) {
            cardAlpha_*=.7;
            if (cardAlpha_<.01) {
                if (mission_==3 && unlockCard_==0) {
                    unlockCard_=1; closingCard_=false; cardAlpha_=0;
                    sound("UnitUnlock",{},false);
                } else resultCard_=false;
            }
        } else cardAlpha_+=(1-cardAlpha_)*.2;
    }
}
std::vector<Draw> CampaignTutorial::drawOutcome() const {
    std::vector<Draw> out;
    coverBackdrop(out,sprite(286,1,{0,0}),{Viewport::left,0,Viewport::width,Viewport::height});
    out.push_back(sprite(343,1+mission_,{0,0}));
    out.push_back(sprite(312,won() ? 3 : 4,{165.5,40}));
    auto time=[](int seconds){
        std::ostringstream s;
        s<<std::setfill('0')<<std::setw(2)<<seconds/60<<':'<<std::setw(2)<<seconds%60;
        return s.str();
    };
    out.push_back(Draw::label({334.75,219.9,181,26},time(int(history_.size())*2),18,0xffffff,true));
    int maximum=1,maxPopulation=0;
    for (const auto& s:history_) {
        maxPopulation=std::max({maxPopulation,s.playerArmy,s.enemyArmy});
        maximum=std::max({maximum,maxPopulation,s.playerWorkers,s.enemyWorkers});
    }
    constexpr Rect graph{67,311,740,216};
    const std::array<int Sample::*,4> series={&Sample::playerWorkers,&Sample::enemyWorkers,&Sample::playerArmy,&Sample::enemyArmy};
    constexpr unsigned colors[]={26367,16685313,35840,10223616};
    for (int layer=0;layer<3;++layer) for (int i=0;i<4;++i) {
        auto line=Draw::shape(Draw::Kind::Line,graph,layer==0 ? 2365457 : layer==1 ? 0 : colors[i]);
        line.lineWidth=layer==1 ? 10 : 2;
        for (size_t j=0;j<history_.size();++j)
            line.vertices.push_back({graph.w*j/std::max<size_t>(history_.size()-1,1),
                graph.h-(history_[j].*series[i])*graph.h/maximum+(layer==0 ? 6 : 0)});
        out.push_back(std::move(line));
    }
    std::string prev;
    for (int i=0;i<graph.w/75;++i) {
        const auto value=time(int(i*75/graph.w*history_.size()*2));
        if (value!=prev) out.push_back(Draw::label({graph.x+i*75-5,graph.y+graph.h+3,55,18},value,12,0xffffff));
        prev=value;
    }
    prev.clear();
    for (int i=0;i<graph.h/75+1;++i) {
        const auto value=std::to_string(int(maxPopulation*i/(graph.w/75)));
        if (value!=prev) out.push_back(Draw::label({graph.x-20,graph.y+graph.h-i*75-6,25,18},value,12,0xffffff));
        prev=value;
    }
    for (const auto& [x,label]:std::array<std::pair<double,const char*>,2>{{{168.,"Play Online"},{485.95,won() ? "Continue" : "Retry"}}}) {
        out.push_back(sprite(301,1,{x,633.6}));
        out.push_back(Draw::label({x+8,644.2,185,26},label,18,0xffffff,true));
    }
    if (resultCard_) {
        const Point origin=won() ? Point{162,-75} : Point{-159.95,-97.1};
        const Point backdrop=won() ? Point{origin.x-324,origin.y-21.05} : origin;
        auto shade=sprite(320,1,backdrop);
        auto r=shade.target;
        for (Rect flank:{Rect{Viewport::left,0,r.x-Viewport::left,700},Rect{r.x+r.w,0,Viewport::right-r.x-r.w,700}})
            if (flank.w>0) out.push_back(Draw::shape(Draw::Kind::Box,flank,0,.75*cardAlpha_));
        shade.alpha=cardAlpha_; out.push_back(std::move(shade));
        auto card=sprite(won() ? 335 : 338,1+mission_+(won() && unlockCard_ ? 1000 : 0),origin);
        card.alpha=cardAlpha_; out.push_back(std::move(card));
        if (won()) {
            auto portrait=sprite(mission_==5 ? 1314 : mission_==3 ? (unlockCard_ ? 1480 : 1531) : mission_==2 ? 1272 : mission_==1 ? 1509 : 1366,
                1,{origin.x+262.5,origin.y+369});
            portrait.alpha=cardAlpha_; out.push_back(std::move(portrait));
        }
    }
    return out;
}
}
