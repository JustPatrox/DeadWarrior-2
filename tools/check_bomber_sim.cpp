#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","interaction"}) art.loadTextures(dir/folder);
        CampaignProgress progress;progress.level=6;
        // Six earned points buy the original prerequisite chain to Growth II.
        for(int upgrade:{2,4,7,11,16,20}) progress.upgrades.set(upgrade);
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if(!value) throw std::runtime_error(reason);};
        auto click=[&](Point p){scene.press(p);scene.release(p);};
        auto state=scene.battleStatus();
        require(state.miners==4&&state.fighters==1&&state.population==9&&state.bombers==4&&state.gold==700&&state.mana==200&&state.playerStatue==1500&&state.enemyStatue==1500,"Explosive War deployment differs from XML");
        scene.tick();state=scene.battleStatus();
        require(state.bombers==6&&state.bomberWaves==1&&state.nextBomberWave==3,"Frame-zero bomber wave differs from original controller");
        // Grow an economy, research the castle archer, then field a Giant.
        click({310,600});for(int i=0;i<80;++i) scene.tick();
        click({550-scene.camera(),300});click({675,606});
        require(scene.defenceStatus().researchTicks>=0,"Castle archer research unavailable");
        int maxGiants=0,maxArchers=0,giantOrderTick=-1,firstGiant=-1;
        bool priceChecked=false,healerQueued=false;
        for(int i=0;i<22000&&!scene.ended();++i) {
            state=scene.battleStatus();
            if(i%120==0) {
                if(!healerQueued&&state.gold>=300&&state.mana>=200) {
                    click({200,610});healerQueued=true;
                }
                if(state.miners+state.minersQueued<16) click({30,610});
                if(state.archers<3&&state.gold>=200) click({146,610});
                if(state.miners>=14&&state.gold>=1500&&state.enslavedGiants<4) {
                    if(!priceChecked) {
                        const auto gold=scene.battleStatus().gold;click({230,665});
                        require(scene.battleStatus().gold==gold-1500,"Allied Giant training price differs");
                        click({250,646});require(scene.battleStatus().gold==gold,"Giant cancellation failed to refund");
                        click({230,665});giantOrderTick=scene.ticks();priceChecked=true;
                    } else click({230,665});
                }
                if(state.enslavedGiants>=3) {
                    click({500,634});
                } else click({425,634});
            }
            scene.tick();state=scene.battleStatus();
            if(state.enslavedGiants&&firstGiant<0) firstGiant=scene.ticks();
            maxGiants=std::max(maxGiants,state.enslavedGiants);maxArchers=std::max(maxArchers,state.archers);
            if(scene.ticks()==1350) require(state.bomberWaves==1,"Second wave spawned before 45 seconds");
            if(scene.ticks()==1351) require(state.bomberWaves==2&&state.nextBomberWave==3,"45-second bomber wave missing");
            if(i%600==0) std::cout<<scene.ticks()<<" gold "<<state.gold<<" miners "<<state.miners<<" archers "<<state.archers<<" giants "<<state.enslavedGiants<<" waves "<<state.bomberWaves<<" explosions "<<state.explosions<<" boulders "<<state.boulderShots<<'/'<<state.boulderHits<<" statues "<<state.playerStatue<<'/'<<state.enemyStatue<<'\n';
            if(i%120==0) scene.drawList();
        }
        state=scene.battleStatus();
        std::cout<<"Bomber tick "<<scene.ticks()<<" won "<<scene.won()<<" giants "<<maxGiants<<" archers "<<maxArchers<<" trained "<<giantOrderTick<<'/'<<firstGiant<<" shots "<<state.boulderShots<<" hits "<<state.boulderHits<<'\n';
        require(priceChecked&&maxGiants>0,"Allied Giant never trained");
        require(firstGiant-giantOrderTick==1202,"Giant training cooldown differs from production update");
        require(state.boulderShots>0&&state.boulderHits>0&&state.explosions>0,"Boulder or bomber combat did not activate");
        require(scene.won(),"Explosive War did not reach victory");
        require(state.nextBomberWave<=10&&state.bomberWaves>=3,"Wave escalation or its cap differs from the original");
        for(int i=0;i<120;++i) scene.tick();scene.drawList();
        progress.finish(true,scene.ticks()/30,scene.reward());
        std::stringstream data;progress.write(data);const auto restored=CampaignProgress::read(data);
        require(restored&&restored->level==7&&restored->points==1&&restored->upgrades==progress.upgrades,"Seventh mission save round trip failed");
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for(int i=0;i<90;++i) {map.tick();map.drawList();}
        require(map.mapFrame()==401,"Campaign did not advance to Juggerknights");
        progress.level=6;progress.difficulty=Difficulty::Insane;
        CampaignTutorial defeat(art,dir,progress);
        defeat.press({330,634});defeat.release(Point{330,634});
        for(int i=0;i<24000&&!defeat.ended();++i) defeat.tick();
        require(defeat.ended()&&!defeat.won()&&defeat.battleStatus().playerStatue==0,"Garrisoned Insane army did not reach defeat");
        for(int i=0;i<120;++i) defeat.tick();defeat.drawList();
        std::cout<<"Defeat tick "<<defeat.ticks()<<", saved level "<<restored->level<<", map "<<map.mapFrame()<<'\n';
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
