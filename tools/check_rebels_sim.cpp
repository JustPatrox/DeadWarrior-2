#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <iostream>
#include <stdexcept>

using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for (const char* folder:{"tutorial","archidon","shadow","magic","rebels"}) art.loadTextures(dir/folder);
        art.loadTextures(dir/"interaction");
        art.loadTextures(dir/"interaction");
        CampaignProgress progress; progress.level=4; progress.points=4;
        progress.upgrades.set(13); progress.upgrades.set(17);
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if (!value) throw std::runtime_error(reason);};
        auto click=[&](Point p){scene.press(p); scene.release(p);};
        auto state=scene.battleStatus();
        require(state.population==24 && state.miners==4 && state.fighters==5 && state.ninjas==1 &&
                state.playerStatue==1500 && state.enemyStatue==1500,"Rebels United deployment differs from XML");
        click({203,610});
        require(scene.battleStatus().mana==0 && scene.battleStatus().gold==400,"Meric training price differs from XML");
        click({225,595});
        require(scene.battleStatus().mana==200 && scene.battleStatus().gold==700,"Meric cancellation failed");
        scene.selectArmy();
        scene.unitAbility();
        scene.point(Point{300,450});
        require(scene.cursorSymbol()==8785,"Original explosion cursor missing");
        click({300,450});
        for (int i=0;i<90;++i) scene.tick();
        scene.unitAbility(1);
        click({300,450});
        for (int i=0;i<90;++i) scene.tick();
        scene.unitAbility(2);
        scene.point(Point{300,450});
        require(scene.cursorSymbol()==8800,"Original electric wall cursor missing");
        click({300,450});
        state=scene.battleStatus();
        for (int count:state.spells) require(count==1,"An allied targeted spell failed or cast more than once");
        for (int i=0;i<24000 && !scene.ended();++i) {
            if (i%360==0) {click({56,665}); click({88,610}); click({146,610}); click({500,634});}
            scene.tick();
            if (i%120==0) scene.drawList();
        }
        state=scene.battleStatus();
        std::cout<<"Rebels tick "<<scene.ticks()<<" won "<<scene.won()<<" statues "<<state.playerStatue<<" / "<<state.enemyStatue
                 <<" spell hits "<<state.magicHits<<" heals "<<state.heals<<'\n';
        require(scene.ended(),"Rebels United did not reach an outcome");
        for (int i=0;i<120;++i) scene.tick();
        scene.drawList();
        if (scene.won()) {
            progress.finish(true,scene.ticks()/30,scene.reward());
            CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
            for (int i=0;i<60;++i) {map.tick(); map.drawList();}
            require(map.mapFrame()==298,"Map did not reach Massive Battle");
        }
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
