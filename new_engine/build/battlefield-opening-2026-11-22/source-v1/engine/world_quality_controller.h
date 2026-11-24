#ifndef VALWOOD_WORLD_QUALITY_CONTROLLER_H
#define VALWOOD_WORLD_QUALITY_CONTROLLER_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Pure CPU policy: the caller owns render-target sizes, GPU queries and the
// resize/loading barrier. cpu_ms is active CPU work, excluding present/vsync.
// wall_dt is the source frame's elapsed cadence in seconds (including pacing),
// not an FPS value or CPU/GPU duration. Keep it paired with that GPU serial.
class QualityController {
public:
    struct Settings {
        bool enabled=true;
        float scale=1.f,min_scale=.70f,target_fps=60.f; // scale is the user's quality ceiling.
        // Runtime target may be a known VSync refresh below the saved UI's30FPS
        // minimum. Respect that actual cadence rather than fighting the display.
    };
    struct Sample {
        std::uint64_t serial=0,epoch=0;
        double gpu_ms=0,cpu_ms=0;
        bool eligible=false;
        double wall_dt=0;
    };
    struct Status {
        bool enabled=true,ready=false,cpu_limited=false,floor_limited=false;
        float effective_scale=1.f,min_scale=.70f,max_scale=1.f,target_fps=60.f;
        double filtered_gpu_ms=0,filtered_cpu_ms=0,filtered_frame_ms=0,frame_budget_ms=1000./60.,gpu_budget_ms=1000./60.-1.5;
        double over_budget_s=0,headroom_s=0,cooldown_s=0;
        std::uint64_t epoch=0,last_serial=0,accepted_samples=0,stale_samples=0;
        std::uint64_t duplicate_samples=0,excluded_samples=0,invalid_samples=0,resizes=0;
    };

    QualityController() noexcept {reset(Settings{},0);}

    void reset(Settings settings,std::uint64_t epoch) noexcept {
        status_=Status{};
        status_.enabled=settings.enabled;
        const float ceiling=finite_clamp(settings.scale,.70f,1.f,1.f);
        const float floor=std::min(ceiling,finite_clamp(settings.min_scale,.70f,1.f,.70f));
        // Exact endpoints keep arbitrary UI percentages inside the user's
        // interval. Standard five-percent steps fill only its interior.
        levels_={};level_count_=0;levels_[level_count_++]=ceiling;
        for(float step:standard_levels_)if(step<ceiling&&step>floor)levels_[level_count_++]=step;
        if(floor<ceiling)levels_[level_count_++]=floor;
        maximum_index_=0;minimum_index_=level_count_-1;index_=maximum_index_;
        status_.effective_scale=levels_[index_];status_.min_scale=levels_[minimum_index_];status_.max_scale=levels_[maximum_index_];
        status_.target_fps=finite_clamp(settings.target_fps,1.f,240.f,60.f);
        status_.frame_budget_ms=1000./status_.target_fps;
        status_.gpu_budget_ms=std::max(1.,status_.frame_budget_ms-1.5);
        status_.epoch=epoch;have_serial_=false;clear_window();
    }

    // Advance the epoch AFTER applying a changed target size/settings and
    // clearing pending GPU queries. Same/older epochs cannot reopen old samples.
    // Preserve scale and counters, and require fresh post-resize observations.
    bool reconfigure(std::uint64_t epoch) noexcept {
        if(epoch<=status_.epoch)return false;
        status_.epoch=epoch;status_.last_serial=0;have_serial_=false;
        clear_window();status_.cooldown_s=std::max(status_.cooldown_s,1.);
        return true;
    }

