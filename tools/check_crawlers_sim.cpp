#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static std::unique_ptr<CampaignTutorial> create(const MenuAssets& art,const std::filesystem::path& dir,CampaignProgress progress) {
        auto game=std::make_unique<CampaignTutorial>(art,dir/"tutorial",progress.difficulty);
        game->configureCampaign(dir/"archidon",progress);return game;
    }
    static void rules(CampaignTutorial& normal,CampaignTutorial& insane) {
        auto index=[](CampaignTutorial& g){return int(std::find_if(g.units_.begin(),g.units_.end(),[](const auto& u){return u.symbol==6123;})-g.units_.begin());};
        const int a=index(normal),b=index(insane);normal.updateOpponent();insane.updateOpponent();
        if(normal.crawlerTech_ || !insane.crawlerTech_ || normal.stat(normal.units_.at(a),"maxVelocity")!=6 || insane.stat(insane.units_.at(b),"maxVelocity")!=8)
            throw std::runtime_error("Original difficulty-specific Crawler tech differs");
        if(normal.crawlerDamage(normal.units_.at(a))!=5) throw std::runtime_error("Normal Crawler uses the wrong damage branch");
        if(insane.crawlerDamage(insane.units_.at(b))!=6) throw std::runtime_error("Original int pack damage differs from XML fraction");
        for(int i=0;i<6;++i) insane.spawn(6123,{5000.,125.},true);
        if(insane.crawlerDamage(insane.units_.at(b))!=10) throw std::runtime_error("Original five-stack pack cap differs");
    }
};
}
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","deads","crawlers","interaction"}) art.loadTextures(dir/folder);
        CampaignProgress progress;progress.level=9;progress.points=3;
        for(int upgrade:{2,3,5,8,13,17}) progress.upgrades.set(upgrade);
        auto prepared=CampaignProbe::create(art,dir,progress);auto& scene=*prepared;
        auto brutal=progress;brutal.difficulty=Difficulty::Insane;
        auto normalRules=CampaignProbe::create(art,dir,progress),insaneRules=CampaignProbe::create(art,dir,brutal);
        CampaignProbe::rules(*normalRules,*insaneRules);
        auto require=[](bool value,const char* reason){if(!value) throw std::runtime_error(reason);};
        auto abandoned=CampaignProbe::create(art,dir,progress);
        for(Point button:std::array<Point,2>{{{350,634},{350,660}}}) {abandoned->press(button);abandoned->release(button);}
        for(int i=0;i<16000&&!abandoned->ended();++i) abandoned->tick();
        require(abandoned->ended()&&!abandoned->won()&&abandoned->battleStatus().playerStatue==0,
                "Undefended Crawler mission did not reach natural defeat");
        std::cout<<"Crawlers garrison defeat "<<abandoned->ticks()<<'\n';
        auto click=[&](Point p){scene.press(p);scene.release(p);};
        auto state=scene.battleStatus();
        require(state.miners==3&&state.fighters==1&&state.population==7&&state.enemyCrawlers==1&&state.bombers==0&&state.gold==700&&state.mana==200,"Crawler deployment differs from XML");
        scene.selectArmy();require(scene.battleStatus().selectedMilitary==1,"SPACE selection included miners or lost soldiers");
        click({100,380});
        scene.command({500,320});
        click({310,600});for(int i=0;i<80;++i) scene.tick();
        click({550-scene.camera(),300});click({675,606});
        require(scene.defenceStatus().researchTicks>=0,"Castle research unavailable");
        click({30,610});click({30,610});
        bool hint=false;int maxCrawlers=1,maxGiants=0,hintDuration=0;
        for(int i=0;i<26000&&!scene.ended();++i) {
            state=scene.battleStatus();
            if(i%120==0) {
                if(state.miners+state.queued[0]<10) click({30,610});
                state=scene.battleStatus();
                if(state.alliedHealers+state.queued[4]<2&&state.gold>=300&&state.mana>=200) click({200,610});
                state=scene.battleStatus();
                if(state.alliedWizards+state.queued[3]<2&&state.gold>=500&&state.mana>=500) click({260,610});
                state=scene.battleStatus();
                const int swords=state.fighters-state.alliedHealers-state.alliedWizards-state.enslavedGiants-state.archers;
                if(swords+state.queued[1]<6&&state.gold>=100) click({88,610});
                state=scene.battleStatus();
                if(state.miners>=10&&state.gold>=1500&&state.enslavedGiants+state.queued[5]<2) click({230,665});
                click({state.alliedWizards>0||state.enslavedGiants>=2 ? 500. : 425.,634});
                if(state.alliedWizards>0) scene.selectArmy(true);
            }
            scene.tick();state=scene.battleStatus();
            maxCrawlers=std::max(maxCrawlers,state.enemyCrawlers);maxGiants=std::max(maxGiants,state.enslavedGiants);
            const auto draws=scene.drawList();
            if(i%15==0&&state.alliedWizards>0) {
                const auto enemy=std::find_if(draws.begin(),draws.end(),[](const Draw& d){
                    return ((d.symbol==5778||d.symbol==5827||d.symbol==5898||d.symbol==5970)||(d.symbol==1904&&d.frame==11))&&d.target.x>Viewport::left&&
                           d.target.x+d.target.w<Viewport::right&&d.target.y+d.target.h<575;
                });
                if(enemy!=draws.end()) {
                    scene.selectArmy();
                    for(int slot:{0,1,2}) {
                        scene.unitAbility(slot);scene.unitAbility(slot);
                        click({enemy->target.x+enemy->target.w/2,enemy->target.y+enemy->target.h});
                    }
                }
            }
            const bool visible=std::any_of(draws.begin(),draws.end(),[](const Draw& d){return d.text=="A unit has been poisoned. Garrisoning this unit will cure its poison";});
            if(visible) {hint=true;++hintDuration;}
            if(i%900==0) std::cout<<scene.ticks()<<" gold "<<state.gold<<" mana "<<state.mana<<" workers "<<state.miners<<" soldiers "<<state.fighters<<" mages "<<state.alliedWizards<<" nukes "<<state.spells[0]<<" cures "<<state.cures<<" crawlers "<<state.enemyCrawlers<<" guts "<<state.gutsShots<<" poison "<<state.poisonHits<<" giants "<<state.enslavedGiants<<" boulders "<<state.boulderShots<<'/'<<state.boulderHits<<" statues "<<state.playerStatue<<'/'<<state.enemyStatue<<'\n';
        }
        state=scene.battleStatus();
        std::cout<<"Crawlers tick "<<scene.ticks()<<" won "<<scene.won()<<" recruited "<<maxCrawlers<<" giants "<<maxGiants<<" hint "<<hintDuration<<" castle shots "<<state.enemyCastleShots<<'\n';
        require(scene.won(),"Crawlers did not reach victory");
        require(maxCrawlers>1&&state.crawlerHits>0&&state.maxCrawlerDamage==5,"Original Crawler recruitment or normal attack missing");
        require(!hint,"Crawler mission incorrectly displays CampaignDeads instruction");
        require(state.enemyCastleShots>0,"Chaos castle research never activated its Dead defender");
        for(int i=0;i<120;++i) scene.tick();scene.drawList();
        progress.finish(true,scene.ticks()/30,scene.reward());std::stringstream data;progress.write(data);const auto restored=CampaignProgress::read(data);
        require(restored&&restored->level==10&&restored->points==4,"Tenth mission save did not restore");
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for(int i=0;i<90;++i) {map.tick();map.drawList();}
        require(map.mapFrame()==554,"Map did not reach Eclipsors");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
