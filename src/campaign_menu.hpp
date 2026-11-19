#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sw2 {
struct Point { double x{}, y{}; };
struct Rect {
    double x{}, y{}, w{}, h{};
    bool contains(Point p) const {
        return p.x >= x && p.y >= y && p.x < x+w && p.y < y+h;
    }
};

struct Viewport {
    static constexpr double height = 700, width = height * 16 / 9;
    static constexpr double left = (850 - width) / 2, right = left + width;
    double scale;
    Rect pixels;
    Viewport(int clientWidth, int clientHeight);
    std::optional<Point> toStage(Point physical) const;
};

struct Placement {
    int owner{}, symbol{}, frame{}, depth{};
    std::string name;
    double sx{}, sy{}, x{}, y{};
    Rect bounds;
};

struct Art {
    std::filesystem::path file;
    Rect bounds;
};

class MenuAssets {
public:
    explicit MenuAssets(const std::filesystem::path& directory);
    void loadTextures(const std::filesystem::path& directory);
    void loadPlacements(const std::filesystem::path& directory);
    const Placement& placement(int owner, int symbol) const;
    const Art& texture(int symbol, int frame = 1) const;
    double alpha(int symbol, int frame) const;
    std::map<std::pair<int,int>,Art> art;
    std::map<int,std::vector<Placement>> placements;
private:
    std::map<std::pair<int,int>,double> alphas;
};

struct Draw {
    // Zero denotes an authored full-stage black field (base, modal overlay).
    int symbol{}, frame{1};
    Rect target;
    double alpha{1};
    enum class Kind { Image, Box, Ellipse, Text, Outline, Polygon, Line };
    Kind kind{Kind::Image};
    unsigned color{};
    std::string text;
    double fontSize{};
    bool centered{}, mirrored{};
    double rotation{};
    int redOffset{};
    std::vector<Point> vertices;
    double lineWidth{1};
    static Draw shape(Kind kind,Rect target,unsigned color,double opacity=1);
    static Draw label(Rect target,std::string text,double size,unsigned color,bool centered=false);
};
enum class Page { Opening, Main, NewOrContinue, Difficulty, Intro, CampaignMap };
enum class Action { None, ClickSound, MusicToggle, Online, FirstGame, Stickpage, IntroLink, Map, LoadCampaign, Unavailable };
enum class Difficulty { Normal, Hard, Insane };
struct Hit { int symbol; Rect bounds; int maskFrame{}; };

// Translated from com.brockw.stickwar.campaign.CampaignMenuScreen.
class CampaignMenu {
public:
    explicit CampaignMenu(const MenuAssets& assets, bool hasSave = false);
    void restart(bool hasSave);
    void tick();
    void hover(int symbol) { hover_ = symbol; }
    void press(int symbol);
    void release(int symbol);
    std::vector<Draw> drawList() const;
    std::vector<Hit> hits() const;
    std::vector<Action> takeActions();
    Page page() const { return page_; }
    bool music() const { return music_; }
    void setMusic(bool enabled) { music_=enabled; }
    Difficulty difficulty() const { return difficulty_; }
private:
    const MenuAssets& assets_;
    Page page_{Page::Opening};
    Difficulty difficulty_{Difficulty::Normal};
    int frame_{1}, hover_{}, pressed_{}, delay_{}, pending_{}, introTicks_{};
    bool credits_{}, music_{true}, hasSave_;
    double overlay_{};
    std::array<double,4> panelY_{};
    std::vector<Action> actions_;
    void activate(int symbol);
    int panel() const;
    int buttonFrame(int id) const;
    void compose(std::vector<Draw>& draws, std::vector<Hit>& hits) const;
};
}
