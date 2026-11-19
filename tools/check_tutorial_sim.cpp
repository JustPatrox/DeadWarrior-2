#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace sw2;

// Drive the same controller as the window in discrete 30 Hz ticks. Transitions
// must come from actual selection, unit positions, death and resource production.
int main(int argc,char** argv) {
    try {
        const auto assetsDir=std::filesystem::path(argc>1 ? argv[1] : "assets");
        MenuAssets art(assetsDir/"menu");
        art.loadTextures(assetsDir/"tutorial");
        art.loadTextures(assetsDir/"interaction");
        art.loadTextures(assetsDir/"interaction");
        CampaignTutorial scene(art,assetsDir/"tutorial",Difficulty::Normal);
        auto advance=[&](int count){for (int i=0;i<count;++i) scene.tick();};
        auto until=[&](int step,int limit) {
            for (int i=0;i<limit && scene.stepNumber()!=step;++i) scene.tick();
            if (scene.stepNumber()!=step)
                throw std::runtime_error("Expected state "+std::to_string(step)+", reached "+std::to_string(scene.stepNumber()));
            std::cout<<"Step "<<step<<" tick "<<scene.ticks()<<" camera "<<scene.camera()<<'\n';
        };
        advance(180);
        scene.press({580,260}); scene.point(Point{720,510}); scene.release(Point{720,510});
        until(2,10); advance(120);
        scene.command({150,420});
        until(3,300); advance(60);
        scene.point(Point{Viewport::right-5,165});
        until(4,300); scene.point(Point{425,225}); advance(120);
        scene.point(Point{3350-scene.camera(),430});
        if (scene.cursorSymbol()!=2313) throw std::runtime_error("Attack pointer missing");
        scene.drawList();
        scene.command({3350-scene.camera(),430});
        until(5,900); advance(90);
        scene.press({350,600}); scene.release(Point{350,600});
        until(6,400); advance(120);
        Point worker{1250-scene.camera(),430};
        scene.press(worker); scene.release(worker); until(7,160); advance(100);
        scene.point(Point{1350-scene.camera(),320});
        if (scene.cursorSymbol()!=8821) throw std::runtime_error("Prayer pointer missing");
        scene.drawList();
        scene.command({1350-scene.camera(),320});
        until(8,220); until(9,600); advance(150);
        scene.point(Point{1590-scene.camera(),490});
        if (scene.cursorSymbol()!=8824) throw std::runtime_error("Mining pointer missing");
        scene.drawList();
        scene.command({1590-scene.camera(),490}); until(10,200); until(11,400);
        scene.press({95,610}); scene.release(Point{95,610});
        until(12,20); until(13,800);
        auto click=[&](Point p){scene.press(p); scene.release(p);};
        auto require=[](bool test,const char* message){if (!test) throw std::runtime_error(message);};
        until(14,600); advance(160);
        click({350,662});
        require(scene.defenceStatus().garrisoned==0,"Missed garrison button accepted input");
        click({350,634});
        require(scene.defenceStatus().garrisoned==4,"Garrison did not recall all four units");
        until(15,10); until(16,1200); advance(180);
        click({460-scene.camera(),170});
        require(!scene.defenceStatus().rangeSelected,"Building accepted input outside its silhouette");
        click({532-scene.camera(),170});
        until(17,180); advance(180);
        require(scene.camera()>=-Viewport::left && scene.camera()<=-Viewport::left+3,
            "Range camera exposed terrain behind the castle");
        const auto funds=scene.defenceStatus();
        require(funds.gold==400 && funds.rangeSelected,"Range or starting research balance incorrect");
        click({675,606}); click({675,606});
        auto state=scene.defenceStatus();
        require(state.gold==100 && state.mana==funds.mana && state.researchTicks==300,
                "Castle archer purchase did not charge the original cost exactly once");
        until(18,10); advance(299);
        require(!scene.defenceStatus().castleArcher,"Castle archer appeared before research finished");
        until(19,400);
        require(scene.defenceStatus().castleArcher,"Completed research failed to place castle archer");
        advance(160);
        const auto exposed=scene.battleStatus();
        require(exposed.playerStatue>0 && exposed.playerStatue<1500 &&
                int(1500-exposed.playerStatue)%17==0,
                "The invading Spearton failed to land original 17-point hits on the exposed statue");
        click({425,637});
        require(scene.defenceStatus().garrisoned==0,"Defend left units in garrison");
        until(20,20); until(21,1500); until(22,400);
        state=scene.defenceStatus();
        require(state.shots>0 && state.impacts>0,"Castle archer failed to hit the invading Spearton");
        require(state.gold>=500,"Enemy defeat failed to fund the free battle");
        std::cout<<"Castle archer shots "<<state.shots<<", direct impacts "<<state.impacts
                 <<", gold "<<state.gold<<", mana "<<state.mana<<'\n';
        const auto before=scene.battleStatus();
        click({26,610}); click({88,610}); click({88,610});
        auto battle=scene.battleStatus();
        require(battle.minersQueued==1 && battle.fightersQueued==2 && battle.gold==before.gold-450 &&
                battle.population==before.population+4,"The bank and barracks failed to reserve gold and population");
        advance(72); click({110,595});
        battle=scene.battleStatus();
        require(battle.fightersQueued==1 && battle.minersQueued==1 && battle.gold>=before.gold-300 &&
                battle.population==before.population+3,"Cancel did not refund the last queued fighter");
        advance(169);
        require(scene.battleStatus().minersQueued==1,"Miner appeared early");
        advance(1);
        require(scene.battleStatus().miners==before.miners+1 && scene.battleStatus().minersQueued==0,
                "Bank did not produce the miner in 242 ticks");
        advance(119);
        require(scene.battleStatus().fightersQueued==1,"Swordwrath appeared early");
        advance(1);
        require(scene.battleStatus().fighters==before.fighters+1 && scene.battleStatus().fightersQueued==0,
                "Removing the last order reset progress on the first one");
        battle=scene.battleStatus();
        click({350,634}); require(scene.defenceStatus().garrisoned==battle.miners+battle.fighters,
                                  "Free-battle garrison order did not recall the whole army");
        click({425,637}); require(!scene.defenceStatus().garrisoned,"Free-battle defence order failed");
        click({88,610}); click({500,634});
        until(28,14000);
        battle=scene.battleStatus();
        require(battle.enemyStatue==0 && battle.playerStatue>0 && battle.reinforcement,
                "Victory did not come from destroying the reinforced enemy statue");
        std::cout<<"Victory: enemy statue "<<battle.enemyStatue<<", player statue "<<battle.playerStatue
                 <<", reinforcement "<<battle.reinforcement<<'\n';
        advance(87); require(!scene.showingResults(),"Results appeared before collapse completed");
        advance(1); require(scene.showingResults() && scene.won(),"Victory screen failed to open");
        scene.drawList();
        using Action=CampaignTutorial::ResultAction;
        click({580,653});
        require(scene.takeResultAction()==Action::None,"Unlock card left the Continue button exposed");
        click({425,490}); advance(30);
        scene.press({580,653}); scene.release(Point{265,653});
        require(scene.takeResultAction()==Action::None,"Drag between result buttons activated a link");
        click({265,653});
        require(scene.takeResultAction()==Action::Online,"Online button missed its release");
        click({580,653});
        require(scene.takeResultAction()==Action::Continue && scene.takeResultAction()==Action::None,
                "Continue did not produce one result action");
        CampaignTutorial hard(art,assetsDir/"tutorial",Difficulty::Hard);
        hard.tick(); hard.press({442,137}); hard.release(Point{442,137});
        require(hard.stepNumber()==27 && hard.battleStatus().gold==500,"Hard-mode skip failed to enter free battle");
        hard.press({350,634}); hard.release(Point{350,634});
        require(hard.defenceStatus().garrisoned==6,"Hard-mode skip did not recall all six starting units");
        for (int i=0;i<180000 && hard.battleStatus().playerStatue>0;++i) hard.tick();
        hard.tick();
        auto loss=hard.battleStatus();
        std::cout<<"Garrison-only hard battle, tick "<<hard.ticks()<<", player statue "<<loss.playerStatue
                 <<", enemy statue "<<loss.enemyStatue<<'\n';
        require(hard.stepNumber()==29 && loss.enemyStatue>0,"Opposing army failed to destroy player statue");
        for (int i=0;i<88;++i) hard.tick();
        require(hard.showingResults() && !hard.won(),"Defeat screen failed to open");
        hard.drawList();
        hard.press({425,417}); hard.release(Point{425,417});
        for (int i=0;i<30;++i) hard.tick();
        hard.press({580,653}); hard.release(Point{580,653});
        require(hard.takeResultAction()==Action::Retry,"Retry button missed its release");
        CampaignProgress progress;
        progress.finish(false,300,scene.reward());
        progress.finish(true,220,scene.reward());
        require(progress.level==1 && progress.points==1 && progress.records[0].attempts==2 &&
                progress.records[0].totalSeconds==520 && progress.records[0].bestSeconds==220,
                "Campaign did not retain both mission times");
        CampaignIntermission board(art,assetsDir/"intermission",progress,CampaignIntermission::Page::Upgrades);
        auto choose=[&](Point p){board.point(p); board.press(); board.release();};
        choose({580,403}); require(progress.points==1,"Blocked technology spent a point");
        choose({750,664}); require(board.confirming(),"Unspent point did not prompt");
        choose({425,493}); require(board.confirming() && progress.points==1,"Covered tree accepted a purchase");
        choose({510,381}); choose({425,493}); choose({425,493});
        require(progress.points==0 && progress.upgrades[2],"Rage did not cost exactly one point");
        board.drawList(); choose({750,664});
        require(board.page()==CampaignIntermission::Page::Map && board.mapFrame()==1,"Map entry is misaligned");
        for (int i=1;i<50;++i) {board.drawList(); board.tick();}
        require(board.mapFrame()==50 && !progress.autoSave,"Map did not stop at level2");
        choose({309,644});
        auto events=board.takeEvents();
        require(progress.autoSave && std::count(events.begin(),events.end(),CampaignIntermission::Event::Save)==1,
                "Enabling auto-save did not emit one save");
        board.saved(true); choose({309,644}); require(progress.autoSave,"Save modal leaked clicks");
        choose({435,390}); choose({309,644}); require(!progress.autoSave,"Auto-save toggle did not disable");
        std::stringstream bytes; progress.write(bytes);
        auto loaded=CampaignProgress::read(bytes);
        require(loaded && loaded->level==1 && loaded->points==0 && loaded->records[0].totalSeconds==520 &&
                loaded->upgrades==progress.upgrades,"Campaign save round trip lost data");
        auto saved=bytes.str(); std::istringstream truncated(saved.substr(0,saved.size()/2));
        require(!CampaignProgress::read(truncated),"Truncated campaign was accepted");
        std::cout<<"Upgrade, map frame "<<board.mapFrame()<<", save and resume verified\n";
        CampaignTutorial controls(art,assetsDir/"tutorial",Difficulty::Normal);
        controls.press({580,260}); controls.point(Point{720,510}); controls.release(Point{720,510});
        auto chooseUnit=[&](Point p){controls.press(p); controls.release(p);};
        chooseUnit({813,642}); controls.point(Point{150,420});
        require(controls.cursorSymbol()==2313,"Attack-move button did not activate its original pointer");
        controls.command({150,420});
        require(controls.cursorSymbol()==0,"Right click did not cancel attack-move targeting");
        chooseUnit({675,678});
        require(controls.defenceStatus().garrisoned==2,"Selected-unit garrison command failed");
        chooseUnit({675,678});
        require(controls.defenceStatus().garrisoned==0,"Selected-unit exit command failed");
        controls.command({410,600});
        const auto draws=controls.drawList();
        require(std::any_of(draws.begin(),draws.end(),[](const Draw& draw){return draw.symbol==2310;}),
                "Minimap command did not produce the original movement marker");
        require(!controls.takeSounds().empty(),"Unit orders produced no original sound events");
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<e.what()<<'\n'; return 1;
    }
}
