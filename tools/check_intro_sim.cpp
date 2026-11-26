#include "intro_movie.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace sw2;
int main(int argc,char** argv) {
    try {
        const auto root=std::filesystem::absolute(argc>1?argv[1]:"work/intro-probe-fixture");
        std::filesystem::create_directories(root);
        auto require=[](bool value,const char* text){if(!value)throw std::runtime_error(text);};
        const auto start=std::chrono::steady_clock::time_point{};
        using namespace std::chrono_literals;
        IntroClock clock;clock.begin(start);
        require(clock.seconds(false,0,10,start+2s)==2,"Silent fallback clock failed");
        clock.pause(true,start+2s);
        require(clock.seconds(false,0,10,start+8s)==2,"Paused fallback clock advanced");
        clock.pause(false,start+8s);
        require(clock.seconds(false,0,10,start+9s)==3,"Fallback resume included paused time");
        require(clock.seconds(true,4,10,start+9s)==4,"Audio clock did not control presentation");
        require(clock.seconds(true,9.9,10,start+9s)==9.9&&clock.seconds(false,0,10,start+9s)==10,"One-shot audio completion failed");
        clock.begin(start+10s);
        require(clock.seconds(false,0,10,start+11s)==1,"Restart retained the previous audio clock");
        std::ofstream(root/"frames.tsv")<<"DeadWarrior2-Intro 1\n2 1 30 3 3 5\nmusic.wav\n3 K key.png\n4 D delta.png\n5 K final.png\n";
        std::ofstream(root/"events.tsv")<<"4 audio/a.wav\n4 audio/b.wav\n5 audio/c.wav\n";
        int decodes=0;
        IntroMovie movie(root,[&](const std::filesystem::path& file) {
            ++decodes;
            if(file.filename()=="key.png")return Raster{2,1,{10,20,30,255,40,50,60,255}};
            if(file.filename()=="delta.png")return Raster{2,1,{1,2,3,255,4,5,6,255}};
            if(file.filename()=="final.png")return Raster{2,1,{90,80,70,255,60,50,40,255}};
            throw std::runtime_error("Unexpected intro decode");
        });
        require(movie.available()&&movie.duration()==.1,"Intro timing header differs");
        movie.start();const auto original=movie.pixels();
        require(movie.sourceFrame()==3&&decodes==1,"First authored frame was not loaded");
        movie.advance(1./30);
        require(movie.sourceFrame()==4&&movie.pixels()->rgba==std::vector<std::uint8_t>({11,22,29,255,44,55,58,255}),"RGB XOR delta differs");
        require(original->rgba[0]==10&&movie.pixels()!=original,"Published frame pixels were mutated");
        auto sounds=movie.takeEffects();
        require(sounds.size()==2&&sounds[0].filename()=="a.wav"&&sounds[1].filename()=="b.wav","Simultaneous source sound events lost order");
        movie.advance(1./30);movie.advance(0);
        require(decodes==2&&movie.takeEffects().empty()&&movie.sourceFrame()==4,"Repeated renders or clock jitter advanced the movie");
        movie.advance(2./30);
        require(movie.sourceFrame()==5&&movie.pixels()->rgba[0]==90,"A later key frame failed to replace the canvas");
        require(movie.takeEffects().size()==1,"Final source effect was not dispatched");
        movie.advance(.1);require(movie.finished(),"Movie did not finish at its authored duration");
        movie.seek(0);require(movie.sourceFrame()==3&&!movie.finished(),"Backward seek failed");
        movie.seek(1./30);require(movie.sourceFrame()==4&&movie.pixels()->rgba[0]==11&&movie.takeEffects().empty(),"Delta seek or stale effects differ");
        const auto image=movie.draw({-197.222,0,1244.444,700});
        require(image.pixels==movie.pixels()&&image.target.h==622.222&&image.target.y>38,"Uniform movie fitting failed");
        movie.stop();require(!movie.pixels(),"Stopped intro retained its current frame");
        IntroMovie absent(root/"missing",[](const auto&){return Raster{};});
        require(!absent.available(),"An absent optional legacy intro was reported playable");
        const auto bad=root/"invalid";std::filesystem::create_directories(bad);
        std::ofstream(bad/"frames.tsv")<<"DeadWarrior2-Intro 1\n2 1 30 1 3 3\nmusic.wav\n3 K ../outside.png\n";
        bool rejected=false;
        try {IntroMovie invalid(bad,[](const auto&){return Raster{};});}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Intro resource traversal was accepted");
        require(argc>2,"Missing lossless chunk fixture");
        const auto packed=root/"chunked";std::filesystem::create_directories(packed);
        std::filesystem::copy_file(argv[2],packed/"movie.dw2v",std::filesystem::copy_options::overwrite_existing);
        std::ofstream(packed/"frames.tsv")<<"DeadWarrior2-Intro 2\n8 4 30 70 3 72\nmusic.wav\nmovie.dw2v\n";
        std::ofstream(packed/"events.tsv")<<"33 audio/one.wav\n33 audio/two.wav\n72 audio/end.wav\n";
        // Three independent chunks: 30 identical slots coalesced to one visual,
        // 30 slots coalesced to three visuals, and ten distinct translucent slots.
        for(bool prefetch:{false,true}) {
            IntroMovie chunked(packed,[](const auto&)->Raster{throw std::runtime_error("Container opened a PNG frame");},prefetch);
            chunked.start();auto retained=chunked.pixels();
            auto check=[&](int slot) {
                const auto pixels=chunked.pixels();
                const std::array<std::uint8_t,4> color=slot<30?std::array<std::uint8_t,4>{12,24,36,255}:
                    slot<60?std::array<std::uint8_t,4>{std::uint8_t((slot-30)/10*60+40),80,120,255}:
                    std::array<std::uint8_t,4>{std::uint8_t((slot-60)*20),100,30,128};
                require(chunked.sourceFrame()==slot+3&&pixels&&pixels->width==8&&pixels->height==4,"Chunk logical source clock/canvas differs");
                for(std::size_t pixel=0;pixel<pixels->rgba.size();++pixel)
                    require(pixels->rgba[pixel]==color[pixel%4],"Chunk coalesced span RGBA differs");
            };
            for(int slot=0;slot<70;++slot) {
                chunked.advance((slot+.25)/30.);check(slot);
                const auto before=chunked.statistics().decodes;const auto pixels=chunked.pixels();
                chunked.advance((slot+.25)/30.);
                require(chunked.pixels()==pixels&&chunked.statistics().decodes==before,"Repeated container frame decoded again");
            }
            require(retained->rgba[0]==12,"Container mutated a published raster");
            auto events=chunked.takeEffects();require(events.size()==3&&events[0].filename()=="one.wav"&&events[1].filename()=="two.wav"&&events[2].filename()=="end.wav","Container events lost their source clock/order");
            chunked.advance(chunked.duration());require(chunked.finished(),"Container finish clock differs");
            for(int slot:{29,28,30,59,60,69,0,40,2,39,62,12}) {
                chunked.seek((slot+.25)/30.);check(slot);require(chunked.takeEffects().empty(),"Container seek replayed historical audio");
            }
            const auto statistics=chunked.statistics();
            require(statistics.codecDecodes>0&&statistics.maximumCodecRequestDecodes<=30,"Container decoder work exceeded its bound");
            chunked.stop();require(!chunked.pixels(),"Stopped container retained pixels");chunked.start();check(0);
        }
        std::cout<<"Authored 30fps clock, immutable RGB delta/key frames, seek, effects, fit and path checks passed\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
