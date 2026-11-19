#pragma once
#include "campaign_progress.hpp"

namespace sw2 {
class CampaignIntermission {
public:
    enum class Page { Upgrades, Map };
    enum class Event { Click, Equip, Arrived, Save, Online, Guide, StartLevel };
    CampaignIntermission(MenuAssets&,const std::filesystem::path&,CampaignProgress&,Page);
    void tick();
    void point(std::optional<Point>);
    void press();
    void release();
    std::vector<Draw> drawList() const;
    std::vector<Event> takeEvents();
    void saved(bool ok) { savePrompt_=ok; }
    Page page() const { return page_; }
    int mapFrame() const { return mapFrame_; }
    bool confirming() const {return confirm_;}
    bool savePrompt() const {return savePrompt_;}
private:
    struct Node {int symbol{},parent{}; std::string name,tip;};
    struct Motion {double scale{},x{},y{},flagX{},flagY{};};
    struct Element {int id{}; Rect bounds{}; double scale{1}; Point origin{};};
    const MenuAssets& art_;
    CampaignProgress& campaign_;
    Page page_;
    std::array<Node,CampaignProgress::upgradeCount+1> nodes_{};
    std::vector<Motion> track_;
    std::optional<Point> pointer_;
    int pressed_{},tip_{},mapFrame_{1},flagTick_{};
    bool confirm_{},savePrompt_{};
    std::vector<Event> events_;
    int nodeFrame(int index) const;
    int target() const;
    Element element(int id,Point origin={},double scale=1,int frame=1) const;
    Element child(int owner,int id,Point origin={},double scale=1,int frame=1) const;
    std::vector<Element> controls() const;
    void enterMap();
};
}
