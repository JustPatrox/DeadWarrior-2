#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <iostream>
#include <stdexcept>

using namespace sw2;

int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        art.loadTextures(dir/"tutorial"); art.loadTextures(dir/"archidon");
        art.loadTextures(dir/"interaction");
        art.loadTextures(dir/"interaction");
        CampaignProgress progress;
        progress.finish(true,220,1);
        progress.upgrades.set(2); progress.points=0;
        CampaignTutorial rage(art,dir,progress);
        rage.press({70,400}); rage.point(Point{240,560}); rage.release(Point{240,560});
        rage.unitAbility(); rage.unitAbility();
        if (rage.battleStatus().activeRage!=2 || rage.battleStatus().fighterHealth!=150)
            throw std::runtime_error("Rage did not charge the original health cost once per selected fighter");
        progress.upgrades.reset(2); progress.points=1;
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool test,const char* text){if (!test) throw std::runtime_error(text);};
        auto click=[&](Point p){scene.press(p); scene.release(p);};
        auto advance=[&](int ticks){for (int i=0;i<ticks;++i) scene.tick();};
        auto initial=scene.battleStatus();
        require(initial.playerStatue==1000 && initial.enemyStatue==800 && initial.gold==700 && initial.mana==200 &&
                initial.miners==3 && initial.fighters==2 && initial.population==8 && scene.camera()==850,"Initial armies differ from the original");
        click({88,610}); click({56,665});
        auto queued=scene.battleStatus();
        require(queued.fightersQueued==2 && queued.gold==100 && queued.mana==150 && queued.population==12,"Mixed barracks queue failed");
        click({110,595});
        require(scene.battleStatus().fightersQueued==1 && scene.battleStatus().gold==250 &&
                scene.battleStatus().population==11,"Cancelling the fighter refunded the wrong unit");
        click({80,646});
        require(scene.battleStatus().fightersQueued==0 && scene.battleStatus().gold==700 &&
                scene.battleStatus().mana==200 && scene.battleStatus().population==8,"Spearton refund failed");
        click({56,665}); click({88,610});
        advance(1200);
        scene.drawList();
        auto trained=scene.battleStatus();
        std::cout<<"Trained lancers "<<trained.lancers<<", enemies "<<trained.enemyArchers
                 <<", queue "<<trained.fightersQueued<<", tower "<<trained.towerControl<<'\n';
        require(trained.lancers==1 && trained.fightersQueued==0 && trained.enemyArchers>0,"Training or enemy archers failed");
        click({500,634});
        for (int i=0;i<18000 && !scene.ended();++i) {
            scene.tick();
            if (i%120==0) scene.drawList();
            if (i%450==0) click({500,634});
            if (i%360==0 && scene.battleStatus().gold>=150) click({88,610});
        }
        auto end=scene.battleStatus();
        std::cout<<"Tick "<<scene.ticks()<<", shots "<<end.mobileShots<<", statues "<<end.playerStatue<<" / "
                 <<end.enemyStatue<<", tower "<<end.towerControl<<", ended "<<scene.ended()<<'\n';
        require(end.mobileShots>0,"Mobile Archidons did not shoot");
        require(scene.ended(),"No outcome after the attacking battle");
        advance(120); scene.drawList();
        require(scene.showingResults(),"Second battle did not open results");
        if (scene.won()) {
            progress.finish(true,scene.ticks()/30,scene.reward());
            CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
            require(map.mapFrame()==50,"Third territory did not start at the previous flag");
            for (int i=0;i<42;++i) {map.tick(); map.drawList();}
            require(map.mapFrame()==92 && progress.level==2,"Third territory did not reach its authored stop");
            std::cout<<"Campaign level "<<progress.level<<", map frame "<<map.mapFrame()<<'\n';
            art.loadTextures(dir/"shadow");
            CampaignTutorial shadow(art,dir,progress);
            auto state=shadow.battleStatus();
            require(state.archers==1 && state.miners==3 && state.playerStatue==1200 && state.enemyStatue==1200,
                    "Invalid third mission deployment");
            auto command=[&](Point p){shadow.press(p); shadow.release(p);};
            command({146,610});
            int ninjas=0,poisoned=0;
            auto sample=[&]{
                const auto state=shadow.battleStatus();
                ninjas=std::max(ninjas,state.enemyNinjas);
                poisoned=std::max(poisoned,state.poisonedAllies);
            };
            for (int i=0;i<2400;++i) {shadow.tick(); sample();}
            require(shadow.battleStatus().archers>=2,"Archery range did not train an allied Archidon");
            for (int i=0;i<18000 && !shadow.ended();++i) {
                if (i%360==0) {command({56,665}); command({88,610}); command({500,634});}
                shadow.tick();
                sample();
                if (i%120==0) shadow.drawList();
            }
            require(shadow.ended(),"Shadowrath battle did not reach an outcome");
            require(ninjas>0 && poisoned>0,"Shadowrath cloak attack did not poison an ally");
            for (int i=0;i<120;++i) shadow.tick();
            shadow.drawList();
            std::cout<<"Shadowrath tick "<<shadow.ticks()<<" victory "<<shadow.won()
                     <<" ninjas "<<ninjas<<" poisoned "<<poisoned<<'\n';
            if (shadow.won()) {
                progress.finish(true,shadow.ticks()/30,shadow.reward());
                CampaignIntermission fourth(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
                for (int i=0;i<59;++i) {fourth.tick(); fourth.drawList();}
                require(fourth.mapFrame()==151,"Fourth territory not reached");
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
