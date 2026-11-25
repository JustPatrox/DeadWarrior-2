#pragma once
#include "raster_compositor.hpp"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace sw2 {
// One CPU decoder worker, one normal UI consumer. Returned rasters/canvases are
// owned by the caller and do not count against the completed+in-flight bound.
class IntroFrameCache {
public:
    using Decoder=std::function<Raster(const std::filesystem::path&)>;

    IntroFrameCache(std::vector<std::filesystem::path> files,Decoder decoder,std::size_t capacity=8)
        :files_(std::move(files)),decoder_(std::move(decoder)),capacity_(capacity) {
        if(!capacity_)throw std::invalid_argument("Intro frame cache capacity must be positive");
        if(!decoder_)throw std::invalid_argument("Intro frame cache needs a decoder");
        worker_=std::thread([this]{run();});
    }
    IntroFrameCache(const IntroFrameCache&)=delete;
    IntroFrameCache& operator=(const IntroFrameCache&)=delete;

    ~IntroFrameCache() {
        {
            std::lock_guard lock(mutex_);
            stopping_=true;active_=false;++generation_;
        }
        changed_.notify_all();
        if(worker_.joinable())worker_.join();
    }

    Raster get(std::size_t index) {
        if(index>=files_.size())throw std::out_of_range("Intro frame cache index is out of range");
        std::unique_lock lock(mutex_);
        if(stopping_)throw std::runtime_error("Intro frame cache is stopping");
        // Ordinary forward consumption keeps useful work in flight. A seek
        // outside the current window or backwards invalidates its old job.
        if(!active_ || index<first_ || index>=end_)++generation_;
        active_=true;first_=wanted_=index;
        end_=index+std::min(capacity_,files_.size()-index);
        for(auto it=ready_.begin();it!=ready_.end();) {
            if(it->first<first_ || it->first>=end_)it=ready_.erase(it);
            else ++it;
        }
        const auto generation=generation_;
        changed_.notify_all();
        changed_.wait(lock,[&]{return stopping_ || generation_!=generation || ready_.contains(index);});
        // clear() may cancel a waiting get from another thread. Normal UI use
        // calls clear between gets; this path defines cancellation for tests.
        if(stopping_ || generation_!=generation)
            throw std::runtime_error("Intro frame request cancelled");
        auto found=ready_.find(index);
        wanted_=index+1;
        if(found->second.error) {
            const auto error=found->second.error;
            // Keep failed entries in this window so neither prefetch nor a
            // repeated get retries a failing decoder in a busy loop.
            changed_.notify_all();lock.unlock();std::rethrow_exception(error);
        }
        Raster result=std::move(found->second.pixels);
        ready_.erase(found);
        changed_.notify_all();
        return result;
    }

    // Invalidate queued/completed work immediately. An already executing PNG
    // decode cannot be interrupted; its result is discarded when it returns.
    void clear() {
        {
            std::lock_guard lock(mutex_);
            ++generation_;active_=false;ready_.clear();
        }
        changed_.notify_all();
    }

    // cached() counts completed slots, including stored exceptions.
    std::size_t cached() const {
        std::lock_guard lock(mutex_);return ready_.size();
    }
    // Peak reserved slots includes the worker's one in-flight decode.
    std::size_t peakCached() const {
        std::lock_guard lock(mutex_);return peak_;
    }

private:
    struct Result {Raster pixels;std::exception_ptr error;};
    struct Job {std::size_t index{};unsigned long long generation{};};
    const std::vector<std::filesystem::path> files_;
    const Decoder decoder_;
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::size_t,Result> ready_;
    std::thread worker_;
    bool stopping_{},active_{},loading_{};
    std::size_t first_{},wanted_{},end_{},peak_{};
    unsigned long long generation_{};
    static constexpr auto none=std::numeric_limits<std::size_t>::max();

    std::size_t next() const {
        if(!active_ || ready_.size()+std::size_t(loading_)>=capacity_)return none;
        for(auto index=wanted_;index<end_;++index)
            if(!ready_.contains(index))return index;
        return none;
    }
    void run() {
        std::unique_lock lock(mutex_);
        for(;;) {
            changed_.wait(lock,[&]{return stopping_ || next()!=none;});
            if(stopping_)return;
            const Job job{next(),generation_};
            loading_=true;peak_=std::max(peak_,ready_.size()+1);
            lock.unlock();
            Result result;
            try {result.pixels=decoder_(files_[job.index]);}
            catch(...) {result.error=std::current_exception();}
            lock.lock();
            loading_=false;
            if(!stopping_ && active_ && job.generation==generation_ &&
               job.index>=wanted_ && job.index<end_) {
                ready_.emplace(job.index,std::move(result));
                peak_=std::max(peak_,ready_.size());
            }
            changed_.notify_all();
            // Do not destroy a discarded large raster while holding the UI
            // mutex. Valid results were moved into their completed slot.
            lock.unlock();result={};lock.lock();
        }
    }
};
}
