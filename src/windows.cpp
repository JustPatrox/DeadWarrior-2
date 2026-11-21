#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>
#include <shellapi.h>
#include <mmsystem.h>
#include "campaign_menu.hpp"
#include "campaign_tutorial.hpp"
#include "campaign_intermission.hpp"
#include "windows_display.hpp"
#include "windows_render.hpp"
#include "windows_save.hpp"
#include "windows_audio.hpp"
#include "windows_log.hpp"
#include "work_in_progress.hpp"
#include "pause_menu.hpp"
#include "resources/ids.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <deque>
#include <memory>
#include <stdexcept>

namespace {
namespace gd = Gdiplus;
namespace fs = std::filesystem;
using namespace sw2;

struct GdiSession {
    ULONG_PTR token{};
    GdiSession() {
        gd::GdiplusStartupInput input;
        if (gd::GdiplusStartup(&token,&input,nullptr)!=gd::Ok)
            throw std::runtime_error("Unable to start GDI+");
    }
    ~GdiSession() { gd::GdiplusShutdown(token); }
};

class Renderer {
    const MenuAssets& assets_;
    RasterCompositor compositor_;
    std::shared_ptr<const Raster> compositePixels_;
    std::vector<BYTE> compositeBytes_;
    std::unique_ptr<gd::Bitmap> compositeBitmap_;
    gd::Bitmap& composite(const Draw& d) {
        auto pixels=compositor_.image(d);
        if(pixels!=compositePixels_) {
            compositeBitmap_.reset();compositeBytes_.resize(pixels->rgba.size());
            for(size_t i=0;i<compositeBytes_.size();i+=4) {
                const auto* p=pixels->rgba.data()+i;auto* b=compositeBytes_.data()+i;
                b[0]=p[2];b[1]=p[1];b[2]=p[0];b[3]=p[3];
            }
            compositeBitmap_=std::make_unique<gd::Bitmap>(pixels->width,pixels->height,pixels->width*4,PixelFormat32bppARGB,compositeBytes_.data());
            compositePixels_=pixels;
        }
        return *compositeBitmap_;
    }
    std::map<fs::path,std::unique_ptr<gd::Bitmap>> images_;
    gd::Bitmap& bitmap(int id,int frame) {
        const auto& art=assets_.texture(id,frame);
        auto& image=images_[art.file];
        if (!image) {
            image=std::make_unique<gd::Bitmap>(art.file.c_str());
            if (image->GetLastStatus()!=gd::Ok) throw std::runtime_error("Cannot decode "+art.file.string());
        }
        return *image;
    }
public:
    explicit Renderer(const MenuAssets& a):assets_(a),compositor_(a,loadWindowsPng) {}
    int hit(const CampaignMenu& menu,const Viewport& viewport,Point physical) {
        auto point=viewport.toStage(physical);
        if (!point) return 0;
        auto hits=menu.hits();
        for (auto it=hits.rbegin();it!=hits.rend();++it) {
            if (!it->bounds.contains(*point)) continue;
            if (it->maskFrame) {
                const auto& p=assets_.texture(it->symbol,it->maskFrame);
                const auto& uses=assets_.placements;
                const Placement* placement=nullptr;
                for (const auto& [owner,list]:uses) {
                    for (const auto& use:list) if (use.symbol==it->symbol) placement=&use;
                }
                if (!placement) continue;
                const auto& use=*placement;
                double x=(point->x-it->bounds.x)/use.sx+use.bounds.x-p.bounds.x;
                double y=(point->y-it->bounds.y)/use.sy+use.bounds.y-p.bounds.y;
                auto& img=bitmap(it->symbol,it->maskFrame);
                if (x<0 || y<0 || x>=p.bounds.w || y>=p.bounds.h) continue;
                gd::Color color;
                img.GetPixel(int(x*img.GetWidth()/p.bounds.w),int(y*img.GetHeight()/p.bounds.h),&color);
                if (!color.GetA()) continue;
            }
            return it->symbol;
        }
        return 0;
    }
    void draw(gd::Graphics& g,const std::vector<Draw>& draws,int w,int h,bool diagnostic,bool clear=true) {
        Viewport v(w,h);
        g.SetPageUnit(gd::UnitPixel);
        if (clear) g.Clear(diagnostic ? gd::Color::Magenta : gd::Color::Black);
        auto pixels=[&](Rect r) {
            return gd::RectF(float(v.pixels.x+(r.x-Viewport::left)*v.scaleX),float(v.pixels.y+r.y*v.scale),
                             float(r.w*v.scaleX),float(r.h*v.scale));
        };
        g.SetClip(pixels({Viewport::left,0,Viewport::width,Viewport::height}));
        g.SetInterpolationMode(gd::InterpolationModeBilinear);
        g.SetPixelOffsetMode(gd::PixelOffsetModeHalf);
        g.SetTextRenderingHint(gd::TextRenderingHintAntiAliasGridFit);
        for (const auto& d:draws) {
            auto dst=pixels(d.target);
            if (dst.Y>=h || dst.GetBottom()<=0 || dst.X>=w || dst.GetRight()<=0 ||
                dst.Width<=0 || dst.Height<=0) continue;
            if (!d.symbol && !d.children) {
                const gd::Color color(BYTE(std::lround(d.alpha*255)),BYTE(d.color>>16),BYTE(d.color>>8),BYTE(d.color));
                gd::SolidBrush field(color);
                switch (d.kind) {
                case Draw::Kind::Polygon: case Draw::Kind::Line: {
                    std::vector<gd::PointF> points;
                    for (auto p:d.vertices) points.emplace_back(float(dst.X+p.x*v.scaleX),float(dst.Y+p.y*v.scale));
                    if (d.kind==Draw::Kind::Line && points.size()>=2) {
                        gd::Pen pen(color,float(d.lineWidth*v.scale));
                        g.DrawLines(&pen,points.data(),int(points.size()));
                    } else if (points.size()>=3) g.FillPolygon(&field,points.data(),int(points.size()));
                    break;
                }
                case Draw::Kind::Ellipse: g.FillEllipse(&field,dst); break;
                case Draw::Kind::Outline: {
                    gd::Pen pen(color,float(v.scale)); g.DrawRectangle(&pen,dst); break;
                }
                case Draw::Kind::Text: {
                    gd::Font font(L"Verdana",float(d.fontSize*v.scale),gd::FontStyleRegular,gd::UnitPixel);
                    gd::StringFormat format;
                    format.SetAlignment(d.centered ? gd::StringAlignmentCenter : gd::StringAlignmentNear);
                    std::wstring message(d.text.begin(),d.text.end());
                    g.DrawString(message.c_str(),int(message.size()),&font,dst,&format,&field);
                    break;
                }
                default: g.FillRectangle(&field,dst); break;
                }
                continue;
            }
            auto& img=d.children ? composite(d) : bitmap(d.symbol,d.frame);
            gd::ImageAttributes attributes;
            attributes.SetWrapMode(gd::WrapModeTileFlipXY);
            if (d.alpha<1 || d.redOffset) {
                gd::ColorMatrix m{};
                for (int i=0;i<5;++i) m.m[i][i]=1;
                m.m[3][3]=float(d.alpha);
                m.m[4][0]=float(d.redOffset)/255;
                attributes.SetColorMatrix(&m);
            }
            auto state=g.Save();
            if (d.rotation || d.mirrored) {
                g.TranslateTransform(dst.X+dst.Width/2,dst.Y+dst.Height/2);
                g.RotateTransform(float(d.rotation));
                if (d.mirrored) g.ScaleTransform(-1,1);
                dst.X=-dst.Width/2; dst.Y=-dst.Height/2;
            }
            const auto source=d.source.value_or(Rect{0,0,1,1});
            auto status=g.DrawImage(&img,dst,float(source.x*img.GetWidth()),float(source.y*img.GetHeight()),
                float(source.w*img.GetWidth()),float(source.h*img.GetHeight()),gd::UnitPixel,&attributes);
            g.Restore(state);
            if (status!=gd::Ok)
                throw std::runtime_error("Unable to draw original art");
        }
        g.ResetClip();
    }
};

void savePng(gd::Bitmap& image,const fs::path& file) {
    CLSID png{};
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}",&png);
    if (image.Save(file.c_str(),&png,nullptr)!=gd::Ok) throw std::runtime_error("Unable to save capture");
}