    // Returns true only when the requested scale changes. Every current-epoch
    // serial is consumed once, including invalid/excluded/manual samples.
    bool submit(const Sample &sample) noexcept {
        if(sample.epoch!=status_.epoch){status_.stale_samples++;return false;}
        if(have_serial_&&sample.serial<=status_.last_serial){status_.duplicate_samples++;return false;}
        have_serial_=true;status_.last_serial=sample.serial;
        if(!status_.enabled||!sample.eligible){status_.excluded_samples++;clear_window();return false;}
        if(!std::isfinite(sample.gpu_ms)||sample.gpu_ms<.05||sample.gpu_ms>250.||
           !std::isfinite(sample.cpu_ms)||sample.cpu_ms<0||sample.cpu_ms>1000.||
           !std::isfinite(sample.wall_dt)||sample.wall_dt<=0||sample.wall_dt>.25){
            status_.invalid_samples++;clear_window();return false;
        }
        status_.accepted_samples++;
        status_.cooldown_s=std::max(0.,status_.cooldown_s-sample.wall_dt);
        gpu_[cursor_]=sample.gpu_ms;cpu_[cursor_]=sample.cpu_ms;frame_[cursor_]=sample.wall_dt*1000.;cursor_=(cursor_+1)%gpu_.size();
        observations_=std::min(unsigned(gpu_.size()),observations_+1);
        if(observations_<gpu_.size())return false;

        const double gpu=median(gpu_),cpu=median(cpu_),frame=median(frame_);
        if(!status_.ready){status_.filtered_gpu_ms=gpu;status_.filtered_cpu_ms=cpu;status_.filtered_frame_ms=frame;status_.ready=true;}
        else{
            const double weight=1.-std::exp(-sample.wall_dt/.20);
            status_.filtered_gpu_ms+=weight*(gpu-status_.filtered_gpu_ms);
            status_.filtered_cpu_ms+=weight*(cpu-status_.filtered_cpu_ms);
            status_.filtered_frame_ms+=weight*(frame-status_.filtered_frame_ms);
        }
        const double budget=status_.gpu_budget_ms;
        status_.cpu_limited=status_.filtered_cpu_ms>status_.frame_budget_ms&&
                            status_.filtered_cpu_ms>status_.filtered_gpu_ms+1.;
        const bool stable_cadence=status_.filtered_frame_ms<=status_.frame_budget_ms*1.03;
        // Near-VSync timestamp spans alone do not prove GPU-limited misses.
        // Require actual missed cadence as well as GPU pressure for ordinary
        // reductions; active CPU work identifies misses resolution cannot fix.
        const bool gpu_limited_miss=!stable_cadence&&!status_.cpu_limited&&status_.filtered_gpu_ms>budget+.15;
        // A deliberate30FPS cap has a33ms budget; the old fixed45FPS alarm
        // incorrectly reduced quality even while that target was met. Keep
        // the prior severe threshold at60+FPS, but respect slower targets.
        const double severe_budget=std::max(1000./45.,status_.frame_budget_ms*1.15);
        status_.floor_limited=index_==minimum_index_&&(gpu_limited_miss||
            (!status_.cpu_limited&&status_.filtered_gpu_ms>severe_budget));
        status_.over_budget_s=gpu_limited_miss?
            std::min(10.,status_.over_budget_s+sample.wall_dt):std::max(0.,status_.over_budget_s-2.*sample.wall_dt);
        severe_s_=!status_.cpu_limited&&gpu>severe_budget?
            std::min(10.,severe_s_+sample.wall_dt):0.;
        const double next_scale=index_>maximum_index_?levels_[index_-1]:levels_[index_];
        const double predicted_up=status_.filtered_gpu_ms*std::pow(next_scale/scale(),2.);
        const bool headroom=index_>maximum_index_&&stable_cadence&&status_.filtered_gpu_ms<budget-3.&&gpu<budget-2.5&&predicted_up<budget-1.5;
        status_.headroom_s=headroom?std::min(10.,status_.headroom_s+sample.wall_dt):0.;
        if(status_.cooldown_s>0)return false;

        unsigned next=index_;
        if(severe_s_>=.04&&index_<minimum_index_){
            // Pixel-cost estimate is a fast first response, not an FPS promise:
            // geometry/fixed GPU cost may remain even at the resolution floor.
            const double estimated=scale()*std::sqrt(budget/std::max(gpu,status_.filtered_gpu_ms));
            next=index_+1;
            while(next<minimum_index_&&levels_[next]>estimated)next++;
        }else if(status_.over_budget_s>=.5&&index_<minimum_index_)next=index_+1;
        else if(status_.headroom_s>=4.&&index_>maximum_index_)next=index_-1;
        if(next==index_)return false;
        index_=next;status_.effective_scale=levels_[index_];status_.resizes++;
        clear_window();status_.cooldown_s=1.;return true;
    }

    float scale() const noexcept {return status_.effective_scale;}
    Status status() const noexcept {return status_;}

private:
    inline static constexpr std::array<float,7> standard_levels_={1.f,.95f,.90f,.85f,.80f,.75f,.70f};
    Status status_;
    std::array<float,9> levels_={};
    std::array<double,3> gpu_={},cpu_={},frame_={};
    unsigned index_=0,maximum_index_=0,minimum_index_=6,level_count_=0,cursor_=0,observations_=0;
    bool have_serial_=false;
    double severe_s_=0;

    static float finite_clamp(float value,float low,float high,float fallback) noexcept {
        return std::isfinite(value)?std::clamp(value,low,high):fallback;
    }
    static double median(std::array<double,3> values) noexcept {
        if(values[0]>values[1])std::swap(values[0],values[1]);
        if(values[1]>values[2])std::swap(values[1],values[2]);
        if(values[0]>values[1])std::swap(values[0],values[1]);
        return values[1];
    }
    void clear_window() noexcept {
        gpu_={};cpu_={};frame_={};cursor_=observations_=0;severe_s_=0;
        status_.ready=status_.cpu_limited=status_.floor_limited=false;
        status_.filtered_gpu_ms=status_.filtered_cpu_ms=status_.filtered_frame_ms=0;
        status_.over_budget_s=status_.headroom_s=0;
    }
};

#endif
