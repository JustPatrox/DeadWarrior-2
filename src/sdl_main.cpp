#include "pc_settings_layout.hpp"
#include "sdl_platform.hpp"
#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include "pause_menu.hpp"
#include "work_in_progress.hpp"
#include "intro_movie.hpp"
#include "startup_splash.hpp"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace {
using namespace sw2;
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
constexpr auto interval=std::chrono::nanoseconds(1'000'000'000/30);
class Application {
    SDL_Window* window_;
    fs::path root_,assetsDir_,saveDir_;
    std::ofstream log_;
    MenuAssets art_;
    CampaignMenu menu_;
    SdlRenderer renderer_;
    SdlAudio audio_;
    IntroMovie intro_;
    IntroClock introClock_;
    bool introPlaying_{};
    SdlDisplay display_;
    PauseMenu pause_;
    CampaignProgress progress_;
    std::optional<CampaignProgress> saved_;
    std::unique_ptr<CampaignTutorial> battle_;
    std::unique_ptr<CampaignIntermission> campaign_;
    std::unique_ptr<WorkInProgress> wip_;
    SDL_Texture* buffer_{};
    int bufferWidth_{},bufferHeight_{},lastStep_{-1},lastLogTick_{};
    bool running_{true},notice_{true},pauseButton_{},pcButton_{},optionPending_{};
    Clock::time_point noticeEnd_{Clock::now()+std::chrono::seconds(3)},bannerEnd_{},lastArmySelect_{};
    std::string banner_;
    double fps_{};
    void log(const std::string& text) {log_<<SDL_GetTicks()<<' '<<text<<std::endl;}
    void loadSave() {std::ifstream file(saveDir_/"campaign.sav");saved_=CampaignProgress::read(file);}
    bool save() {
        auto path=saveDir_/"campaign.sav",temp=saveDir_/"campaign.sav.tmp";std::ofstream out(temp);progress_.write(out);out.close();
        std::error_code error;if(out) fs::rename(temp,path,error);
        const bool ok=out&&!error;if(ok) saved_=progress_;log(ok ? "SAVE success" : "SAVE failed");return ok;
    }
    void unsupported(std::string control) {
        log("UNAVAILABLE "+control);banner_="This button does not work yet, please report it with the log";bannerEnd_=Clock::now()+std::chrono::seconds(6);
    }
    void paused(bool value) {pause_.setOpen(value);audio_.paused(value||display_.isOpen());if(battle_) {battle_->release({});battle_->point({});}}
    void toggleSettings() {
        display_.setOpen(!display_.isOpen());audio_.paused(display_.isOpen()||pause_.isOpen());
        if(introPlaying_)introClock_.pause(display_.isOpen());
        if(battle_) {battle_->point({});battle_->release({});}
    }
    void mainMenu() {
        battle_.reset();campaign_.reset();wip_.reset();paused(false);progress_={};loadSave();menu_.restart(saved_.has_value());audio_.music("loginMusic");log("SCENE main-menu");
    }
    void unfinished() {
        battle_.reset();campaign_.reset();wip_=std::make_unique<WorkInProgress>(art_);paused(false);audio_.stopNarration();audio_.music("loginMusic");
        log("SCENE work-in-progress level="+std::to_string(progress_.level+1));
    }
    void startBattle() {
        if(progress_.level>=CampaignProgress::playableLevelCount) {unfinished();return;}
        paused(false);campaign_.reset();wip_.reset();
        if(progress_.level==0) battle_=std::make_unique<CampaignTutorial>(art_,assetsDir_/"tutorial",progress_.difficulty,progress_.records[0].attempts);
        else battle_=std::make_unique<CampaignTutorial>(art_,assetsDir_,progress_);
        audio_.stopNarration();audio_.music("orderInGame");lastStep_=-1;lastLogTick_=0;
        log("MISSION start level="+std::to_string(progress_.level+1));
    }
    void openCampaign(CampaignIntermission::Page page) {
        if(progress_.level>CampaignProgress::playableLevelCount) {unfinished();return;}
        battle_.reset();wip_.reset();campaign_=std::make_unique<CampaignIntermission>(art_,assetsDir_/"intermission",progress_,page);
        audio_.stopNarration();audio_.music("loginMusic");log("SCENE campaign");
    }
    void campaignEvents() {
        using E=CampaignIntermission::Event;
        for(auto event:campaign_->takeEvents()) {
            if(event==E::StartLevel) {startBattle();return;}
            if(event==E::Save) campaign_->saved(save());
            else if(event==E::Click) audio_.effect("clickButton",0,false);
            else if(event==E::Equip) audio_.effect("ArmoryEquipSound",0,false);
            else if(event==E::Arrived) audio_.effect("SelectRaceSound",0,false);
            else if(event==E::Online) SDL_OpenURL("https://www.stickempires.com");
            else if(event==E::Guide) unsupported("campaign guide");
        }
    }
    void stopIntro(bool resumeMenu) {
        if(!introPlaying_)return;
        introPlaying_=false;intro_.stop();menu_.setIntroPixels({});
        audio_.stopMovieEffects();audio_.stopMusic();audio_.paused(false);log("INTRO stopped");
        if(resumeMenu)audio_.music("loginMusic");
    }
    void syncIntro() {
        const bool wanted=menu_.page()==Page::Intro&&!battle_&&!campaign_&&!wip_;
        if(!wanted) {stopIntro(!battle_&&!campaign_&&!wip_);return;}
        if(!introPlaying_) {
            if(!intro_.available()) {log("INTRO resources unavailable; continuing campaign");menu_.finishIntro();menuActions();return;}
            intro_.start();introClock_.begin();introPlaying_=true;
            audio_.musicFile(intro_.music());menu_.setIntroPixels(intro_.pixels());
            log("INTRO started fps=30 first=3 last=3235");
        }
    }
    void updateIntro() {
        if(!introPlaying_)return;
        intro_.advance(introClock_.seconds(audio_.musicPlaying(),audio_.musicSeconds(),intro_.duration()));
        for(const auto& file:intro_.takeEffects())audio_.effectFile(file);
        menu_.setIntroPixels(intro_.pixels());
        if(intro_.finished()) {log("INTRO completed source_frame=3235");menu_.finishIntro();menuActions();}
    }
    void menuActions() {
        for(auto action:menu_.takeActions()) switch(action) {
            case Action::ClickSound:audio_.effect("clickButton",0,false);break;
            case Action::MusicToggle:audio_.channels(menu_.music(),menu_.music());break;
            case Action::Map:stopIntro(false);progress_={};progress_.difficulty=menu_.difficulty();startBattle();break;
            case Action::LoadCampaign:if(saved_) {progress_=*saved_;if(progress_.level>=CampaignProgress::playableLevelCount) unfinished();else openCampaign(CampaignIntermission::Page::Map);}break;
            case Action::Online:SDL_OpenURL("https://www.stickempires.com");break;
            case Action::FirstGame:SDL_OpenURL("https://www.stickpage.com/stickwargame.shtml");break;
            case Action::Stickpage:SDL_OpenURL("https://www.stickpage.com");break;
            case Action::IntroLink:break;
            case Action::Unavailable:unsupported("main menu");break;
            default:break;
        }
        syncIntro();
    }
    void pauseAction(PauseMenu::Action action) {
        using A=PauseMenu::Action;
        if(action==A::Resume) paused(false);
        else if(action==A::Restart) startBattle();
        else if(action==A::MainMenu) mainMenu();
        else if(action==A::Sound||action==A::Music) {
            const bool sound=audio_.soundEnabled(),music=audio_.musicEnabled();
            audio_.channels(action==A::Sound ? !sound : sound,action==A::Music ? !music : music);
            menu_.setMusic(audio_.soundEnabled()||audio_.musicEnabled());
        }
    }
    void wipAction(WorkInProgress::Choice choice) {
        using C=WorkInProgress::Choice;
        if(choice==C::None) return;
        if(choice==C::Quit) {running_=false;return;}
        if(choice==C::RestartDelete) {std::error_code error;fs::remove(saveDir_/"campaign.sav",error);if(error) {unsupported("delete save failed");return;}}
        else if(!save()) {unsupported("restart save failed");return;}
        mainMenu();audio_.stopMusic();notice_=true;noticeEnd_=Clock::now()+std::chrono::seconds(3);
    }
public:
    Application(SDL_Window* window,fs::path root,fs::path saves)
        :window_(window),root_(std::move(root)),assetsDir_(root_/"assets"),saveDir_(std::move(saves)),
         log_(saveDir_/"lastest.log",std::ios::trunc),art_(assetsDir_/"menu"),menu_(art_),renderer_(window,art_,root_),
         audio_(assetsDir_/"audio",[this](std::string text){log(text);}),intro_(assetsDir_/"intro",loadSdlPng,true),display_(window,renderer_.native(),saveDir_/"options.ini"),pause_(art_) {
        for(const char* folder:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","deads","crawlers","interaction","wip","pause"}) art_.loadTextures(assetsDir_/folder);
        loadSave();menu_.restart(saved_.has_value());
        if(auto icon=SDL_LoadPNG((root_/"icon.png").string().c_str())) {SDL_SetWindowIcon(window_,icon);SDL_DestroySurface(icon);}
        log(std::string("START platform=")+SDL_GetPlatform()+" renderer="+SDL_GetRendererName(renderer_.native()));
    }
    ~Application() {display_.save();if(buffer_) SDL_DestroyTexture(buffer_);log("SHUTDOWN");}
    void pointer(int action,std::optional<Point> p) {
        if(action==1||action==3) optionPending_=false;
        if(action==1) SDL_CaptureMouse(true);if(action==2) SDL_CaptureMouse(false);
        if(display_.isOpen()) {display_.pointer(action,p);return;}
        if(notice_) return;
        if(pause_.isOpen()) {pause_.point(p);if(action==1) pause_.press();if(action==2) pauseAction(pause_.release());return;}
        if(wip_) {wip_->point(p);if(action==1) wip_->press();if(action==2) wipAction(wip_->release());return;}
        if(campaign_) {campaign_->point(p);if(action==1) campaign_->press();if(action==2) {campaign_->release();campaignEvents();}return;}
        if(battle_) {
            p=Viewport::battlePoint(p);
            battle_->point(p);
            if(action==1) battle_->toolbarMouseDown(true);
            if(action==2) battle_->toolbarMouseDown(false);
            const Rect pc{320,674,48,22};
            if(action==1 && p && !battle_->ended() && pc.contains(*p)) {pcButton_=true;battle_->pcPressed(true);return;}
            if(action==2 && pcButton_) {pcButton_=false;battle_->pcPressed(false);if(p&&pc.contains(*p)) toggleSettings();return;}
            if(action==1&&p&&!battle_->ended()&&battle_->toolbarControlHit("menuButton",*p)) {pauseButton_=true;return;}
            if(action==2&&pauseButton_) {pauseButton_=false;if(p&&battle_->toolbarControlHit("menuButton",*p)) paused(true);return;}
            if(p&&action==1) {battle_->press(*p);const auto missing=battle_->takeUnavailableControl();if(!missing.empty()) unsupported(missing);}
            if(p&&action==3) battle_->command(*p);
            if(action==2) {
                battle_->release(p);const auto result=battle_->takeResultAction();
                if(result==CampaignTutorial::ResultAction::Retry) startBattle();
                else if(result==CampaignTutorial::ResultAction::Continue) openCampaign(CampaignIntermission::Page::Upgrades);
                else if(result==CampaignTutorial::ResultAction::Online) SDL_OpenURL("https://www.stickempires.com");
            }
            return;
        }
        const int target=p ? renderer_.hit(menu_,*p) : 0;menu_.hover(target);
        if(action==1) menu_.press(target);if(action==2) menu_.release(target);menuActions();
    }
    void key(SDL_Keycode key) {
        if constexpr(optionSettingsKey) {
            if(key==SDLK_LALT||key==SDLK_RALT) {
                optionPending_=!(SDL_GetModState()&(SDL_KMOD_CTRL|SDL_KMOD_SHIFT|SDL_KMOD_GUI));return;
            }
            optionPending_=false;
        } else if(key==SDLK_INSERT) {toggleSettings();return;}
        if(key==SDLK_F11) {display_.fullscreen();return;}
        if(display_.isOpen()) {if(key==SDLK_ESCAPE) {display_.setOpen(false);audio_.paused(pause_.isOpen());if(introPlaying_)introClock_.pause(false);}else display_.key(key);return;}
        if(introPlaying_&&key==SDLK_ESCAPE) {menu_.finishIntro();menuActions();return;}
        if(pause_.isOpen()) {
            int code=key==SDLK_ESCAPE||key==SDLK_P ? 27 : key==SDLK_UP ? 38 : key==SDLK_DOWN ? 40 : key==SDLK_LEFT ? 37 : key==SDLK_RIGHT ? 39 : key==SDLK_TAB ? 9 : key==SDLK_RETURN ? 13 : key==SDLK_SPACE ? 32 : 0;
            pauseAction(pause_.key(code));return;
        }
        if(battle_&&!battle_->ended()&&(key==SDLK_ESCAPE||key==SDLK_P)) {paused(true);return;}
        if(wip_) {
            if(key==SDLK_TAB||key==SDLK_DOWN||key==SDLK_RIGHT) wip_->focus(1);
            else if(key==SDLK_UP||key==SDLK_LEFT) wip_->focus(-1);
            else if(key==SDLK_RETURN||key==SDLK_SPACE) wipAction(wip_->selected());
        } else if(battle_&&key==SDLK_SPACE) {const auto now=Clock::now();battle_->selectArmy(now-lastArmySelect_<std::chrono::milliseconds(400));lastArmySelect_=now;}
        else if(battle_&&(key==SDLK_Q||key==SDLK_W||key==SDLK_E)) battle_->unitAbility(key==SDLK_Q ? 0 : key==SDLK_W ? 1 : 2);
    }
    void keyRelease(SDL_Keycode key) {
        // Option is a modifier on macOS. A standalone tap is the menu shortcut;
        // Option+another key or a mouse click keeps its usual application use.
        if constexpr(optionSettingsKey) if(key==SDLK_LALT||key==SDLK_RALT) {
            const bool toggle=optionPending_;optionPending_=false;if(toggle) toggleSettings();
        }
    }
    void tick() {
        if(notice_) {
            if(Clock::now()<noticeEnd_) return;
            notice_=false;audio_.music("loginMusic");
        }
        if(display_.isOpen()||pause_.isOpen()||wip_) return;
        if(battle_) {
            const bool ended=battle_->ended(),results=battle_->showingResults();battle_->tick();
            for(const auto& event:battle_->takeSounds()) audio_.effect(event.name,(event.position.x-battle_->camera()-425)/Viewport::width,event.spatial);
            const auto voice=battle_->takeVoice();if(!voice.empty()) {
                auto file=assetsDir_/"tutorial"/(voice+".wav");if(!fs::exists(file)) file=assetsDir_/"giants"/(voice+".wav");audio_.narration(file);
            }
            if(!ended&&battle_->ended()) {audio_.stopNarration();audio_.effect("StatueDestroyed",0,false);}
            if(!results&&battle_->showingResults()) {progress_.finish(battle_->won(),battle_->ticks()/30,battle_->reward());audio_.music(battle_->won() ? "OrderVictory" : "OrderDefeat");}
            if(lastStep_!=battle_->stepNumber()||battle_->ticks()-lastLogTick_>=900) {
                lastStep_=battle_->stepNumber();lastLogTick_=battle_->ticks();const auto state=battle_->battleStatus();
                log("BATTLE level="+std::to_string(progress_.level+1)+" step="+std::to_string(lastStep_)+" tick="+std::to_string(lastLogTick_)+" statues="+std::to_string(state.playerStatue)+"/"+std::to_string(state.enemyStatue));
            }
        } else if(campaign_) {campaign_->tick();campaignEvents();}else {menu_.tick();menuActions();updateIntro();}
    }
    void draw(int width,int height,const fs::path& snapshot={}) {
        display_.configureViewport(width,height);
        auto native=renderer_.native();const auto [w,h]=snapshot.empty() ? display_.renderSize(width,height) : std::pair{width,height};
        if(!buffer_||w!=bufferWidth_||h!=bufferHeight_) {
            if(buffer_) SDL_DestroyTexture(buffer_);buffer_=SDL_CreateTexture(native,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);
            if(!buffer_) throw std::runtime_error(SDL_GetError());bufferWidth_=w;bufferHeight_=h;
        }
        SDL_SetRenderTarget(native,buffer_);
        std::vector<Draw> draws;
        const auto splash=startupSplash(startupDuration-std::chrono::duration<double>(noticeEnd_-Clock::now()).count());
        const double logoWidth=splash.engine ? 480 : 372;
        const double logoHeight=logoWidth*(splash.engine ? 442./1014 : 52./380);
        const double splashTop=(Viewport::height-38-16-logoHeight)/2;
        if(notice_) {
            auto label=Draw::label({Viewport::left, splashTop,Viewport::width,38},splash.engine ? "Powered by" : "This project is not affiliated with",24,0xffffff,true);
            label.alpha=splash.alpha;draws.push_back(std::move(label));
        }
        else if(wip_) draws=wip_->drawList();else if(campaign_) draws=campaign_->drawList();else draws=battle_ ? battle_->drawList() : menu_.drawList();
        if(pause_.isOpen()&&!display_.isOpen()) {
            auto panel=pause_.drawList(audio_.soundEnabled(),audio_.musicEnabled());
            if constexpr(optionSettingsKey) for(auto& item:panel) {
                const auto at=item.text.find("Insert: PC settings");
                if(at!=std::string::npos) item.text.replace(at,6,settingsKeyLabel);
            }
            draws.insert(draws.end(),panel.begin(),panel.end());
        }
        if(!banner_.empty()&&Clock::now()<bannerEnd_) {
            draws.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left+24,15,Viewport::width-48,46},0,.94));
            draws.push_back(Draw::label({Viewport::left+34,25,Viewport::width-68,30},banner_,19,0xffdc82,true));
        }
        if(display_.showFps()&&!notice_) draws.push_back(Draw::label({Viewport::left+16,12,120,24},"FPS: "+std::to_string(int(std::lround(fps_))),17,0x8dff8d));
        display_.append(draws);renderer_.draw(draws,w,h);
        if(notice_) renderer_.picture(assetsDir_/(splash.engine ? "branding/stick-engine-2.png" : "branding/max-games.png"),
            {425-logoWidth/2,splashTop+54,logoWidth,logoHeight},Viewport(w,h),splash.alpha);
        if(!snapshot.empty()) renderer_.png(snapshot);
        SDL_SetRenderTarget(native,nullptr);SDL_SetRenderDrawColor(native,0,0,0,255);SDL_RenderClear(native);SDL_RenderTexture(native,buffer_,nullptr,nullptr);SDL_RenderPresent(native);
    }
    void show() {
        noticeEnd_=Clock::now()+std::chrono::seconds(3);
        // Present the initial black frame before Cocoa makes the Metal window
        // visible. Smoke checks must exercise the same startup path as play.
        int firstW,firstH;SDL_GetRenderOutputSize(renderer_.native(),&firstW,&firstH);
        SDL_Log("STARTUP priming hidden window %dx%d",firstW,firstH);
        draw(firstW,firstH);
        SDL_Log("STARTUP showing native window");
        SDL_ShowWindow(window_);
        SDL_Log("STARTUP native window shown");
    }
    int run() {
        show();
        auto deadline=Clock::now(),frameDeadline=deadline,countStart=deadline;int count=0;
        while(running_) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT) running_=false;
                else if(event.type==SDL_EVENT_KEY_DOWN&&!event.key.repeat) key(event.key.key);
                else if(event.type==SDL_EVENT_KEY_UP) keyRelease(event.key.key);
                else if(event.type==SDL_EVENT_WINDOW_MOUSE_LEAVE) pointer(0,{});
                else if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST) {optionPending_=false;pcButton_=false;if(battle_) battle_->pcPressed(false);if(battle_&&!battle_->ended()) paused(true);}
                else if(event.type==SDL_EVENT_MOUSE_MOTION||event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP) {
                    const bool motion=event.type==SDL_EVENT_MOUSE_MOTION;
                    const Point mouse{motion ? double(event.motion.x) : double(event.button.x),motion ? double(event.motion.y) : double(event.button.y)};
                    int w,h,pw,ph;SDL_GetWindowSize(window_,&w,&h);SDL_GetRenderOutputSize(renderer_.native(),&pw,&ph);
                    display_.configureViewport(pw,ph);
                    const auto stage=Viewport(pw,ph).toStage({mouse.x*pw/std::max(w,1),mouse.y*ph/std::max(h,1)});
                    pointer(motion ? 0 : event.type==SDL_EVENT_MOUSE_BUTTON_UP ? 2 : event.button.button==SDL_BUTTON_RIGHT ? 3 : 1,stage);
                }
            }
            int clientW,clientH;SDL_GetWindowSize(window_,&clientW,&clientH);display_.configureViewport(clientW,clientH);
            const bool confine=display_.mouseLock() && battle_ && !battle_->ended() && !pause_.isOpen() &&
                !display_.isOpen() && !notice_ && (SDL_GetWindowFlags(window_)&SDL_WINDOW_INPUT_FOCUS);
            SDL_SetWindowMouseGrab(window_,confine);
            if(confine) {
                const Viewport view(clientW,clientH);SDL_Rect bounds{int(view.pixels.x),int(view.pixels.y),int(view.pixels.w),int(view.pixels.h)};
                SDL_SetWindowMouseRect(window_,&bounds);
            } else SDL_SetWindowMouseRect(window_,nullptr);
            const auto now=Clock::now();
            // Keep each 30 Hz step, including when a presentation frame is slow.
            if((battle_&&(pause_.isOpen()||display_.isOpen()))||notice_) {deadline=now+interval;if(notice_) tick();}
            for(int batch=0;batch<8&&deadline<=now;++batch) {tick();deadline+=interval;}
            if(now>=frameDeadline) {
                int w,h;SDL_GetRenderOutputSize(renderer_.native(),&w,&h);if(w>0&&h>0) draw(w,h);
                const bool cursor=battle_&&!pause_.isOpen()&&!display_.isOpen()&&battle_->cursorSymbol();if(cursor) SDL_HideCursor();else SDL_ShowCursor();
                ++count;if(now-countStart>=std::chrono::seconds(1)) {fps_=count/std::chrono::duration<double>(now-countStart).count();count=0;countStart=now;}
                const int limit=notice_ ? 60 : display_.frameLimit();
                frameDeadline=now+std::chrono::nanoseconds(limit ? 1'000'000'000/limit : 0);
            } else SDL_Delay(1);
        }
        return 0;
    }
    void selfTest(const fs::path& out) {
        fs::create_directories(out);auto require=[](bool value,const char* reason){if(!value) throw std::runtime_error(reason);};
        const bool spriteSheets=std::any_of(art_.art.begin(),art_.art.end(),[](const auto& entry){return entry.second.region.has_value();});
        require(spriteSheets,"Prepared smoke-test assets contain no sprite sheets");
        require(audio_.testMix(),"Audio decode/playback failed");
        draw(1280,720,out/"notice.png");notice_=false;
        for(int i=0;i<200;++i) tick();require(menu_.page()==Page::Main,"Menu did not finish opening");draw(1280,720,out/"main.png");
        require(Viewport::aspect==Viewport::Aspect::Dynamic,"Fresh options did not default to Dynamic");
        draw(686,720,out/"menu-portrait.png");
        int menuButtons=0;
        for(const auto& h:menu_.hits())if(h.bounds.y<Viewport::height&&h.bounds.y+h.bounds.h>0) {
            require(h.symbol!=1009&&h.symbol!=1010&&h.symbol!=1015&&h.symbol!=1021&&h.symbol!=1018&&h.symbol!=1027,"Inactive menu panel visible in portrait window");
            if(h.symbol==1002||h.symbol==708||h.symbol==1005)++menuButtons;
        }
        require(menuButtons==3,"Portrait menu lost its main controls");draw(1280,720);
        auto click=[&](Point p){if(display_.isOpen()) {const auto shift=pcSettings::offset();p.x+=shift.x;p.y+=shift.y;}pointer(0,p);pointer(1,p);pointer(2,p);};
        click({161,664});for(int i=0;i<50;++i) tick();require(menu_.page()==Page::NewOrContinue,"Native menu input failed");
        click({437,664});for(int i=0;i<50;++i) tick();require(menu_.page()==Page::Difficulty,"New campaign input failed");
        click({154,668});for(int i=0;i<100;++i) tick();require(menu_.page()==Page::Intro,"Difficulty input failed");
        require(introPlaying_&&intro_.pixels(),"Authored intro did not autoplay");
        for(const auto& hit:menu_.hits())require(hit.symbol!=998&&hit.symbol!=1044,"Intro retained an invisible image link");
        const int introBefore=intro_.sourceFrame();SDL_Delay(220);tick();
        require(intro_.sourceFrame()>introBefore,"Audio presentation clock did not advance the intro");
        const bool losslessIntro=intro_.statistics().codecDecodes>0;
        draw(1280,720,out/"intro.png");
        toggleSettings();const int pausedFrame=intro_.sourceFrame();const auto pausedSamples=audio_.musicSamplesPlayed();
        SDL_Delay(180);for(int i=0;i<8;++i)tick();
        require(intro_.sourceFrame()==pausedFrame&&audio_.musicSamplesPlayed()==pausedSamples,"PC settings did not pause movie pixels and audio");
        key(SDLK_ESCAPE);require(!display_.isOpen()&&introPlaying_,"Closing PC settings skipped the movie");
        SDL_Delay(120);tick();require(intro_.sourceFrame()>pausedFrame,"Movie did not resume after PC settings");
        click({434,639});for(int i=0;i<80;++i) tick();require(bool(battle_),"Skip intro did not open tutorial");
        require(!introPlaying_&&!intro_.pixels(),"Skip retained cinematic pixels or playback state");
        for(int i=0;i<40;++i) tick();draw(1280,720,out/"tutorial.png");
        click({650,397});require(battle_->battleStatus().selectedMilitary==1,"Single click selected overlapping units");
        pointer(1,Point{580,260});pointer(0,Point{720,510});pointer(2,Point{720,510});
        require(battle_->battleStatus().selectedMilitary==2,"Held rectangle did not select both tutorial soldiers");
        const int before=battle_->ticks();click({506,686});require(pause_.isOpen(),"HUD MENU did not open pause");
        for(int i=0;i<60;++i) tick();require(battle_->ticks()==before,"Paused simulation advanced");draw(1280,720,out/"pause.png");
        if constexpr(optionSettingsKey) {
            key(SDLK_LALT);key(SDLK_A);keyRelease(SDLK_LALT);
            require(!display_.isOpen(),"Option combination opened PC settings");
        }
        key(settingsKeyCode);keyRelease(settingsKeyCode);require(display_.isOpen(),"Platform settings shortcut failed");draw(1280,720,out/"settings.png");
        auto settingsCentered=[&]{
            std::vector<Draw> panel;display_.append(panel);
            const auto border=std::find_if(panel.begin(),panel.end(),[](const Draw& d){return d.kind==Draw::Kind::Polygon;});
            require(border!=panel.end() && std::abs(border->target.x+border->target.w/2-425)<.001 &&
                    std::abs(border->target.y+border->target.h/2-Viewport::height/2)<.001,"PC settings panel is not centered");
        };
        settingsCentered();
        if constexpr(optionSettingsKey) {
            key(SDLK_RALT);keyRelease(SDLK_RALT);require(!display_.isOpen(),"Right Option did not close settings");
            key(settingsKeyCode);keyRelease(settingsKeyCode);require(display_.isOpen(),"Option did not reopen settings");
        }
        key(SDLK_END);key(SDLK_RETURN);require(display_.showFps(),"FPS switch failed");
        key(SDLK_DOWN);key(SDLK_RETURN);require(display_.mouseLock(),"Mouse lock setting failed");
        key(SDLK_DOWN);key(SDLK_RETURN);display_.configureViewport(1280,720);
        require(Viewport::aspect==Viewport::Aspect::Fixed,"Saved 16:9 option is unavailable after Dynamic default");
        key(SDLK_RETURN);display_.configureViewport(1024,768);
        require(Viewport::aspect==Viewport::Aspect::Stretched,"Stretched mode input failed");draw(1024,768,out/"aspect-stretched.png");
        key(SDLK_RETURN);display_.configureViewport(2560,1080);
        require(Viewport::aspect==Viewport::Aspect::Dynamic && std::abs(Viewport::width/Viewport::height-2560./1080)<.0001,"Dynamic aspect input failed");
        draw(2560,1080,out/"aspect-ultrawide.png");draw(720,1280,out/"aspect-portrait.png");settingsCentered();
        {
            SdlDisplay restored(window_,renderer_.native(),saveDir_/"options.ini");restored.configureViewport(1024,768);
            require(restored.mouseLock() && Viewport::aspect==Viewport::Aspect::Dynamic,"Mouse/aspect settings did not restore");
        }
        key(SDLK_RETURN);key(SDLK_UP);key(SDLK_RETURN);require(!display_.mouseLock(),"Mouse lock did not disable");
        auto waitWindowMode=[&](bool fullscreen) {
            const Uint64 deadline=SDL_GetTicks()+10000;
            int w=0,h=0;SDL_Rect monitor{};
            do {
                SDL_PumpEvents();SDL_GetWindowSize(window_,&w,&h);
                const bool bounds=SDL_GetDisplayBounds(SDL_GetDisplayForWindow(window_),&monitor);
                const bool flag=bool(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN);
                if(flag==fullscreen && (!fullscreen || (bounds && std::abs(w-monitor.w)<=2 && std::abs(h-monitor.h)<=2))) {
                    log("SMOKE display mode="+std::to_string(display_.displayMode())+" client="+std::to_string(w)+"x"+std::to_string(h));return;
                }
                SDL_Delay(10);
            } while(SDL_GetTicks()<deadline);
            throw std::runtime_error("Window transition timed out mode="+std::to_string(display_.displayMode())+
                " flags="+std::to_string(SDL_GetWindowFlags(window_))+" client="+std::to_string(w)+"x"+std::to_string(h)+
                " monitor="+std::to_string(monitor.w)+"x"+std::to_string(monitor.h)+" SDL="+SDL_GetError());
        };
        SDL_ClearError();click({425,300});waitWindowMode(true);draw(1280,720,out/"borderless.png");
        SDL_ClearError();click({425,243});waitWindowMode(false);
        key(SDLK_ESCAPE);key(SDLK_ESCAPE);
        require(!pause_.isOpen(),"Pause did not resume");for(int i=0;i<15;++i) tick();require(battle_->ticks()==before+15,"Resume changed simulation rate");
        for(int level=1;level<CampaignProgress::playableLevelCount;++level) {
            progress_={};progress_.level=level;progress_.points=level;
            if(level==1) {progress_.upgrades.set(6);progress_.upgrades.set(10);}
            startBattle();for(int i=0;i<60;++i) tick();
            if(level==1) {
                click({344,683});require(display_.isOpen(),"PC toolbar button did not open settings");key(SDLK_ESCAPE);
                pointer(0,Point{30,610});for(int i=0;i<4;++i) tick();
                const auto list=battle_->drawList();require(std::any_of(list.begin(),list.end(),[](const Draw& d){return d.symbol==1119;}),"Original tooltip did not appear");
                draw(1280,720,out/"original-tooltip.png");
                click({310,600});for(int i=0;i<100;++i)tick();
                click({550-battle_->camera(),300});
                for(int tier=1;tier<=3;++tier) {
                    const int cost=200+tier*100;
                    for(int i=0;i<6000&&battle_->battleStatus().gold<cost;++i)tick();
                    require(!battle_->ended()&&battle_->battleStatus().gold>=cost,"Castle research fixture could not afford its next tier");
                    click({675,606});require(battle_->defenceStatus().researchLevel==tier,"Unlocked Castle Archer tier did not begin research");
                    for(int i=0;i<1000&&battle_->defenceStatus().castleArchers<tier;++i)tick();
                    require(battle_->defenceStatus().castleArchers==tier,"Castle Archer tier did not finish");
                    draw(1280,720,out/("castle-tier-"+std::to_string(tier)+".png"));
                }
            }
            if(level==7) {
                key(SDLK_SPACE);keyRelease(SDLK_SPACE);
                require(battle_->battleStatus().selectedMilitary==3,"SPACE did not select the three starting soldiers");
                pointer(0,Point{813,642});
                auto hover=battle_->drawList();require(std::any_of(hover.begin(),hover.end(),[](const Draw& d){return d.symbol==10009&&d.frame==71001;}),"Original action hover state failed");
                pointer(1,Point{813,642});auto pressed=battle_->drawList();
                require(std::any_of(pressed.begin(),pressed.end(),[](const Draw& d){return d.symbol==10009&&d.frame==72001;}),"Original action pressed state failed");
                pointer(2,Point{813,642});
            }
            draw(1280,720,out/("mission-"+std::to_string(level+1)+"-720.png"));draw(1920,1080,out/("mission-"+std::to_string(level+1)+"-1080.png"));
        }
        progress_.level=8;startBattle();
        require(battle_->prepareTacticalCheck(1),"Prepared Giant fixture unavailable");
        battle_->command({500,420});for(int i=0;i<8;++i)tick();
        require(battle_->unitStatus()[0].pose=="attack_1","Direct target did not replace Giant Hold");
        require(battle_->prepareTacticalCheck(2),"Prepared Meric fixture unavailable");tick();
        battle_->command({700,445});for(int i=0;i<130;++i)tick();
        require(battle_->unitStatus()[0].position.x>2450&&battle_->unitStatus()[1].health>100,"Meric did not finish healing and move");
        draw(1280,720,out/"meric-moved.png");
        require(battle_->prepareTacticalCheck(3),"Prepared Block fixture unavailable");
        pointer(1,Point{675,606});pointer(2,Point{675,606});for(int i=0;i<24;++i)tick();
        require(battle_->unitStatus()[0].pose=="block"&&battle_->unitStatus()[0].frame==15,"Block did not hold its original frame");
        draw(1280,720,out/"block-held.png");
        for(int fixture:{4,5}) {
            require(battle_->prepareTacticalCheck(fixture),"Prepared tower fixture unavailable");
            for(int i=0;i<150;++i)tick();draw(1280,720,out/("tower-"+std::to_string(fixture-3)+"-progress.png"));
            for(int i=0;i<353;++i)tick();
            const auto units=battle_->unitStatus();
            require(units.size()==2&&units[1].symbol==(fixture==4?6456:7526),"Captured tower did not spawn the original ghost tier");
            battle_->selectArmy();require(!battle_->unitStatus()[1].selected,"Army selection included the uncontrolled ghost");
            draw(1280,720,out/("tower-"+std::to_string(fixture-3)+"-ghost.png"));
        }
        openCampaign(CampaignIntermission::Page::Upgrades);draw(1280,720,out/"upgrades.png");openCampaign(CampaignIntermission::Page::Map);for(int i=0;i<90;++i) tick();draw(1280,720,out/"map.png");
        const auto musicBeforeWip=audio_.musicSamplesPlayed();
        progress_.level=CampaignProgress::playableLevelCount;startBattle();require(bool(wip_),"Unfinished level guard failed");draw(1280,720,out/"wip.png");
        require(musicBeforeWip>0&&audio_.musicSamplesPlayed()>=musicBeforeWip,"WIP reset the current music playhead");
        require(save(),"Campaign save failed");loadSave();require(saved_&&saved_->level==CampaignProgress::playableLevelCount,"Campaign save round trip failed");
        const auto opts=saveDir_/"options.ini";require(fs::exists(opts),"Options were not saved");
        SdlDisplay restored(window_,renderer_.native(),opts);require(restored.showFps(),"Options did not restore");
        std::ofstream report(out/"checks.json");report<<"{\"platform\":\""<<SDL_GetPlatform()<<"\",\"settingsKey\":\""<<settingsKeyLabel<<"\",\"settingsKeyVerified\":true,\"menuInput\":true,\"missionEntries\":"<<CampaignProgress::playableLevelCount<<",\"render720And1080\":true,\"pauseFreezesClock\":true,\"resume15Ticks\":true,\"settingsPersist\":true,\"audioPlayback\":true,\"campaignSaveRoundTrip\":true,\"unfinishedGuard\":true,\"mouseLockSetting\":true,\"aspectModes\":true,\"borderlessDesktop\":true,\"pcToolbar\":true,\"originalTooltip\":true,\"toolbarStates\":true,\"settingsCentered\":true,\"dynamicDefault\":true,\"portraitMenu\":true,\"singleClickSelection\":true,\"dragSelection\":true,\"castleArcherTiers\":true,\"tacticalCommands\":true,\"blockStance\":true,\"towerSpawn\":true,\"musicContinuity\":true,\"introAutoplay\":true,\"introPause\":true,\"introSkip\":true,\"spriteSheets\":true,\"losslessIntro\":"<<std::boolalpha<<losslessIntro<<"}\n";
        log("SELF-TEST passed");
    }
};
}
int main(int argc,char** argv) {
    bool test=false;fs::path testDir,root,saveDir;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--smoke-test"&&i+1<argc) {test=true;testDir=fs::absolute(argv[++i]);}
        else if(arg=="--root"&&i+1<argc) root=fs::absolute(argv[++i]);
        else if(arg=="--save-dir"&&i+1<argc) saveDir=fs::absolute(argv[++i]);
    }
    if(test) SDL_Log("SMOKE initializing video/audio");
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) return 1;
    if(test) SDL_Log("SMOKE creating native window");
    int result=1;
    try {
        if(root.empty()) {
            root=fs::path(SDL_GetBasePath());
            if(!fs::exists(root/"assets/menu")) root=fs::weakly_canonical(root/"../Resources");
        }
        if(saveDir.empty()) {char* pref=SDL_GetPrefPath("JustPatrox","DeadWarrior 2");if(!pref) throw std::runtime_error(SDL_GetError());saveDir=pref;SDL_free(pref);}
        fs::create_directories(saveDir);
        SDL_Window* window=SDL_CreateWindow("DeadWarrior 2",1280,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_HIDDEN);
        if(!window) throw std::runtime_error(SDL_GetError());
        if(test) SDL_Log("SMOKE native window created");
        const auto windowGuard=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>(window,SDL_DestroyWindow);
        SDL_SetWindowMinimumSize(window,640,360);
        if(test) SDL_Log("SMOKE constructing application");
        Application app(window,root,saveDir);
        if(test) app.show();
        if(test) SDL_Log("SMOKE application ready");
        if(test) {app.selfTest(testDir);result=0;}else result=app.run();
    } catch(const std::exception& error) {
        SDL_Log("FATAL: %s",error.what());
        if(!saveDir.empty()) {std::ofstream log(saveDir/"lastest.log",std::ios::app);log<<"FATAL "<<error.what()<<'\n';}
        if(!test) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"DeadWarrior 2",error.what(),nullptr);
    }
    SDL_Quit();return result;
}
