#include "intro_frame_cache.hpp"
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <set>
#include <sstream>

using namespace std::chrono_literals;
using sw2::IntroFrameCache;
using sw2::Raster;

namespace {
void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
std::vector<std::filesystem::path> files() {
    std::vector<std::filesystem::path> result;
    for(int i=0;i<40;++i)result.emplace_back(std::to_string(i));
    return result;
}
struct DecodeFailure:std::runtime_error {using std::runtime_error::runtime_error;};
class FakeDecoder {
    using Call=std::pair<std::size_t,unsigned>;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::size_t,unsigned> counts_;
    std::vector<Call> started_;
    std::set<Call> allowed_,finished_,failures_;
    bool allowAll_{};
    unsigned active_{},peakActive_{};
public:
    Raster decode(const std::filesystem::path& file) {
        const auto index=std::stoull(file.string());
        std::unique_lock lock(mutex_);
        const Call call{index,++counts_[index]};
        started_.push_back(call);++active_;peakActive_=std::max(peakActive_,active_);
        changed_.notify_all();
        changed_.wait(lock,[&]{return allowAll_ || allowed_.contains(call);});
        --active_;finished_.insert(call);changed_.notify_all();
        const bool fail=failures_.contains(call);
        lock.unlock();
        if(fail)throw DecodeFailure("prepared decode failure");
        return {2,1,{std::uint8_t(index),std::uint8_t(call.second),90,255,
                     std::uint8_t(index^0xa5),std::uint8_t(call.second+7),0,255}};
    }
    void open(std::size_t index,unsigned call=1) {
        std::lock_guard lock(mutex_);allowed_.insert({index,call});changed_.notify_all();
    }
    void openAll() {
        std::lock_guard lock(mutex_);allowAll_=true;changed_.notify_all();
    }
    void fail(std::size_t index,unsigned call=1) {
        std::lock_guard lock(mutex_);failures_.insert({index,call});
    }
    void started(std::size_t index,unsigned call=1) {
        std::unique_lock lock(mutex_);
        if(!changed_.wait_for(lock,2s,[&]{return counts_[index]>=call;})) {
            allowAll_=true;changed_.notify_all();throw std::runtime_error("Decoder did not receive its expected frame");
        }
    }
    unsigned count(std::size_t index) const {
        std::lock_guard lock(mutex_);auto p=counts_.find(index);return p==counts_.end()?0:p->second;
    }
    unsigned peakActive() const {std::lock_guard lock(mutex_);return peakActive_;}
    unsigned active() const {std::lock_guard lock(mutex_);return active_;}
    std::vector<Call> order() const {std::lock_guard lock(mutex_);return started_;}
};
struct Cleanup {
    FakeDecoder& decoder;IntroFrameCache& cache;
    ~Cleanup(){decoder.openAll();cache.clear();}
};
template<class T> T ready(std::future<T>& result,FakeDecoder& decoder,IntroFrameCache& cache) {
    if(result.wait_for(2s)!=std::future_status::ready) {
        decoder.openAll();cache.clear();throw std::runtime_error("Cache request did not complete");
    }
    return result.get();
}
std::future<Raster> request(IntroFrameCache& cache,std::size_t index) {
    return std::async(std::launch::async,[&cache,index]{return cache.get(index);});
}
void value(const Raster& image,std::size_t index,unsigned call) {
    const std::vector<std::uint8_t> expected{std::uint8_t(index),std::uint8_t(call),90,255,
                                           std::uint8_t(index^0xa5),std::uint8_t(call+7),0,255};
    require(image.width==2&&image.height==1&&image.rgba==expected,"Decoded pixels changed or a stale generation escaped");
}
void completed(IntroFrameCache& cache,std::size_t count,FakeDecoder& decoder) {
    const auto until=std::chrono::steady_clock::now()+2s;
    while(cache.cached()!=count) {
        if(std::chrono::steady_clock::now()>=until) {
            decoder.openAll();cache.clear();throw std::runtime_error("Completed-cache state did not arrive");
        }
        std::this_thread::yield();
    }
}
void sequential() {
    FakeDecoder decoder;
    IntroFrameCache cache(files(),[&](const auto& path){return decoder.decode(path);});
    Cleanup cleanup{decoder,cache};
    auto first=request(cache,0);decoder.started(0);decoder.openAll();value(ready(first,decoder,cache),0,1);
    decoder.started(7); // Seven future frames arrive without the consumer asking for them.
    for(std::size_t index=1;index<24;++index) {
        value(cache.get(index),index,1);
        require(cache.cached()<=8&&cache.peakCached()<=8,"Prefetch exceeded its eight-slot reservation bound");
    }
    require(cache.peakCached()>0&&decoder.peakActive()==1,"Cache did not use exactly one decoder worker");
    for(int i=0;i<24;++i)require(decoder.count(i)==1,"Sequential playback redundantly decoded a frame");
    std::cout<<"Sequential pixels and seven-frame lookahead match; ready+in-flight reservation stays <=8\n";
}
void seekAndClear() {
    FakeDecoder decoder;
    IntroFrameCache cache(files(),[&](const auto& path){return decoder.decode(path);},4);
    Cleanup cleanup{decoder,cache};
    auto first=request(cache,0);decoder.started(0);decoder.open(0);value(ready(first,decoder,cache),0,1);
    decoder.started(1);decoder.open(1);decoder.started(2);decoder.open(2);decoder.started(3);
    completed(cache,2,decoder);
    auto forward=request(cache,10);
    // Observe eviction before unblocking the old worker. This tests priority
    // without assuming the asynchronous get thread ran on a particular tick.
    completed(cache,0,decoder);decoder.open(3);decoder.started(10);
    const auto order=decoder.order();require(order.size()==5&&order[4].first==10,"Seek decoded the obsolete queue before its requested frame");
    decoder.open(10);value(ready(forward,decoder,cache),10,1);
    decoder.started(11);decoder.open(11);decoder.started(12);decoder.open(12);decoder.started(13);
    completed(cache,2,decoder);
    auto back=request(cache,1);completed(cache,0,decoder);decoder.open(13);decoder.started(1,2);decoder.open(1,2);
    value(ready(back,decoder,cache),1,2);decoder.started(2,2);
    auto clear=std::async(std::launch::async,[&]{cache.clear();});
    require(clear.wait_for(1s)==std::future_status::ready,"clear waited for a blocked PNG decoder");clear.get();
    require(cache.cached()==0,"clear retained completed frames");
    auto afterClear=request(cache,2);decoder.open(2,2);decoder.started(2,3);decoder.open(2,3);
    value(ready(afterClear,decoder,cache),2,3);
    require(cache.peakCached()<=4,"Seek/clear exceeded its configured bound");
    std::cout<<"Forward/back seeks reprioritize; clear returns while decoding and discards the old generation\n";
}
void cancellationAndErrors() {
    {
        FakeDecoder decoder;
        IntroFrameCache cache(files(),[&](const auto& path){return decoder.decode(path);},4);
        Cleanup cleanup{decoder,cache};
        auto waiting=request(cache,5);decoder.started(5);cache.clear();
        bool cancelled=false;
        try {ready(waiting,decoder,cache);}catch(const std::runtime_error& e){cancelled=std::string(e.what()).find("cancelled")!=std::string::npos;}
        require(cancelled,"A get waiting across clear did not report cancellation");
        auto replacement=request(cache,5);decoder.open(5);decoder.started(5,2);decoder.open(5,2);
        value(ready(replacement,decoder,cache),5,2);
    }
    {
        FakeDecoder decoder;decoder.fail(4);
        IntroFrameCache cache(files(),[&](const auto& path){return decoder.decode(path);},4);
        Cleanup cleanup{decoder,cache};
        auto first=request(cache,3);decoder.started(3);decoder.open(3);value(ready(first,decoder,cache),3,1);
        decoder.started(4);decoder.open(4);decoder.started(5);
        for(int repeat=0;repeat<3;++repeat) {
            bool failed=false;
            try {cache.get(4);}catch(const DecodeFailure& e){failed=std::string(e.what())=="prepared decode failure";}
            require(failed,"A prefetched failure was not rethrown at its own requested index");
        }
        require(decoder.count(4)==1,"A failed frame was retried automatically or by repeated gets");
        cache.clear();auto retry=request(cache,4);decoder.open(5);decoder.started(4,2);decoder.open(4,2);
        value(ready(retry,decoder,cache),4,2);
    }
    std::cout<<"Concurrent clear cancels waiting get; prefetched errors stay indexed and retry only after clear\n";
}
void forwardSkip() {
    FakeDecoder decoder;
    IntroFrameCache cache(files(),[&](const auto& path){return decoder.decode(path);});
    Cleanup cleanup{decoder,cache};
    auto first=request(cache,0);decoder.started(0);decoder.open(0);value(ready(first,decoder,cache),0,1);
    decoder.started(1);decoder.open(1);decoder.started(2);decoder.open(2);decoder.started(3);
    completed(cache,2,decoder);
    auto skipped=request(cache,5);completed(cache,0,decoder);decoder.open(3);decoder.started(5);
    const auto order=decoder.order();require(order.size()==5&&order.back().first==5,"A nearby forward skip decoded an obsolete intermediate frame");
    decoder.open(5);value(ready(skipped,decoder,cache),5,1);
    require(decoder.count(4)==0&&cache.peakCached()<=8,"Forward skip retained unrequested obsolete work");
    std::cout<<"Forward skip inside the old lookahead window prioritizes its current frame\n";
}
void destructionAndBounds() {
    FakeDecoder decoder;
    auto cache=std::make_unique<IntroFrameCache>(files(),[&](const auto& path){return decoder.decode(path);},1);
    auto waiting=request(*cache,8);decoder.started(8);cache->clear();
    bool cancelled=false;try {ready(waiting,decoder,*cache);}catch(const std::runtime_error&){cancelled=true;}
    require(cancelled,"Destructor fixture failed to cancel its consumer before destroying the cache");
    auto closing=std::async(std::launch::async,[&]{cache.reset();});
    decoder.openAll();
    require(closing.wait_for(2s)==std::future_status::ready,"Destructor did not join its in-flight decoder");closing.get();
    require(decoder.active()==0&&decoder.peakActive()==1,"Decoder still ran after destruction");
    bool zero=false;try {IntroFrameCache bad(files(),[](const auto&){return Raster{};},0);}catch(const std::invalid_argument&){zero=true;}
    require(zero,"A zero-capacity cache accepted requests it cannot satisfy");
    IntroFrameCache empty({},[](const auto&){return Raster{};});
    bool range=false;try {empty.get(0);}catch(const std::out_of_range&){range=true;}
    require(range&&empty.cached()==0,"An empty cache did not reject its index safely");
    IntroFrameCache single(files(),[](const auto& p){const auto i=std::stoi(p.string());return Raster{2,1,{std::uint8_t(i),1,90,255,std::uint8_t(i^0xa5),8,0,255}};},1);
    value(single.get(3),3,1);value(single.get(4),4,1);
    require(single.peakCached()==1&&single.cached()==0,"Capacity one did not move its current frame out of the cache");
    std::cout<<"Capacity one, invalid inputs and destruction during decode complete without deadlock\n";
}
}
int main() {
    // A failed synchronization test must not leave the test process hanging.
    std::mutex watchdogMutex;std::condition_variable watchdogChanged;bool done=false;
    std::thread watchdog([&]{std::unique_lock lock(watchdogMutex);if(!watchdogChanged.wait_for(lock,20s,[&]{return done;})){std::cerr<<"Intro cache probe deadlocked\n";std::_Exit(2);}});
    int status=0;
    try {sequential();seekAndClear();cancellationAndErrors();forwardSkip();destructionAndBounds();}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';status=1;}
    {std::lock_guard lock(watchdogMutex);done=true;}watchdogChanged.notify_all();watchdog.join();
    return status;
}
