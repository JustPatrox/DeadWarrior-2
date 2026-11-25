#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <iostream>
#include <stdexcept>

using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void fundSpellCoverage(CampaignTutorial& scene) {
        // Explicit prepared economy, after unmodified deployment/cost checks.
        // The normal AI must still recruit, train and use its own Magikill.
        scene.enemyGold_=600;scene.enemyMana_=600;
    }
};
}
int main(int argc,char** argv) {
    try {
        const auto dir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(dir/"menu");
        for (const char* folder:{"tutorial","archidon","shadow","magic"}) art.loadTextures(dir/folder);
        art.loadTextures(dir/"interaction");
        art.loadTextures(dir/"interaction");
        CampaignProgress progress; progress.level=3; progress.points=3;
        CampaignTutorial scene(art,dir,progress);
        auto require=[](bool value,const char* reason){if (!value) throw std::runtime_error(reason);};
        auto click=[&](Point p){scene.press(p); scene.release(p);};
        auto initial=scene.battleStatus();
        require(initial.miners==2 && initial.fighters==1 && initial.gold==700 && initial.mana==200 &&
                initial.playerStatue==700 && initial.enemyStatue==1200 && initial.healers==1,"Fourth mission deployment differs from XML");
        click({117,665});
        require(scene.battleStatus().gold==250 && scene.battleStatus().mana==50,"Shadowrath training price differs from XML");
        click({140,646});
        require(scene.battleStatus().gold==700 && scene.battleStatus().mana==200,"Shadowrath cancellation did not refund its price");
        CampaignProbe::fundSpellCoverage(scene);
        click({117,665}); click({88,610});
        int wizards=0,hits=0,heals=0,castleShots=0;
        for (int i=0;i<24000 && !scene.ended();++i) {
            if (i>6500 && i%360==0) {
                click({56,665}); click({88,610}); click({500,634});
                scene.selectArmy();
                scene.unitAbility(); scene.unitAbility();
            }
            scene.tick();
            const auto state=scene.battleStatus();
            wizards=std::max(wizards,state.wizards);
            hits=state.magicHits; heals=state.heals; castleShots=state.enemyCastleShots;
            if (i%120==0) scene.drawList();
        }
        auto end=scene.battleStatus();
        std::cout<<"Magic tick "<<scene.ticks()<<" won "<<scene.won()<<" statues "<<end.playerStatue<<" / "<<end.enemyStatue
                 <<" wizards "<<wizards<<" hits "<<hits<<" heals "<<heals<<" castle shots "<<castleShots<<'\n';
        require(wizards>0 && hits>0,"Prepared enemy spellcasters did not attack");
        for (int count:end.spells) require(count>0,"A Magikill spell never activated");
        require(scene.ended(),"Fourth mission did not reach an outcome");
        for (int i=0;i<120;++i) scene.tick();
        scene.drawList();
        CampaignTutorial attack(art,dir,progress);
        auto order=[&](Point p){attack.press(p); attack.release(p);};
        order({117,665}); order({88,610});
        for (int i=0;i<10000 && !attack.ended();++i) {
            if (i>1500 && i%360==0) {
                order({56,665}); order({88,610}); order({500,634});
                attack.selectArmy();
                const int mana=attack.battleStatus().mana;
                attack.unitAbility(); attack.unitAbility();
                if (i==1800) require(attack.battleStatus().mana==mana-20,"Cloak charged the wrong mana or ignored its cooldown");
            }
            attack.tick();
        }
        require(attack.won(),"Attacking formation did not win the fourth mission");
        require(attack.battleStatus().enemyCastleShots>0,"The unmodified mission's castle archer did not fire");
        for (int i=0;i<120;++i) attack.tick();
        attack.drawList();
        progress.finish(true,attack.ticks()/30,attack.reward());
        CampaignIntermission map(art,dir/"intermission",progress,CampaignIntermission::Page::Map);
        for (int i=0;i<87;++i) {map.tick(); map.drawList();}
        require(map.mapFrame()==238,"Map did not reach Rebels United");
        std::cout<<"Attack won at tick "<<attack.ticks()<<", map "<<map.mapFrame()<<'\n';
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n'; return 1;}
}
