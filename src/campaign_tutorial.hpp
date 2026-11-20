#pragma once
#include "campaign_progress.hpp"
#include <deque>

namespace sw2 {
// CampaignTutorial owns the 30 Hz simulation. Its coordinates use the original
// 850-wide game rules even when the renderer exposes extra terrain on either side.
class CampaignTutorial {
    friend struct CampaignProbe;
public:
    CampaignTutorial(const MenuAssets&,const std::filesystem::path&,Difficulty,int retries=0);
    CampaignTutorial(const MenuAssets&,const std::filesystem::path& assetsRoot,const CampaignProgress&);
    void tick();
    void point(std::optional<Point>);
    void press(Point);
    void release(std::optional<Point>);
    void command(Point);
    void unitAbility(int slot=0);
    void selectArmy(bool center=false);
    void minerOrder(bool);
    void pcPressed(bool value) {pcPressed_=value;}
    std::vector<Draw> drawList() const;
    std::string takeVoice();
    struct SoundEvent {std::string name; Point position{}; bool spatial{true};};
    std::vector<SoundEvent> takeSounds();
    int cursorSymbol() const;
    int stepNumber() const { return cinematic_>=0 && !ended() ? 200+cinematic_ : static_cast<int>(step_); }
    std::string takeUnavailableControl();
    double camera() const { return camera_; }
    int ticks() const { return ticks_; }
    std::optional<Point> cursor() const { return cursor_; }
    struct DefenceStatus {
        int gold{},mana{},garrisoned{},researchTicks{},shots{},impacts{};
        bool rangeSelected{},castleArcher{};
    };
    DefenceStatus defenceStatus() const;
    struct BattleStatus {
        int gold{},mana{},population{},minersQueued{},fightersQueued{},miners{},fighters{};
        double playerStatue{},enemyStatue{};
        bool reinforcement{};
        int lancers{},archers{},enemyArchers{},mobileShots{};
        double towerControl{50};
        double fighterHealth{};
        int activeRage{},enemyNinjas{},poisonedAllies{},ninjas{},wizards{},healers{},magicHits{},heals{},enemyCastleShots{};
        std::array<int,3> spells{};
        int giants{},giantHits{},cinematic{-1},dialogue{-1};
        double giantHealth{};
        int bombers{},bomberWaves{},nextBomberWave{},explosions{},boulderShots{},boulderHits{},enslavedGiants{};
        int enemyKnights{},selectedMilitary{};
        int knightHint{-1};
        double furthestMilitary{};
        int poisonHits{};
    };
    BattleStatus battleStatus() const;
    struct UnitStatus {int symbol{};Point position;bool enemy{};double health{};int target{};std::string pose;int frame{},cloaked{};bool garrisoned{};};
    std::vector<UnitStatus> unitStatus() const;
    bool showingResults() const;
    bool ended() const {return step_>=Step::Victory;}
    bool won() const {return step_==Step::Victory;}
    int reward() const { return int(rules_.at("campaign.points")); }
    bool fastForward() const {return fastForward_ && step_==Step::Free && cinematic_<0;}
    enum class ResultAction { None,Online,Retry,Continue };
    ResultAction takeResultAction();
private:
    static constexpr double legacyWidth=850;
    enum class Step { Select=1, Move, Scroll, Attack, Return, Miner, Pray, PrayInfo, Mine, GoldInfo,
                      Train, Training, Enemy, Garrison, Retreat, Range, Upgrade, Research, Defend, Fight,
                      Commands, Objective, AttackWait, AttackHint, QualityWait, QualityHint, Free, Victory, Defeat };
    enum class Order { Stand, Move, Attack, Gather };
    struct Unit {
        int symbol{}, frame{1}, facing{1};
        Point position{}, velocity{}, goal{};
        double hp{}, maxHp{}, width{}, height{};
        bool enemy{}, selected{}, hit{},garrisoned{},engageOnMove{},deadCounted{},hold{};
        bool canFight{true};
        int stunned{},targetsHit{},tint{-1};
        double altitude{};
        Order order{Order::Stand};
        int target{-1};
        int resource{-2},miningSlot{-1},bag{};
        int bowFrame{1};
        int rageCounter{200};
        int furyStacks{},lastHitTick{},lastVictim{-1},cloakTicks{},cloakCooldown{};
        std::array<int,3> spellCooldown{};
        int spell{},spellTarget{-1};
        Point spellPoint{};
        double poison{};
        int healTicks{};
        double healAmount{};
        bool autoHeal{true},autoCure{};
        double bowAngle{};
        std::string pose{"stand"};
    };
    struct Animation { int symbol{},length{}; Point origin{}; int frameBase{}; };
    struct Message { std::string step,text; double seconds{}; };
    const MenuAssets& art_;
    Difficulty difficulty_;
    std::map<std::pair<int,std::string>,Animation> animations_;
    std::map<std::pair<int,int>,Point> tips_;
    std::map<std::pair<int,int>,Point> bows_;
    std::map<std::string,double> rules_;
    std::map<int,Message> messages_;
    std::array<int,56> seed_{};
    int randomA_{},randomB_{21};
    std::vector<Unit> units_;
    std::vector<SoundEvent> sounds_;
    unsigned soundSequence_{};
    std::map<int,int> cursorFrames_;
    struct Tooltip {int frame{},delay{};};
    std::map<int,Tooltip> tooltips_;
    int hoveredTooltip_{},tooltipHoverTicks_{},shownTooltip_{},tooltipRemaining_{};
    struct CastlePart {int symbol{};Point origin{};Rect hit;};
    std::vector<CastlePart> castleParts_;
    std::map<int,std::vector<Rect>> castleMasks_;
    bool castleHit(const CastlePart&,Point) const;
    struct CastleLayer {int owner{},state{},depth{},base{},prefix{},period{},length{};Draw::Blend blend{};};
    std::vector<CastleLayer> castleLayers_;
    std::map<int,std::pair<int,int>> castleState_;
    int pressedCastle_{};
    bool pcPressed_{};
    int poisonHits_{};
    int selectedBuilding_{};
    struct CommandTarget {enum Kind {Move,Mine,Pray,Enemy}; Kind kind{Move}; int id{-1};};
    struct ClickMarker {Point position; int frame{1};};
    std::vector<ClickMarker> clickMarkers_;
    std::vector<Point> gold_;
    std::vector<int> oreRemaining_;
    std::vector<int> mineOrder_;
    Step step_{Step::Select};
    int ticks_{},voiceTicks_{},displayed_{},checkFrame_{1};
    int goldBalance_{},mana_{},population_{4},counter_{};
    int enemyGold_{},enemyMana_{},enemyPopulation_{};
    struct Training {int symbol{}; double gold{},mana{};};
    struct ProductionQueue {std::deque<Training> units; int elapsed{};};
    std::array<std::array<ProductionQueue,6>,2> production_{};
    std::array<Unit,2> statues_{};
    Unit wall_;
    bool reinforcement_{};
    bool canSkip_{},resultCard_{},closingCard_{},pressedCard_{},fastForward_{};
    double cardAlpha_{};
    int unlockCard_{};
    struct Sample { int playerWorkers{},enemyWorkers{},playerArmy{},enemyArmy{}; };
    std::vector<Sample> history_;
    ResultAction pressedResult_{ResultAction::None},resultAction_{ResultAction::None};
    bool cardHit(Point) const;
    enum class ArmyOrder { Garrison,Defend,Attack };
    std::array<ArmyOrder,2> orders_{ArmyOrder::Defend,ArmyOrder::Defend};
    std::array<double,2> median_{};
    static constexpr int ownStatue=-2,opposingStatue=-3,ownWall=-4;
    static constexpr int sword=4922,miner=3104,spearton=6456,archer=4236,ninja=7289,wizard=7926,healer=8233,
        giant=7025,chaosMiner=7323,enslavedGiant=7526,medusa=8061,dead=8529,bomber=8774,knight=5724;
    static bool isWorker(int symbol) {return symbol==miner || symbol==chaosMiner;}
    int mission_{},ownStatueFrame_{1},enemyStatueFrame_{10},mobileShots_{},hint_{},hintTicks_{};
    bool chaosOpponent_{};
    std::vector<int> backgroundLayers_{4102,4098};
    double towerControl_{50};
    std::bitset<CampaignProgress::upgradeCount+1> upgrades_;
    bool trainingAvailable_{};
    bool transitioning_{true},mapDown_{};
    double messageX_{-600},camera_{2200},targetCamera_{2200},length_{},depth_{},battlefieldY_{};
    std::optional<Point> cursor_,dragStart_;
    std::string nextVoice_;
    std::string unavailableControl_;
    std::map<std::string,std::vector<Rect>> controlMasks_;
    struct Flight {
        double x{},depth{},z{},dx{},dy{},dz{},angle{},previousDistance{};
        int target{-1},age{};
        bool struck{},hidden{};
        bool enemy{},castle{true};
        bool guts{};
        double poison{},spin{};
    };
    std::vector<Flight> flights_;
    int invader_{-1},researchTicks_{-1},bowFrame_{1},archerFrame_{1},shots_{},impacts_{};
    bool rangeSelected_{},researchStarted_{},castleArcher_{},defending_{};
    double bowAngle_{};
    int enemyBowFrame_{1},enemyArcherFrame_{1},enemyCastleShots_{};
    double enemyBowAngle_{};
    bool enemyCastleArcher_{};
    int enemyCastleResearchTicks_{-1};
    struct EnemyBuild {int symbol{},building{};std::string name;};
    std::vector<EnemyBuild> enemyBuild_;
    bool knightHintTriggered_{};
    int knightHintTicks_{-1};
    struct MagicEffect {int symbol{},frame{1}; Point start{},end{}; bool enemy{}; double depth{},damage{-1};};
    std::vector<MagicEffect> magic_;
    int magicHits_{},heals_{};
    std::array<int,3> spells_{};
    int aimedSpell_{};
    bool aimedAttackMove_{};
    Unit castleDead_;
    int giantHits_{},cinematic_{-1},cinematicTicks_{},dialogueIndex_{-1},dialogueTicks_{},medusaIndex_{-1};
    double sceneShade_{};
    struct Dialogue {std::string voice,text; int ticks{};};
    std::vector<Dialogue> dialogue_;
    std::vector<int> rebels_;
    std::map<std::pair<int,int>,bool> giantHitFrames_;
    int nextBomberWave_{2},bomberWaves_{},explosions_{},boulderShots_{},boulderHits_{};
    struct BoulderFlight {
        double x{},depth{},z{},dx{},dy{},dz{},rotation{},spin{},size{1},previousDistance{};
        Point drawPosition{};
        double drawDepth{},drawRotation{};
        bool enemy{},debris{},struck{},hidden{};
        int target{-1},age{};
    };
    std::vector<BoulderFlight> boulders_;
    static constexpr Rect minimap{305.2,578.05,231,44};
    int random();
    int spawn(int symbol,Point p,bool enemy=false);
    double scale(const Unit&) const;
    double stat(const Unit&,const char*) const;
    double stat(int symbol,const char*) const;
    void animate(Unit&,const std::string&);
    void updateUnit(Unit&);
    CommandTarget pointerTarget(Point) const;
    void drawPointer(std::vector<Draw>&) const;
    void sound(std::string name,Point position={},bool spatial=true);
    void impactSound(const Unit&);
    Point workMiner(Unit&);
    void updateMessage();
    int requestedMessage() const;
    void updateSelection(Point);
    void setFormation(Point world);
    void moveFormation(const std::vector<Unit*>& group,Point world);
    bool hitControl(const char* name,Point p) const;
    bool defencePress(Point);
    void teamOrder(bool garrison);
    void orderArmy(bool enemy,ArmyOrder,double destination);
    Unit& combatTarget(int id);
    bool reserveOre(Unit&,int ore);
    void assignFreeMine(Unit&);
    bool productionPress(Point);
    void drawProduction(std::vector<Draw>&) const;
    void updateProduction();
    bool recruit(bool enemy,int building,bool cancel=false,int type=0);
    int recruitType(bool enemy,int building) const;
    void simulateActors(bool strategy);
    void updateBattle(bool strategy);
    void recordArmies();
    void skipTutorial();
    void tickOutcome();
    std::vector<Draw> drawOutcome() const;
    ResultAction resultHit(Point) const;
    void updateOpponent();
    void spawnGroup(bool enemy,const std::vector<int>& symbols);
    void updateDefence();
    void advanceDefence();
    double frontLine() const;
    double visionBoundary() const;
    bool visible(const Unit&) const;
    void drawTooltips(std::vector<Draw>&) const;
    int tooltipAt() const;
    void updateTooltip();
    void updateCastleArcher(bool enemy=false);
    void advanceArrow(Flight&);
    void drawCastleArcher(std::vector<std::pair<double,Draw>>&,bool enemy=false) const;
    void drawArrows(std::vector<std::pair<double,Draw>>&) const;
    void configureCampaign(const std::filesystem::path&,const CampaignProgress&);
    void loadGiants(const std::filesystem::path&,bool cinematic=true);
    void updateGiants();
    void updateCinematic();
    void speak(int);
    void createCinematic();
    void giantStrike(Unit&);
    void updateCastleDead();
    void drawCastleDead(std::vector<std::pair<double,Draw>>&) const;
    void drawCinematic(std::vector<Draw>&) const;
    void updateBomberController();
    void detonate(Unit&);
    Point updateEnslavedGiant(Unit&);
    void throwBoulder(Unit&);
    void updateBoulders();
    void drawBoulders(std::vector<std::pair<double,Draw>>&) const;
    void updateKnightController();
    void createMiningBlocks();
    void updateArchidon();
    Point updateArcher(Unit&);
    void drawArcher(std::vector<std::pair<double,Draw>>&,const Unit&) const;
    void updateMobileArrows();
    bool updateCaster(Unit&);
    bool startSpell(Unit&,int,Point);
    void castAt(Point);
    void updateMagic();
    void drawMagic(std::vector<std::pair<double,Draw>>&) const;
    void applyMagicDamage(Unit&,double,bool);
    Draw rotatedSprite(int symbol,int frame,Point origin,double size,double angle) const;
    Draw sprite(int symbol,int frame,Point origin,double scale=1,bool mirror=false) const;
};
}
