#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","deads","interaction"}) art.loadTextures(dir/folder);
        CampaignProgress progress;progress.level=8;progress.points=2;
        for(int upgrade:{2,3,5,8,13,17}) progress.upgrades.set(upgrade);
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if(!value) throw std::runtime_error(reason);};
        auto click=[&](Point p){scene.press(p);scene.release(p);};
        auto state=scene.battleStatus();
        require(state.miners==4&&state.fighters==1&&state.population==9&&state.enemyDeads==1&&state.bombers==0&&state.gold==700&&state.mana==200,"Dead deployment differs from XML");
        scene.selectArmy();require(scene.battleStatus().selectedMilitary==1,"SPACE selection included miners or lost soldiers");
        click({100,380});
        scene.command({500,320});
        click({310,600});for(int i=0;i<80;++i) scene.tick();
        click({550-scene.camera(),300});click({675,606});
        require(scene.defenceStatus().researchTicks>=0,"Castle research unavailable");
        click({30,610});click({30,610});
        bool hint=false,cureEnabled=false;int maxDeads=1,maxGiants=0,hintDuration=0;
        for(int i=0;i<26000&&!scene.ended();++i) {
            state=scene.battleStatus();
            if(!cureEnabled&&state.alliedHealers>0) {scene.selectArmy();scene.unitAbility(1);cureEnabled=true;}
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
            maxDeads=std::max(maxDeads,state.enemyDeads);maxGiants=std::max(maxGiants,state.enslavedGiants);
            const auto draws=scene.drawList();
            if(i%15==0&&state.alliedWizards>0) {
                const auto enemy=std::find_if(draws.begin(),draws.end(),[](const Draw& d){
                    return (((d.symbol==8262||d.symbol==8293||d.symbol==8364)&&d.frame>=10001)||(d.symbol==1904&&d.frame==3))&&d.target.x>Viewport::left&&
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
            if(i%900==0) std::cout<<scene.ticks()<<" gold "<<state.gold<<" mana "<<state.mana<<" workers "<<state.miners<<" soldiers "<<state.fighters<<" mages "<<state.alliedWizards<<" nukes "<<state.spells[0]<<" cures "<<state.cures<<" deads "<<state.enemyDeads<<" guts "<<state.gutsShots<<" poison "<<state.poisonHits<<" giants "<<state.enslavedGiants<<" boulders "<<state.boulderShots<<'/'<<state.boulderHits<<" statues "<<state.playerStatue<<'/'<<state.enemyStatue<<'\n';
        }
        state=scene.battleStatus();
        std::cout<<"Deads tick "<<scene.ticks()<<" won "<<scene.won()<<" recruited "<<maxDeads<<" giants "<<maxGiants<<" hint "<<hintDuration<<" castle shots "<<state.enemyCastleShots<<'\n';
        require(scene.won(),"Deads did not reach victory");
        require(maxDeads>1&&state.gutsShots>0&&state.poisonHits>0,"Enemy Dead recruitment or poisoned attacks missing");
        require(hint&&hintDuration==152,"CampaignDeads instruction duration differs from its controller");
        require(state.enemyCastleShots>0,"Chaos castle research never activated its Dead defender");
        for(int i=0;i<120;++i) scene.tick();scene.drawList();
        progress.finish(true,scene.ticks()/30,scene.reward());std::stringstream data;progress.write(data);const auto restored=CampaignProgress::read(data);
        require(restored&&restored->level==9&&restored->points==3,"Ninth mission save did not restore");
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for(int i=0;i<90;++i) {map.tick();map.drawList();}
        require(map.mapFrame()==510,"Map did not reach Crawlers");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
