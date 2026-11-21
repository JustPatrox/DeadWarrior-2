#include "campaign_tutorial.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sw2;
namespace sw2 {
struct CampaignProbe {
    static void require(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
    static void reference(CampaignTutorial& g,const std::filesystem::path& file) {
        std::ifstream input(file);std::string name;int seed,kind,count=0;
        while(input>>name>>seed>>kind) {
            double before[8],otherScale,after[8],next;
            for(auto& n:before)input>>n;input>>otherScale;
            for(auto& n:after)input>>n;input>>next;
            require(bool(input),"Invalid original collision fixture");
            g.units_.clear();g.spawn(g.spearton,{before[0],before[1]});
            g.spawn(kind==8 ? g.giant : g.spearton,{before[4],before[5]},kind==3||kind==8);
            auto& u=g.units_[0];auto& v=g.units_[1];
            u.canFight=v.canFight=false;u.velocity={before[2],before[3]};v.velocity={before[6],before[7]};
            v.perspectiveScale=otherScale;
            if(kind==4)v.garrisoned=true;if(kind==5)u.garrisoned=true;
            if(kind==6)v.altitude=100;if(kind==7)v.mayWalkThrough=true;
            if(kind==8)u.hold=v.hold=true;if(kind==11)v.stunned=10;
            g.seedRandom(seed);g.updateUnit(u);
            const double actual[]={u.position.x,u.position.y,u.velocity.x,u.velocity.y,v.position.x,v.position.y,v.velocity.x,v.velocity.y};
            for(int i=0;i<8;++i) if(std::abs(actual[i]-after[i])>1e-8) {
                std::cerr<<name<<" component "<<i<<" native="<<actual[i]<<" original="<<after[i]<<'\n';
                throw std::runtime_error("Collision differs from original runtime readback");
            }
            require(std::abs(g.random()*4.656612875245797e-10-next)<1e-12,name+": random consumption changed");
            ++count;
        }
        require(count==13&&input.eof(),"Missing original collision readback");
        std::cout<<count<<" original single-update collision cases matched\n";
    }
    static void crowd(CampaignTutorial& g) {
        g.units_.clear();g.seedRandom(0);
        for(int i=0;i<24;++i) {
            g.spawn(g.spearton,{2400,125},true);g.units_.back().canFight=false;
        }
        for(int frame=0;frame<600;++frame)for(auto& u:g.units_)g.updateUnit(u);
        double x0=1e9,x1=-1e9,y0=1e9,y1=-1e9;int overlaps=0;
        for(size_t i=0;i<g.units_.size();++i) {
            const auto& u=g.units_[i];x0=std::min(x0,u.position.x);x1=std::max(x1,u.position.x);
            y0=std::min(y0,u.position.y);y1=std::max(y1,u.position.y);
            require(std::isfinite(u.position.x)&&std::isfinite(u.position.y)&&u.position.y>=0&&u.position.y<=g.depth_,"Crowd escaped map bounds");
            for(size_t j=0;j<i;++j)if(std::hypot(u.position.x-g.units_[j].position.x,u.position.y-g.units_[j].position.y)<5)++overlaps;
        }
        require(x1-x0>40&&y1-y0>40&&overlaps==0,"Coincident enemy troops remained stacked");
        std::cout<<"24-troop crowd dispersed: "<<x1-x0<<" x "<<y1-y0<<", close overlaps="<<overlaps<<'\n';
    }
};
}
int main(int argc,char** argv) {
    try {
        const std::filesystem::path root=argc>1?argv[1]:"assets";
        const std::filesystem::path fixture=argc>2?argv[2]:"tools/fixtures/original-collisions.tsv";
        MenuAssets art(root/"menu");
        for(const char* f:{"tutorial","archidon","shadow","magic","rebels","giants","bomber","knight","interaction"})art.loadTextures(root/f);
        CampaignProgress progress;progress.level=6;progress.points=6;
        CampaignTutorial g(art,root,progress);CampaignProbe::reference(g,fixture);CampaignProbe::crowd(g);
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