class Window {
    using Clock=std::chrono::steady_clock;
    static constexpr auto gameInterval=std::chrono::nanoseconds(1'000'000'000/30);
    const fs::path directory_,saveDirectory_,capturePath_;
    std::optional<CampaignProgress> saved_;
    CampaignProgress progress_;
    MenuAssets& assets_;
    CampaignMenu menu_;
    PauseMenu pause_{assets_};
    bool pauseButtonDown_{};
    bool pcButtonDown_{},cursorConfined_{};
    WindowsAudio soundPlayer_;
    std::unique_ptr<CampaignTutorial> tutorial_;
    std::unique_ptr<CampaignIntermission> intermission_;
    std::unique_ptr<WorkInProgress> unfinished_;
    std::string banner_;
    Clock::time_point bannerEnd_{};
    int loggedStep_{-1},loggedTick_{-900},loggedRenderer_{-1};
    std::ofstream tutorialTrace_;
    int lastTutorialStep_{},lastTraceTick_{};
    std::array<int,7> lastCampaignTrace_{};
    Renderer renderer_;
    HardwareRenderer hardware_;
    DisplaySettings display_;
    std::ofstream frameTiming_;
    gd::Bitmap studioLogo_;
    static constexpr auto noticeDuration = std::chrono::seconds(3);
    bool showingNotice_{true};
    Clock::time_point noticeEnd_{};
    HWND hwnd_{};
    bool diagnostic_{},forceSoftware_{};
    std::unique_ptr<gd::Bitmap> buffer_;
    Clock::time_point gameDeadline_{},lastPaint_{},lastArmySelect_{};
    std::deque<Clock::time_point> presentedFrames_;
    double measuredFps_{};
    Point mouse_{-1,-1};
    void saveOptions() {
        if (!display_.saveOptions(saveDirectory_/L"options.ini"))
            logEvent("ERROR could not save options.ini win32="+std::to_string(GetLastError()));
    }
    void loadPauseArt() {
        if (!assets_.art.contains({9951,1})) assets_.loadTextures(directory_.parent_path()/"pause");
    }
    void showPause(bool open) {
        if (open) loadPauseArt();
        pause_.setOpen(open); pauseButtonDown_=false;
        mouse_={-1,-1};
        if (GetCapture()==hwnd_) ReleaseCapture();
        if (tutorial_) {tutorial_->release({}); tutorial_->point({});}
        SetCursor(LoadCursorW(nullptr,IDC_ARROW));
        soundPlayer_.setPaused(tutorial_ && (open || display_.isOpen()));
        gameDeadline_=Clock::now()+gameInterval;
        logEvent(std::string("PAUSE open=")+(open ? "1" : "0"));
    }
    void pauseAction(PauseMenu::Action action) {
        using A=PauseMenu::Action;
        if (action==A::None) return;
        soundPlayer_.effect("clickButton",0,false);
        if (action==A::Resume) showPause(false);
        else if (action==A::Restart) {showPause(false); startMission();}
        else if (action==A::MainMenu) {
            showPause(false); soundPlayer_.stopNarration();
            tutorial_.reset(); intermission_.reset(); unfinished_.reset();
            saved_=loadProgress(saveDirectory_); progress_=CampaignProgress{};
            menu_.restart(saved_.has_value()); soundPlayer_.music("loginMusic");
            logEvent("SCENE main-menu from-pause");
        } else if (action==A::Sound || action==A::Music) {
            bool sound=menu_.music() && soundPlayer_.soundEnabled();
            bool music=menu_.music() && soundPlayer_.musicEnabled();
            if (action==A::Sound) sound=!sound; else music=!music;
            soundPlayer_.setSoundEnabled(sound); soundPlayer_.setMusicEnabled(music);
            menu_.setMusic(sound || music); soundPlayer_.setMuted(!menu_.music());
        }
    }
    void banner(std::string message) {
        banner_=std::move(message); bannerEnd_=Clock::now()+std::chrono::seconds(6);
    }
    void unavailable(std::string control) {
        logEvent("UNAVAILABLE button="+control+" level="+std::to_string(progress_.level+1)+
            " tick="+std::to_string(tutorial_ ? tutorial_->ticks() : -1));
        banner("This button does not work yet, please report it with the log");
    }
    void showUnfinished() {
        logEvent("SCENE work-in-progress unavailable_level="+std::to_string(progress_.level+1));
        if (tutorialTrace_.is_open()) tutorialTrace_<<"400\t"<<progress_.level<<std::endl;
        soundPlayer_.stopNarration(); soundPlayer_.music("loginMusic");
        if (!assets_.art.contains({1002,10001})) assets_.loadTextures(directory_.parent_path()/"wip");
        tutorial_.reset(); intermission_.reset(); unfinished_=std::make_unique<WorkInProgress>(assets_);
        mouse_={-1,-1}; banner_.clear();
        if (GetCapture()==hwnd_) ReleaseCapture();
        SetCursor(LoadCursorW(nullptr,IDC_ARROW));
    }
    void unfinishedAction(WorkInProgress::Choice choice) {
        if (choice==WorkInProgress::Choice::None) return;
        if (choice==WorkInProgress::Choice::Quit) {
            logEvent("WIP action=quit"); PostMessageW(hwnd_,WM_CLOSE,0,0); return;
        }
        const bool erase=choice==WorkInProgress::Choice::RestartDelete;
        if (erase ? !deleteProgress(saveDirectory_) : !saveProgress(progress_,saveDirectory_)) {
            logEvent(std::string("ERROR restart save operation failed delete=")+(erase ? "true" : "false")+
                " win32="+std::to_string(GetLastError()));
            banner("The save could not be updated. Please check lastest.log and try again."); return;
        }
        saved_=loadProgress(saveDirectory_); progress_=CampaignProgress{};
        tutorial_.reset(); intermission_.reset(); unfinished_.reset();
        menu_.restart(saved_.has_value()); soundPlayer_.stopMusic(); soundPlayer_.stopNarration();
        display_.setOpen(false); showingNotice_=true; noticeEnd_=Clock::now()+noticeDuration;
        gameDeadline_=noticeEnd_; banner_.clear(); mouse_={-1,-1};
        logEvent(std::string("RESTART completed saves=")+(erase ? "deleted" : "kept"));
        InvalidateRect(hwnd_,nullptr,FALSE);
    }
    void startMission() {
        pause_.setOpen(false); soundPlayer_.setPaused(false);
        if (progress_.level>=CampaignProgress::playableLevelCount) {
            showUnfinished();
            return;
        }
        logEvent("MISSION start level="+std::to_string(progress_.level+1)+" difficulty="+std::to_string(int(progress_.difficulty)+1));
        loggedStep_=-1; loggedTick_=-900;
        const auto dir=directory_.parent_path()/"tutorial";
        soundPlayer_.stopNarration();
        if (!assets_.art.contains({4102,1})) assets_.loadTextures(dir);
        if (!assets_.art.contains({8824,1})) assets_.loadTextures(directory_.parent_path()/"interaction");
        if (progress_.level==0)
            tutorial_=std::make_unique<CampaignTutorial>(assets_,dir,progress_.difficulty,progress_.records.front().attempts);
        else {
            if (!assets_.art.contains({4119,1})) assets_.loadTextures(directory_.parent_path()/"archidon");
            if (progress_.level>=2 && !assets_.art.contains({4053,1})) assets_.loadTextures(directory_.parent_path()/"shadow");
            if (progress_.level>=3 && !assets_.art.contains({7531,1})) assets_.loadTextures(directory_.parent_path()/"magic");
            if (progress_.level>=4 && !assets_.art.contains({4067,1})) assets_.loadTextures(directory_.parent_path()/"rebels");
            if (progress_.level>=5 && !assets_.art.contains({6465,1})) assets_.loadTextures(directory_.parent_path()/"giants");
            if (progress_.level>=6 && !assets_.art.contains({8771,1})) assets_.loadTextures(directory_.parent_path()/"bomber");
            if (progress_.level>=7 && !assets_.art.contains({4932,1})) assets_.loadTextures(directory_.parent_path()/"knight");
            if (progress_.level>=8 && !assets_.art.contains({8262,10001})) assets_.loadTextures(directory_.parent_path()/"deads");
            if (progress_.level>=9 && !assets_.art.contains({5778,1})) assets_.loadTextures(directory_.parent_path()/"crawlers");
            tutorial_=std::make_unique<CampaignTutorial>(assets_,directory_.parent_path(),progress_);
        }
        intermission_.reset();
        lastTutorialStep_=lastTraceTick_=0;
        soundPlayer_.music("orderInGame");
    }
    void openIntermission(CampaignIntermission::Page page) {
        if (progress_.level>CampaignProgress::playableLevelCount) {showUnfinished(); return;}
        logEvent("SCENE campaign level="+std::to_string(progress_.level+1)+" page="+std::to_string(int(page)));
        soundPlayer_.stopNarration();
        intermission_=std::make_unique<CampaignIntermission>(assets_,directory_.parent_path()/"intermission",progress_,page);
        tutorial_.reset();
        soundPlayer_.music("loginMusic");
    }
    void campaignEvents() {
        if (!intermission_) return;
        using E=CampaignIntermission::Event;
        for (auto event:intermission_->takeEvents()) {
            switch (event) {
            case E::Click: soundPlayer_.effect("clickButton",0,false); break;
            case E::Equip: soundPlayer_.effect("ArmoryEquipSound",0,false); break;
            case E::Arrived: soundPlayer_.effect("SelectRaceSound",0,false); break;
            case E::Save: {
                const bool ok=saveProgress(progress_,saveDirectory_);
                logEvent(std::string("SAVE result=")+(ok ? "success" : "failure")+" level="+std::to_string(progress_.level+1));
                intermission_->saved(ok);
                if (ok) saved_=progress_;
                else MessageBoxW(hwnd_,L"The campaign could not be saved.",L"DeadWarrior 2",MB_OK|MB_ICONERROR);
                break;
            }
            case E::Online: case E::Guide:
                ShellExecuteW(hwnd_,L"open",event==E::Online ? L"http://www.stickempires.com" :
                              L"http://www.stickpage.com/stickempiresguide.shtml",nullptr,nullptr,SW_SHOWNORMAL);
                break;
            case E::StartLevel: startMission(); return;
            }
        }
        if (tutorialTrace_.is_open()) {
            const std::array row{intermission_->page()==CampaignIntermission::Page::Upgrades ? 100 : 101,
                intermission_->mapFrame(),progress_.points,int(progress_.upgrades.to_ulong()),
                int(intermission_->confirming()),int(intermission_->savePrompt()),int(progress_.difficulty)+1};
            if (row!=lastCampaignTrace_) {
                tutorialTrace_<<row[0];
                for (size_t i=1;i<row.size();++i) tutorialTrace_<<'\t'<<row[i];
                tutorialTrace_<<std::endl;
                lastCampaignTrace_=row;
            }
        }
    }
    void resultAction() {
        const auto action=tutorial_->takeResultAction();
        if (action==CampaignTutorial::ResultAction::Retry) startMission();
        else if (action==CampaignTutorial::ResultAction::Continue) openIntermission(CampaignIntermission::Page::Upgrades);
        else if (action==CampaignTutorial::ResultAction::Online)
            ShellExecuteW(hwnd_,L"open",L"http://www.stickempires.com",nullptr,nullptr,SW_SHOWNORMAL);
        else return;
        soundPlayer_.effect("clickButton",0,false);
    }
    void actions() {
        for (auto action:menu_.takeActions()) {
            const wchar_t* url=nullptr;
            switch (action) {
            case Action::ClickSound:
                soundPlayer_.effect("clickButton",0,false);
                break;
            case Action::MusicToggle:
                soundPlayer_.setSoundEnabled(menu_.music());
                soundPlayer_.setMusicEnabled(menu_.music());
                soundPlayer_.setMuted(!menu_.music());
                break;
            case Action::Online: url=L"http://www.stickempires.com"; break;
            case Action::FirstGame: url=L"http://www.stickpage.com/stickwargame.shtml"; break;
            case Action::Stickpage: url=L"http://www.stickpage.com"; break;
            case Action::IntroLink: url=L"http://www.stickpage.com/stickwar2orderempireintro.shtml"; break;
            case Action::Map:
                progress_=CampaignProgress{}; progress_.difficulty=menu_.difficulty(); startMission(); break;
            case Action::LoadCampaign:
                if (saved_) {progress_=*saved_; if (progress_.level>=CampaignProgress::playableLevelCount) showUnfinished(); else openIntermission(CampaignIntermission::Page::Map);}
                break;
            case Action::Unavailable: unavailable("menu symbol="+std::to_string(target())); break;
            default: break;
            }
            if (url) ShellExecuteW(hwnd_,L"open",url,nullptr,nullptr,SW_SHOWNORMAL);
        }
    }
    int target() {
        if (showingNotice_ || tutorial_ || intermission_ || unfinished_ || display_.isOpen()) return 0;
        RECT r{};
        GetClientRect(hwnd_,&r);
        return renderer_.hit(menu_,Viewport(r.right,r.bottom),mouse_);
    }
    void advanceSimulation(Clock::time_point now) {
        if (showingNotice_) {
            if (now < noticeEnd_) return;
            showingNotice_=false;
            logEvent("SCENE main-menu");
            gameDeadline_=now+gameInterval;
            soundPlayer_.music("loginMusic");
        }
        if (tutorial_ && (pause_.isOpen() || display_.isOpen())) {
            gameDeadline_=now+gameInterval;
            return;
        }
        // Leave any remaining ticks due for the next pass so a slow paint
        // cannot discard simulation time or prevent input from being pumped.
        for (int batch=0;batch<8 && gameDeadline_<=now;++batch) {
            gameDeadline_+=gameInterval;
            if (unfinished_) continue;
            if (tutorial_) {
                const bool hadEnded=tutorial_->ended();
                const bool hadResults=tutorial_->showingResults();
                tutorial_->tick();
                if (loggedStep_!=tutorial_->stepNumber() || tutorial_->ticks()-loggedTick_>=900) {
                    loggedStep_=tutorial_->stepNumber(); loggedTick_=tutorial_->ticks();
                    const auto state=tutorial_->battleStatus();
                    logEvent("BATTLE level="+std::to_string(progress_.level+1)+" step="+std::to_string(loggedStep_)+
                        " tick="+std::to_string(loggedTick_)+" camera="+std::to_string(tutorial_->camera())+
                        " statues="+std::to_string(state.playerStatue)+"/"+std::to_string(state.enemyStatue)+
                        " gold="+std::to_string(state.gold)+" population="+std::to_string(state.population));
                }
                for (const auto& event:tutorial_->takeSounds())
                    soundPlayer_.effect(event.name,(event.position.x-tutorial_->camera()-425)/Viewport::width,event.spatial);
                if (!hadEnded && tutorial_->ended()) {
                    soundPlayer_.stopNarration();
                    soundPlayer_.effect("StatueDestroyed",0,false);
                }
                if (!hadResults && tutorial_->showingResults()) {
                    progress_.finish(tutorial_->won(),tutorial_->ticks()/30,tutorial_->reward());
                    soundPlayer_.music(tutorial_->won() ? "OrderVictory" : "OrderDefeat");
                }
                if (tutorialTrace_.is_open() && (lastTutorialStep_!=tutorial_->stepNumber() || tutorial_->ticks()-lastTraceTick_>=30)) {
                    lastTutorialStep_=tutorial_->stepNumber();
                    lastTraceTick_=tutorial_->ticks();
                    const auto cursor=tutorial_->cursor().value_or(Point{-9999,-9999});
                    const auto status=tutorial_->defenceStatus();
                    const auto battle=tutorial_->battleStatus();
                    tutorialTrace_<<lastTutorialStep_<<'\t'<<tutorial_->ticks()<<'\t'<<tutorial_->camera()
                                  <<'\t'<<cursor.x<<'\t'<<cursor.y<<'\t'<<status.gold<<'\t'<<status.garrisoned
                                  <<'\t'<<status.researchTicks<<'\t'<<status.shots<<'\t'<<status.impacts
                                  <<'\t'<<battle.playerStatue<<'\t'<<battle.enemyStatue<<'\t'<<battle.reinforcement
                                  <<'\t'<<battle.minersQueued<<'\t'<<battle.fightersQueued<<'\t'<<tutorial_->showingResults()
                                  <<'\t'<<tutorial_->fastForward()<<'\t'<<battle.lancers<<'\t'<<battle.enemyArchers
                                  <<'\t'<<battle.mobileShots<<'\t'<<battle.towerControl
                                  <<'\t'<<battle.ninjas<<'\t'<<battle.wizards<<'\t'<<battle.healers
                                  <<'\t'<<battle.magicHits<<'\t'<<battle.heals<<'\t'<<battle.enemyCastleShots
                                  <<'\t'<<battle.spells[0]<<'\t'<<battle.spells[1]<<'\t'<<battle.spells[2]
                                  <<'\t'<<battle.giants<<'\t'<<battle.giantHits<<'\t'<<battle.cinematic<<'\t'<<battle.dialogue
                                  <<'\t'<<battle.bombers<<'\t'<<battle.bomberWaves<<'\t'<<battle.nextBomberWave
                                  <<'\t'<<battle.explosions<<'\t'<<battle.boulderShots<<'\t'<<battle.boulderHits<<'\t'<<battle.enslavedGiants
                                  <<'\t'<<battle.miners<<'\t'<<battle.archers<<'\t'<<battle.population
                                  <<'\t'<<battle.enemyKnights<<'\t'<<battle.selectedMilitary<<'\t'<<battle.knightHint
                                  <<'\t'<<battle.enemyDeads<<'\t'<<battle.gutsShots<<'\t'<<battle.poisonHits<<'\t'<<battle.deadsHint
                                  <<'\t'<<battle.alliedHealers<<'\t'<<battle.cures<<'\t'<<battle.alliedWizards;
                    for(int count:battle.queued) tutorialTrace_<<'\t'<<count;
                    tutorialTrace_<<'\t'<<battle.deadFront.x<<'\t'<<battle.deadFront.y<<'\t'<<battle.mana<<'\t'<<battle.fighters
                                  <<'\t'<<battle.enemyCrawlers<<'\t'<<battle.crawlerHits<<'\t'<<battle.maxCrawlerDamage
                                  <<'\t'<<battle.crawlerFront.x<<'\t'<<battle.crawlerFront.y<<std::endl;
                }
                auto voice=tutorial_->takeVoice();
                if (!voice.empty()) {
                    logEvent("VOICE "+voice);
                    auto file=directory_.parent_path()/"tutorial"/(voice+".wav");
                    soundPlayer_.narration(file);
                }
            } else if (intermission_) {
                intermission_->tick();
                campaignEvents();
            } else {
                menu_.hover(target());
                menu_.tick();
                actions();
            }
        }
    }
    void drawNotice(gd::Graphics& g,int w,int h) {
        g.Clear(gd::Color(255,0,0,0));
        g.SetPageUnit(gd::UnitPixel);
        const float scale=float(Viewport(w,h).scale);
        const float cx=w*0.5f, cy=h*0.5f;
        const float logoWidth=372*scale;
        const float logoHeight=logoWidth*studioLogo_.GetHeight()/studioLogo_.GetWidth();
        const float lineHeight=38*scale, gap=16*scale;
        const float top=cy-(lineHeight+gap+logoHeight)*0.5f;
        gd::Font font(L"Segoe UI",24*scale,gd::FontStyleRegular,gd::UnitPixel);
        gd::StringFormat format;
        format.SetAlignment(gd::StringAlignmentCenter);
        format.SetLineAlignment(gd::StringAlignmentCenter);
        gd::SolidBrush ink(gd::Color(255,242,242,242));
        g.SetTextRenderingHint(gd::TextRenderingHintAntiAliasGridFit);
        g.DrawString(L"This project is not affiliated with",-1,&font,
                     gd::RectF(0,top,float(w),lineHeight),&format,&ink);
        g.SetInterpolationMode(gd::InterpolationModeHighQualityBicubic);
        g.DrawImage(&studioLogo_,gd::RectF(cx-logoWidth/2,top+lineHeight+gap,logoWidth,logoHeight));
    }
    std::vector<Draw> scene() const {
        RECT client{};GetClientRect(hwnd_,&client);display_.configureViewport(client.right,client.bottom);
        std::vector<Draw> draws;
        if (!showingNotice_) {
            if (unfinished_) draws=unfinished_->drawList();
            else if (intermission_) draws=intermission_->drawList();
            else draws=tutorial_ ? tutorial_->drawList() : menu_.drawList();
        }
        if (pause_.isOpen() && !display_.isOpen()) {
            auto overlay=pause_.drawList(menu_.music() && soundPlayer_.soundEnabled(),
                                         menu_.music() && soundPlayer_.musicEnabled());
            draws.insert(draws.end(),overlay.begin(),overlay.end());
        }
        display_.append(draws);
        if (!banner_.empty() && Clock::now()<bannerEnd_) {
            draws.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left+24,15,Viewport::width-48,46},0,.94));
            draws.push_back(Draw::label({Viewport::left+34,25,Viewport::width-68,30},banner_,19,0xffdc82,true));
        }
        if (display_.showFps() && !showingNotice_) {
            draws.push_back(Draw::shape(Draw::Kind::Box,{Viewport::left+8,8,112,30},0,.75));
            draws.push_back(Draw::label({Viewport::left+16,12,100,24},
                "FPS: "+std::to_string(int(std::lround(measuredFps_))),17,0x8dff8d));
        }
        return draws;
    }
    void composeSoftware(int w,int h,const std::vector<Draw>& draws) {
        if (!buffer_ || int(buffer_->GetWidth())!=w || int(buffer_->GetHeight())!=h)
            buffer_=std::make_unique<gd::Bitmap>(w,h,PixelFormat32bppPARGB);
        {
            gd::Graphics back(buffer_.get());
            if (showingNotice_) drawNotice(back,w,h);
            if (!showingNotice_ || !draws.empty()) renderer_.draw(back,draws,w,h,diagnostic_,!showingNotice_);
        }
    }
    void softwareDraw(HDC dc,int w,int h,const std::vector<Draw>& draws) {
        const auto [rw,rh]=display_.renderSize(w,h);
        composeSoftware(rw,rh,draws);
        gd::Graphics screen(dc);
        screen.SetInterpolationMode(gd::InterpolationModeHighQualityBilinear);
        screen.DrawImage(buffer_.get(),0,0,w,h);
    }
    void drawClient(HDC dc,bool present) {
        RECT r{};
        GetClientRect(hwnd_,&r);
        if (r.right<=0 || r.bottom<=0 || (present && IsIconic(hwnd_))) return;
        const auto start=Clock::now();
        const auto draws=scene();
        const auto [rw,rh]=display_.renderSize(r.right,r.bottom);
        const bool accelerated=present && !showingNotice_ && !forceSoftware_ &&
            hardware_.draw(hwnd_,rw,rh,draws,diagnostic_,display_.vsync());
        if (present && loggedRenderer_!=int(accelerated)) {
            loggedRenderer_=int(accelerated);
            logEvent(std::string("RENDER backend=")+(accelerated ? "Direct2D" : "GDI+")+
                " client="+std::to_string(r.right)+"x"+std::to_string(r.bottom));
        }
        if (!accelerated) softwareDraw(dc,r.right,r.bottom,draws);
        if (present) {
            const auto end=Clock::now();
            presentedFrames_.push_back(end);
            while (presentedFrames_.size()>1 && end-presentedFrames_.front()>std::chrono::seconds(1))
                presentedFrames_.pop_front();
            if (presentedFrames_.size()>1)
                measuredFps_=(presentedFrames_.size()-1)/std::chrono::duration<double>(end-presentedFrames_.front()).count();
            else measuredFps_=0;
            const auto ms=[](Clock::duration d){ return std::chrono::duration<double,std::milli>(d).count(); };
            const double interval=lastPaint_==Clock::time_point{} ? 0. : ms(end-lastPaint_);
            if (frameTiming_.is_open()) frameTiming_<<interval<<'\t'<<ms(end-start)<<'\t'<<accelerated<<'\t'
                        <<(showingNotice_ ? "notice" : tutorial_ && (pause_.isOpen() || display_.isOpen()) ? "paused" : tutorial_ ? "battle" : intermission_ ? "map" : "menu")
                        <<'\t'<<display_.frameLimit()<<'\t'<<(tutorial_ ? tutorial_->ticks() : -1)
                        <<'\t'<<(accelerated && hardware_.synchronized())<<'\n';
            lastPaint_=end;
        }
    }
    void showDisplay(bool open) {
        if(open) {ClipCursor(nullptr);cursorConfined_=false;}
        logEvent(std::string("DISPLAY menu=")+(open ? "open" : "closed"));
        if (GetCapture()==hwnd_) ReleaseCapture();
        mouse_={-1,-1};
        menu_.release(0);
        menu_.hover(0);
        if (tutorial_) tutorial_->release({});
        if (intermission_) {intermission_->point({}); intermission_->release();}
        display_.setOpen(open);
        soundPlayer_.setPaused(tutorial_ && (open || pause_.isOpen()));
        if (tutorial_) gameDeadline_=Clock::now()+gameInterval;
        if (showingNotice_) InvalidateRect(hwnd_,nullptr,FALSE);
    }
    void paint() {
        PAINTSTRUCT paint{};
        HDC dc=BeginPaint(hwnd_,&paint);
        drawClient(dc,true);
        EndPaint(hwnd_,&paint);
    }
    void toggleFullscreen() {
        using Mode=DisplaySettings::Mode;
        display_.setMode(hwnd_,display_.mode()==Mode::Fullscreen ? Mode::Windowed : Mode::Fullscreen);
        logEvent("DISPLAY mode="+std::to_string(int(display_.mode())));
        saveOptions();
        InvalidateRect(hwnd_,nullptr,FALSE);
    }
    void refreshPointer() {
        POINT position{};
        if (!GetCursorPos(&position) || WindowFromPoint(position)!=hwnd_) return;
        ScreenToClient(hwnd_,&position);
        RECT client{}; GetClientRect(hwnd_,&client);
        if (!PtInRect(&client,position)) return;
        const bool custom=tutorial_ && !display_.isOpen() && !pause_.isOpen() && tutorial_->cursorSymbol();
        SetCursor(custom ? nullptr : LoadCursorW(nullptr,IDC_ARROW));
    }
    void updateCursorLock() {
        const bool confine=display_.mouseLock() && tutorial_ && !tutorial_->ended() &&
            !showingNotice_ && !display_.isOpen() && !pause_.isOpen() &&
            GetForegroundWindow()==hwnd_ && !IsIconic(hwnd_);
        if(confine) {
            RECT client{};GetClientRect(hwnd_,&client);const Viewport v(client.right,client.bottom);
            POINT first{LONG(std::ceil(v.pixels.x)),LONG(std::ceil(v.pixels.y))};
            POINT last{LONG(std::floor(v.pixels.x+v.pixels.w)),LONG(std::floor(v.pixels.y+v.pixels.h))};
            ClientToScreen(hwnd_,&first);ClientToScreen(hwnd_,&last);
            RECT limits{first.x,first.y,last.x,last.y};ClipCursor(&limits);cursorConfined_=true;
        } else if(cursorConfined_) {ClipCursor(nullptr);cursorConfined_=false;}
    }
    LRESULT message(UINT msg,WPARAM w,LPARAM l) {
        switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_SETCURSOR:
            if (LOWORD(l)==HTCLIENT && tutorial_ && !display_.isOpen() && !pause_.isOpen()) {
                SetCursor(tutorial_->cursorSymbol() ? nullptr : LoadCursorW(nullptr,IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_KILLFOCUS: case WM_CANCELMODE:
            ClipCursor(nullptr);cursorConfined_=false;pcButtonDown_=false;
            pauseButtonDown_=false; pause_.cancelPress();
            mouse_={-1,-1};
            if (tutorial_) {tutorial_->pcPressed(false);tutorial_->release({}); tutorial_->point({});}
            if (GetCapture()==hwnd_) ReleaseCapture();
            return 0;
        case WM_PAINT: paint(); return 0;
        case WM_PRINTCLIENT: drawClient(reinterpret_cast<HDC>(w),false); return 0;
        case WM_APP+15: {
            if (capturePath_.empty()) return 0;
            RECT r{}; GetClientRect(hwnd_,&r);
            if (r.right>0 && r.bottom>0) {
                const auto [rw,rh]=display_.renderSize(r.right,r.bottom);
                composeSoftware(rw,rh,scene());
                if (rw==r.right && rh==r.bottom) savePng(*buffer_,capturePath_);
                else {
                    gd::Bitmap scaled(r.right,r.bottom,PixelFormat32bppPARGB);
                    { gd::Graphics graphics(&scaled);
                      graphics.SetInterpolationMode(gd::InterpolationModeHighQualityBilinear);
                      graphics.DrawImage(buffer_.get(),0,0,r.right,r.bottom); }
                    savePng(scaled,capturePath_);
                }
            }
            return 0;
        }
        case WM_APP+16:
            return !capturePath_.empty() && tutorial_ && !display_.isOpen() ? tutorial_->cursorSymbol() : 0;
        case WM_APP+17:
            return capturePath_.empty() ? 0 : soundPlayer_.submitted();
        case WM_APP+18:
            return capturePath_.empty() ? 0 : (unfinished_ ? 1 : 0) | (!banner_.empty() && Clock::now()<bannerEnd_ ? 2 : 0);
        case WM_APP+19:
            if (capturePath_.empty()) return -1;
            if (frameTiming_.is_open()) frameTiming_.flush();
            return !display_.showFps() ? -1 : LRESULT(std::lround(measuredFps_*100));
        case WM_APP+20: {
            if (capturePath_.empty()) return 0;
            const auto size=hardware_.pixelSize();
            const unsigned width=loggedRenderer_==1 ? unsigned(size.cx) : buffer_ ? buffer_->GetWidth() : 0;
            const unsigned height=loggedRenderer_==1 ? unsigned(size.cy) : buffer_ ? buffer_->GetHeight() : 0;
            return LRESULT((width<<16)|height);
        }
        case WM_APP+21:
            return capturePath_.empty() ? 0 : int(pause_.isOpen()) | (int(display_.isOpen())<<1) |
                (int(menu_.music() && soundPlayer_.soundEnabled())<<2) |
                (int(menu_.music() && soundPlayer_.musicEnabled())<<3);
        case WM_APP+22:
            return capturePath_.empty() || !tutorial_ ? -1 : tutorial_->ticks();
        case WM_APP+23: return capturePath_.empty() ? 0 : LRESULT(std::lround(Viewport::width*100));
        case WM_APP+24: return capturePath_.empty() ? 0 : LRESULT(std::lround(Viewport::height*100));
        case WM_APP+25: return capturePath_.empty() ? 0 : int(cursorConfined_);
        case WM_APP+26: {
            // Explicit diagnostic capture only; normal play still writes lastest.log.
            if(capturePath_.empty() || !tutorial_) return 0;
            auto path=capturePath_;path.replace_extension(".units.tsv");std::ofstream units(path);
            units<<"id\tsymbol\tx\ty\thealth\tenemy\tselected\tfacing\tresource\tslot\tbag\torder\tgoalX\tgoalY\tpose\n";
            int id=0;
            for(const auto& u:tutorial_->unitStatus()) units<<id++<<'\t'<<u.symbol<<'\t'<<u.position.x<<'\t'<<u.position.y<<'\t'<<u.health<<'\t'<<u.enemy
                <<'\t'<<u.selected<<'\t'<<u.facing<<'\t'<<u.resource<<'\t'<<u.miningSlot<<'\t'<<u.bag<<'\t'<<u.order<<'\t'<<u.goal.x<<'\t'<<u.goal.y<<'\t'<<u.pose<<'\n';
            path=capturePath_;path.replace_extension(".ore.tsv");std::ofstream ores(path);
            ores<<"id\tx\ty\tremaining\ttotal\n";id=0;
            for(const auto& ore:tutorial_->oreStatus())ores<<id++<<'\t'<<ore.position.x<<'\t'<<ore.position.y<<'\t'<<ore.remaining<<'\t'<<ore.total<<'\n';
            return id;
        }
        case WM_SIZE: {
            RECT client{};GetClientRect(hwnd_,&client);display_.configureViewport(client.right,client.bottom);
            updateCursorLock();InvalidateRect(hwnd_,nullptr,FALSE);return 0;
        }
        case WM_POWERBROADCAST:
            if (w==PBT_APMRESUMEAUTOMATIC && !showingNotice_) gameDeadline_=Clock::now()+gameInterval;
            return TRUE;
        case WM_SIZING:
            display_.resize(hwnd_,w,*reinterpret_cast<RECT*>(l));
            return TRUE;
        case WM_CAPTURECHANGED:
            if (l) {pauseButtonDown_=false;pause_.cancelPress();}
            if (display_.isOpen()) display_.pointer(hwnd_,msg,{});
            else if (l && tutorial_) tutorial_->release({});
            return 0;
        case WM_DPICHANGED: {
            auto r=reinterpret_cast<RECT*>(l);
            SetWindowPos(hwnd_,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER);
            return 0;
        }
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONDOWN: {
            mouse_={double(GET_X_LPARAM(l)),double(GET_Y_LPARAM(l))};
            RECT actual{};GetClientRect(hwnd_,&actual);display_.configureViewport(actual.right,actual.bottom);
            if (display_.isOpen()) {
                RECT r{}; GetClientRect(hwnd_,&r);
                display_.pointer(hwnd_,msg,Viewport(r.right,r.bottom).toStage(mouse_));
                if (msg==WM_LBUTTONUP) logEvent("DISPLAY mode="+std::to_string(int(display_.mode()))+
                    " fps="+std::to_string(display_.frameLimit())+" vsync="+std::to_string(display_.vsync())+
                    " fps_counter="+std::to_string(display_.showFps())+" resolution="+display_.resolutionLabel());
                if (msg==WM_LBUTTONUP) saveOptions();
                if (showingNotice_ && msg!=WM_MOUSEMOVE) InvalidateRect(hwnd_,nullptr,FALSE);
                return 0;
            }
            if (pause_.isOpen()) {
                RECT r{}; GetClientRect(hwnd_,&r);
                pause_.point(Viewport(r.right,r.bottom).toStage(mouse_));
                if (msg==WM_LBUTTONDOWN) {SetCapture(hwnd_);pause_.press();}
                if (msg==WM_LBUTTONUP) {const auto action=pause_.release();ReleaseCapture();pauseAction(action);}
                return 0;
            }
            if (unfinished_) {
                RECT r{}; GetClientRect(hwnd_,&r);
                unfinished_->point(Viewport(r.right,r.bottom).toStage(mouse_));
                if (msg==WM_LBUTTONDOWN) {SetCapture(hwnd_); unfinished_->press();}
                if (msg==WM_LBUTTONUP) {
                    ReleaseCapture(); const auto choice=unfinished_->release(); unfinishedAction(choice);
                }
                return 0;
            }
            // Hardware moves arrive through the window's queue. A client
            // sending mouse messages supplies its own leave event. Asking
            // USER32 to track the physical pointer would cancel that input.
            if (msg==WM_MOUSEMOVE && InSendMessageEx(nullptr)==ISMEX_NOSEND) {
                TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,hwnd_,0};
                TrackMouseEvent(&track);
            }
            if (intermission_) {
                RECT r{}; GetClientRect(hwnd_,&r);
                intermission_->point(Viewport(r.right,r.bottom).toStage(mouse_));
                if (msg==WM_LBUTTONDOWN) {SetCapture(hwnd_); intermission_->press();}
                if (msg==WM_LBUTTONUP) {ReleaseCapture(); intermission_->release(); campaignEvents();}
                return 0;
            }
            if (tutorial_) {
                RECT r{}; GetClientRect(hwnd_,&r);
                auto stage=Viewport::battlePoint(Viewport(r.right,r.bottom).toStage(mouse_));
                tutorial_->point(stage);
                if(msg==WM_LBUTTONDOWN) tutorial_->toolbarMouseDown(true);
                if(msg==WM_LBUTTONUP) tutorial_->toolbarMouseDown(false);
                const Rect pcButton{320,674,48,22};
                if(!tutorial_->ended() && msg==WM_LBUTTONDOWN && stage && pcButton.contains(*stage)) {
                    pcButtonDown_=true;tutorial_->pcPressed(true);SetCapture(hwnd_);return 0;
                }
                if(msg==WM_LBUTTONUP && pcButtonDown_) {
                    pcButtonDown_=false;tutorial_->pcPressed(false);ReleaseCapture();if(stage && pcButton.contains(*stage)) showDisplay(true);return 0;
                }
                if (!tutorial_->ended() && msg==WM_LBUTTONDOWN && stage && tutorial_->toolbarControlHit("menuButton",*stage)) {
                    pauseButtonDown_=true; SetCapture(hwnd_); return 0;
                }
                if (msg==WM_LBUTTONUP && pauseButtonDown_) {
                    pauseButtonDown_=false; ReleaseCapture();
                    if (stage && tutorial_->toolbarControlHit("menuButton",*stage)) showPause(true);
                    return 0;
                }
                refreshPointer();
                if (InSendMessageEx(nullptr)==ISMEX_NOSEND)
                    SetCursor(tutorial_->cursorSymbol() ? nullptr : LoadCursorW(nullptr,IDC_ARROW));
                if (msg==WM_LBUTTONDOWN && stage) {
                    SetCapture(hwnd_); tutorial_->press(*stage);
                    const auto pending=tutorial_->takeUnavailableControl();
                    if (!pending.empty()) unavailable(pending);
                }
                if (msg==WM_LBUTTONUP) {
                    ReleaseCapture();
                    tutorial_->release(stage);
                    resultAction();
                }
                if (msg==WM_RBUTTONDOWN && stage) tutorial_->command(*stage);
                return 0;
            }
            int id=target();
            menu_.hover(id);
            if (msg==WM_LBUTTONDOWN) { SetCapture(hwnd_); menu_.press(id); }
            if (msg==WM_LBUTTONUP) { ReleaseCapture(); menu_.release(id); }
            actions(); return 0;
        }
        case WM_MOUSELEAVE:
            mouse_={-1,-1}; menu_.hover(0);
            if (tutorial_) tutorial_->point({});
            if (intermission_) intermission_->point({});
            if (unfinished_) unfinished_->point({});
            if (pause_.isOpen()) pause_.point({});
            return 0;
        case WM_KEYDOWN: {
            using Mode=DisplaySettings::Mode;
            const bool repeated=(l&(1L<<30))!=0;
            if (w==VK_INSERT && !repeated) showDisplay(!display_.isOpen());
            else if (w==VK_ESCAPE && display_.isOpen() && !repeated) showDisplay(false);
            else if (w==VK_F11 && !repeated) toggleFullscreen();
            else if (pause_.isOpen() && !display_.isOpen() && !repeated) {
                if (w=='P') showPause(false);
                else pauseAction(pause_.key(int(w)));
            }
            else if (tutorial_ && !tutorial_->ended() && !display_.isOpen() && !repeated && (w=='P' || w==VK_ESCAPE)) showPause(true);
            else if (unfinished_ && !display_.isOpen() && !repeated) {
                if (w==VK_TAB || w==VK_RIGHT || w==VK_DOWN) unfinished_->focus(1);
                else if (w==VK_LEFT || w==VK_UP) unfinished_->focus(-1);
                else if (w==VK_RETURN || w==VK_SPACE) unfinishedAction(unfinished_->selected());
            }
            else if (w==VK_ESCAPE && display_.mode()!=Mode::Windowed && !repeated) {
                display_.setMode(hwnd_,Mode::Windowed);
                InvalidateRect(hwnd_,nullptr,FALSE);
            } else if (w==VK_SPACE && tutorial_ && !display_.isOpen() && !repeated) {
                const auto now=Clock::now();tutorial_->selectArmy(now-lastArmySelect_<std::chrono::milliseconds(400));lastArmySelect_=now;
            } else if ((w=='Q' || w=='W' || w=='E') && tutorial_ && !display_.isOpen() && !repeated) {
                tutorial_->unitAbility(w=='Q' ? 0 : w=='W' ? 1 : 2);
            } else if (display_.isOpen() && (!repeated || (w!=VK_RETURN && w!=VK_SPACE))) {
                display_.key(hwnd_,w);
                saveOptions();
                logEvent("DISPLAY mode="+std::to_string(int(display_.mode()))+
                    " fps="+std::to_string(display_.frameLimit())+" vsync="+std::to_string(display_.vsync())+
                    " fps_counter="+std::to_string(display_.showFps())+" resolution="+display_.resolutionLabel());
                if (showingNotice_) InvalidateRect(hwnd_,nullptr,FALSE);
            }
            return 0;
        }
        case WM_DESTROY: ClipCursor(nullptr);saveOptions(); logEvent("SHUTDOWN window closed"); PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(hwnd_,msg,w,l);
    }
    static LRESULT CALLBACK dispatch(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
        auto self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if (msg==WM_NCCREATE) {
            self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            self->hwnd_=hwnd;
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        try {return self ? self->message(msg,w,l) : DefWindowProcW(hwnd,msg,w,l);}
        catch (const std::exception& error) {
            logEvent("FATAL window message="+std::to_string(msg)+" error="+error.what());
            PostQuitMessage(1); return 0;
        }
    }
    int messageLoop() {
        HANDLE handle=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if (!handle) handle=CreateWaitableTimerW(nullptr,FALSE,nullptr);
        const std::unique_ptr<void,decltype(&CloseHandle)> timer(handle,CloseHandle);
        gameDeadline_=noticeEnd_;
        auto frameDeadline=Clock::now();
        int frameLimit=display_.frameLimit();
        while (true) {
            MSG msg{};
            // Give the clocks a turn even if the input queue stays busy.
            for (int i=0;i<32 && PeekMessageW(&msg,nullptr,0,0,PM_REMOVE);++i) {
                if (msg.message==WM_QUIT) return int(msg.wParam);
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            RECT client{};GetClientRect(hwnd_,&client);display_.configureViewport(client.right,client.bottom);updateCursorLock();
            advanceSimulation(Clock::now());
            const auto now=Clock::now();
            if (now>=gameDeadline_) continue;
            if (frameLimit!=display_.frameLimit()) {
                frameLimit=display_.frameLimit();
                frameDeadline=now;
            }
            const bool renderable=!showingNotice_ && !IsIconic(hwnd_);
            if (renderable && (!frameLimit || now>=frameDeadline)) {
                if (frameLimit) {
                    const auto interval=std::chrono::nanoseconds(1'000'000'000/frameLimit);
                    // Advance the frame schedule past missed slots without drawing them.
                    frameDeadline+=interval*((now-frameDeadline)/interval+1);
                }
                InvalidateRect(hwnd_,nullptr,FALSE);
                UpdateWindow(hwnd_);
                refreshPointer();
                continue;
            }

            const auto wake=renderable ? std::min(gameDeadline_,frameDeadline) : gameDeadline_;
            const auto remaining=wake-Clock::now();
            if (remaining<=Clock::duration::zero()) continue;
            LARGE_INTEGER due{};
            due.QuadPart=-std::max<LONGLONG>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count()/100);
            const bool armed=timer && SetWaitableTimer(handle,&due,0,nullptr,nullptr,FALSE);
            const auto timeout=DWORD(std::chrono::ceil<std::chrono::milliseconds>(remaining).count());
            if (MsgWaitForMultipleObjectsEx(armed ? 1 : 0,armed ? &handle : nullptr,armed ? INFINITE : timeout,
                                            QS_ALLINPUT,MWMO_INPUTAVAILABLE)==WAIT_FAILED)
                throw std::runtime_error("Windows message wait failed");
        }
    }
public:
    Window(const fs::path& dir,MenuAssets& assets,bool diagnostic,const fs::path& trace,const fs::path& saveDir,
           const fs::path& frameTiming,bool software,const fs::path& capture)
        :directory_(dir),saveDirectory_(saveDir),capturePath_(capture),saved_(loadProgress(saveDir)),assets_(assets),
         menu_(assets,saved_.has_value()),soundPlayer_(dir.parent_path()/"audio",logEvent),renderer_(assets),hardware_(assets),
         studioLogo_((dir.parent_path()/"branding/max-games.png").c_str()),diagnostic_(diagnostic),forceSoftware_(software) {
        logEvent("SAVE directory="+saveDir.string()+" valid="+std::to_string(saved_.has_value()));
        if (studioLogo_.GetLastStatus()!=gd::Ok) throw std::runtime_error("Cannot load Max Games logo");
        if (!trace.empty()) tutorialTrace_.open(trace);
        if (!frameTiming.empty()) {
            frameTiming_.open(frameTiming);
            if (!frameTiming_) throw std::runtime_error("Cannot write frame timings");
            frameTiming_<<"interval_ms\trender_ms\tgpu\tscene\tfps_limit\ttutorial_tick\tvsync\n";
        }
    }
    int run(HINSTANCE instance) {
        logEvent("WINDOW creating title=DeadWarrior 2 dpi="+std::to_string(GetDpiForSystem()));
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.hInstance=instance; wc.lpfnWndProc=dispatch; wc.lpszClassName=L"DeadWarrior2Native";
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        auto icon=[&](int width,int height) {
            return static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_DEADWARRIOR),IMAGE_ICON,
                                                GetSystemMetrics(width),GetSystemMetrics(height),LR_SHARED));
        };
        wc.hIcon=icon(SM_CXICON,SM_CYICON);
        wc.hIconSm=icon(SM_CXSMICON,SM_CYSMICON);
        if (!RegisterClassExW(&wc)) throw std::runtime_error("Window class registration failed");
        RECT size{0,0,1280,720};
        AdjustWindowRectExForDpi(&size,WS_OVERLAPPEDWINDOW,FALSE,0,GetDpiForSystem());
        if (!CreateWindowW(wc.lpszClassName,L"DeadWarrior 2",WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,nullptr,nullptr,instance,this))
            throw std::runtime_error("Cannot create the game window");
        const bool loaded=display_.loadOptions(saveDirectory_/L"options.ini");
        logEvent(std::string("OPTIONS load=")+(loaded ? "ok" : "defaults-or-partial")+" file="+(saveDirectory_/L"options.ini").string());
        display_.restore(hwnd_);
        saveOptions();
        ShowWindow(hwnd_,capturePath_.empty() ? SW_SHOW : SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd_);
        noticeEnd_=Clock::now()+noticeDuration;
        const int result=messageLoop();
        soundPlayer_.stopMusic();
        soundPlayer_.stopNarration();
        return result;
    }
};
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc=0;
    auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::vector<std::wstring> args(argv,argv+argc);
    LocalFree(argv);
    try {
        wchar_t path[32768]{};
        GetModuleFileNameW(nullptr,path,32768);
        initializeLog(fs::path(path).parent_path());
        logEvent("BUILD " __DATE__ " " __TIME__);
        GdiSession session;
        const auto dir=fs::path(path).parent_path()/"assets/menu";
        MenuAssets assets(dir);
        bool diagnostic=std::find(args.begin(),args.end(),L"--magenta")!=args.end();
        if (args.size()>=6 && args[1]==L"--capture") {
            int w=std::stoi(args[2]),h=std::stoi(args[3]),ticks=std::stoi(args[4]);
            if (w<1 || h<1 || ticks<0) throw std::runtime_error("Invalid capture arguments");
            CampaignMenu menu(assets);
            for (int i=0;i<ticks;++i) menu.tick();
            Renderer renderer(assets);
            gd::Bitmap image(w,h,PixelFormat32bppARGB);
            { gd::Graphics g(&image); renderer.draw(g,menu.drawList(),w,h,diagnostic); }
            savePng(image,args[5]); return 0;
        }
        fs::path trace;
        const auto option=std::find(args.begin(),args.end(),L"--trace-tutorial");
        if (option!=args.end() && std::next(option)!=args.end()) trace=*std::next(option);
        const auto saveOption=std::find(args.begin(),args.end(),L"--save-dir");
        const fs::path saveDir=saveOption!=args.end() && std::next(saveOption)!=args.end() ?
            fs::absolute(*std::next(saveOption)) : defaultSaveDirectory();
        fs::path frameTiming;
        const auto timing=std::find(args.begin(),args.end(),L"--frame-timing");
        if (timing!=args.end() && std::next(timing)!=args.end()) frameTiming=*std::next(timing);
        const bool software=std::find(args.begin(),args.end(),L"--software-renderer")!=args.end();
        fs::path capture;
        const auto captureOption=std::find(args.begin(),args.end(),L"--capture-client");
        if (captureOption!=args.end() && std::next(captureOption)!=args.end()) capture=fs::absolute(*std::next(captureOption));
        return Window(dir,assets,diagnostic,trace,saveDir,frameTiming,software,capture).run(instance);
    } catch (const std::exception& e) {
        logEvent(std::string("FATAL startup/run error=")+e.what());
        if (args.size()==1) MessageBoxA(nullptr,e.what(),"DeadWarrior 2",MB_OK|MB_ICONERROR);
        return 1;
    }
}
