#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for (const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","interaction"}) art.loadTextures(dir/folder);
        CampaignProgress progress; progress.level=5; progress.points=5;
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if (!value) throw std::runtime_error(reason);};
        CampaignTutorial input(art,dir,progress);
        input.press({535,600}); input.release(Point{535,600});
        for (int i=0;i<100;++i) input.tick();
        require(!input.fastForward() && input.camera()>4949 && input.camera()<=6000-Viewport::right,
            "Transparent speed-button corner intercepted the minimap");
        input.press({554,582}); input.release(Point{554,582});
        require(input.fastForward(),"Speed button's visible face did not respond");
        auto click=[&](Point p){scene.press(p); scene.release(p);};
        auto state=scene.battleStatus();
        require(state.population==7 && state.miners==3 && state.fighters==1 && state.giants==1 &&
            state.playerStatue==1500 && state.enemyStatue==180,"Massive Battle deployment differs from CampaignCutScene1/XML");
        click({310,600}); for (int i=0;i<80;++i) scene.tick();
        click({550-scene.camera(),300}); click({675,606});
        require(scene.defenceStatus().researchTicks>=0,"Castle Archer research was not available");
        int start=-1,poisoned=0,previousDialogue=-1,voices=0;
        for (int i=0;i<24000 && !scene.ended();++i) {
            if (i%300==0 && scene.battleStatus().cinematic<0) {
                click({146,610}); click({88,610});
                if (i>1500) click({500,634});
            }
            scene.tick();
            state=scene.battleStatus(); poisoned=std::max(poisoned,state.poisonedAllies);
            if (i%600==0) std::cout<<i<<" gold "<<state.gold<<" archers "<<state.archers<<" fighters "<<state.fighters
                <<" giant "<<state.giantHealth<<" statue "<<state.playerStatue<<'\n';
            if (state.cinematic>=0 && start<0) {start=scene.ticks(); require(!scene.ended(),"Giant death skipped the cinematic");}
            if (state.dialogue!=previousDialogue) {
                require(state.dialogue==previousDialogue+1,"Dialogue played out of sequence");
                previousDialogue=state.dialogue;
                const auto voice=scene.takeVoice(); require(!voice.empty(),"Dialogue voice missing"); ++voices;
                require(!scene.fastForward(),"Fast forward remained active during the cinematic");
                if(state.dialogue<=2) {
                    const auto list=scene.drawList();
                    const auto medusa=std::find_if(list.begin(),list.end(),[](const Draw& d){return d.symbol==8050;});
                    require(medusa!=list.end(),"Medusa stand pose missing during her dialogue");
                    // Original CampaignCutScene1: py125, flyingHeight380;
                    // Unit.updateMotion: scale1.2 * perspective(.5 + .3/2)=.78.
                    const auto bounds=art.texture(medusa->symbol,medusa->frame).bounds;
                    const double originalTop=320+125-296.4+(-306.05+bounds.y)*.78;
                    require(std::abs(medusa->target.y-originalTop)<.001,
                        "Medusa flight omitted her original unit scale and sank behind the battlement");
                    const double barLeft=5050-scene.camera()-30*1.3*.78/2;
                    require(std::any_of(list.begin(),list.end(),[&](const Draw& d){return !d.symbol && d.kind==Draw::Kind::Box &&
                        d.color==0 && std::abs(d.target.x-barLeft)<.001 && d.target.y+d.target.h<medusa->target.y;}),
                        "Medusa health bar did not follow her airborne position");
                }
                const auto gold=state.gold;
                click({88,610}); click({500,634}); scene.unitAbility();
                require(scene.battleStatus().gold==gold,"Training remained active during the cinematic");
            }
            if (i%90==0) scene.drawList();
        }
        state=scene.battleStatus();
        std::cout<<"Giants tick "<<scene.ticks()<<" won "<<scene.won()<<" cinematic "<<start<<" / "<<state.cinematic
            <<" hits "<<state.giantHits<<" castle shots "<<state.enemyCastleShots<<" poisoned "<<poisoned<<" voices "<<voices<<'\n';
        require(scene.won(),"Massive Battle did not reach victory");
        require(voices==7 && scene.ticks()-start>=1900,"Cinematic duration or dialogue count differs");
        require(state.giantHits>0,"Giant never delivered an area strike");
        for (int i=0;i<120;++i) scene.tick();
        scene.drawList(); require(scene.showingResults(),"Giant unlock results missing");
        click({425,490}); for (int i=0;i<30;++i) scene.tick();
        progress.finish(true,scene.ticks()/30,scene.reward());
        std::stringstream save; progress.write(save); const auto restored=CampaignProgress::read(save);
        require(restored && restored->level==6 && restored->points==6,"Sixth mission save failed");
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for (int i=0;i<60;++i) {map.tick(); map.drawList();}
        require(map.mapFrame()==341,"Map did not reach Explosive War");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
