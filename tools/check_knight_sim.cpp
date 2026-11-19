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
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","interaction"}) art.loadTextures(dir/folder);
        CampaignProgress progress;progress.level=7;progress.points=1;
        for(int upgrade:{2,4,7,11,16,20}) progress.upgrades.set(upgrade);
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if(!value) throw std::runtime_error(reason);};
        auto click=[&](Point p){scene.press(p);scene.release(p);};
        auto state=scene.battleStatus();
        require(state.miners==4&&state.fighters==3&&state.population==11&&state.enemyKnights==1&&state.bombers==2&&state.gold==700&&state.mana==200,"Juggerknight deployment differs from XML");
        scene.selectArmy();require(scene.battleStatus().selectedMilitary==3,"SPACE selection included miners or lost soldiers");
        click({310,600});for(int i=0;i<80;++i) scene.tick();
        click({550-scene.camera(),300});click({675,606});
        require(scene.defenceStatus().researchTicks>=0,"Castle research unavailable");
        bool healer=false,hint=false;int maxKnights=1,maxGiants=0,hintDuration=0;
        for(int i=0;i<26000&&!scene.ended();++i) {
            state=scene.battleStatus();
            if(i%120==0) {
                if(!healer&&state.gold>=300&&state.mana>=200) {click({200,610});healer=true;}
                if(state.miners+state.minersQueued<16) click({30,610});
                if(state.archers<3&&state.gold>=200) click({146,610});
                if(state.miners>=14&&state.gold>=1500&&state.enslavedGiants<4) click({230,665});
                click({state.enslavedGiants>=3 ? 500. : 425.,634});
            }
            scene.tick();state=scene.battleStatus();
            maxKnights=std::max(maxKnights,state.enemyKnights);maxGiants=std::max(maxGiants,state.enslavedGiants);
            const auto draws=scene.drawList();
            const bool visible=std::any_of(draws.begin(),draws.end(),[](const Draw& d){return d.text=="Press SPACE to select all of your attacking units";});
            if(visible) {hint=true;++hintDuration;scene.selectArmy();require(scene.battleStatus().selectedMilitary==state.fighters,"Army shortcut omitted an active attacking unit");}
            if(i%900==0) std::cout<<scene.ticks()<<" gold "<<state.gold<<" knights "<<state.enemyKnights<<" giants "<<state.enslavedGiants<<" boulders "<<state.boulderShots<<'/'<<state.boulderHits<<" statues "<<state.playerStatue<<'/'<<state.enemyStatue<<'\n';
        }
        state=scene.battleStatus();
        std::cout<<"Knight tick "<<scene.ticks()<<" won "<<scene.won()<<" recruited "<<maxKnights<<" giants "<<maxGiants<<" hint "<<hintDuration<<" castle shots "<<state.enemyCastleShots<<'\n';
        require(scene.won(),"Juggerknights did not reach victory");
        require(maxKnights>1&&maxGiants>0&&state.boulderHits>0,"Enemy recruitment or allied Giant attacks missing");
        require(hint&&hintDuration==152,"CampaignKnight instruction duration differs from its controller");
        require(state.enemyCastleShots>0,"Chaos castle research never activated its Dead defender");
        for(int i=0;i<120;++i) scene.tick();scene.drawList();
        progress.finish(true,scene.ticks()/30,scene.reward());std::stringstream data;progress.write(data);const auto restored=CampaignProgress::read(data);
        require(restored&&restored->level==8&&restored->points==2,"Eighth mission save did not restore");
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for(int i=0;i<90;++i) {map.tick();map.drawList();}
        require(map.mapFrame()==441,"Map did not reach Deadly Archers");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
