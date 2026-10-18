#pragma once
// Opt-in timing only. Disabled scopes perform no QPC calls or allocations.
// Samples are atomic so a HUD snapshot can read another thread's Step timings.
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace ndship::perf {
enum class Metric : unsigned { FrameInterval, Player1Step, Player2Step, CodeCallback, Hud, ContextQuery, Count };
inline constexpr std::size_t Window=512;
struct Summary {
    std::uint64_t total_samples=0;
    unsigned window_samples=0;
    double latest_ms=0,mean_ms=0,p50_ms=0,p95_ms=0,p99_ms=0,max_ms=0;
};
namespace detail {
struct Slot { std::atomic<std::int64_t> ticks{0};std::atomic<std::uint64_t> sequence{0}; };
struct Series { std::array<Slot,Window> slots{};std::atomic<std::uint64_t> total{0}; };
inline std::array<Series,static_cast<unsigned>(Metric::Count)> series{};
inline std::atomic<bool> enabled{false};
inline std::atomic<std::int64_t> previous_frame{0};
inline std::int64_t Now() noexcept {LARGE_INTEGER value{};QueryPerformanceCounter(&value);return value.QuadPart;}
inline double MillisecondsPerTick() noexcept {
    static const double scale=[](){LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);return frequency.QuadPart>0?1000.0/static_cast<double>(frequency.QuadPart):0.0;}();
    return scale;
}
}
inline bool Enabled() noexcept {return detail::enabled.load(std::memory_order_relaxed);}
inline void SetEnabled(bool enabled) noexcept {
    detail::previous_frame.store(0,std::memory_order_relaxed);
    detail::enabled.store(enabled,std::memory_order_relaxed);
}
inline void RecordTicks(Metric metric,std::int64_t ticks) noexcept {
    const auto index=static_cast<unsigned>(metric);
    if(!Enabled()||index>=static_cast<unsigned>(Metric::Count)||ticks<0)return;
    auto& series=detail::series[index];
    const auto number=series.total.fetch_add(1,std::memory_order_relaxed);
    auto& slot=series.slots[number%Window];
    slot.sequence.store(0,std::memory_order_release);
    slot.ticks.store(ticks,std::memory_order_relaxed);
    slot.sequence.store(number+1,std::memory_order_release);
}
class Span {
    Metric metric_;
    std::int64_t start_=0;
    bool recording_=false;
public:
    explicit Span(Metric metric) noexcept : metric_(metric),recording_(Enabled()) {if(recording_)start_=detail::Now();}
    ~Span() {if(recording_)RecordTicks(metric_,detail::Now()-start_);}
    Span(const Span&)=delete;
    Span& operator=(const Span&)=delete;
};
// Call once per displayed frame, outside the Step/HUD duration spans. This
// measures frame intervals, not a claim about GPU utilization or presentation.
inline void MarkFrame() noexcept {
    if(!Enabled())return;
    const auto now=detail::Now();
    const auto previous=detail::previous_frame.exchange(now,std::memory_order_relaxed);
    if(previous>0&&now>=previous)RecordTicks(Metric::FrameInterval,now-previous);
}
// Sorting occurs only when a summary is requested; never call this per event.
// At most512 valid recent samples are copied onto the caller's stack.
inline Summary Read(Metric metric) noexcept {
    Summary result;
    const auto index=static_cast<unsigned>(metric);
    if(index>=static_cast<unsigned>(Metric::Count))return result;
    const auto& series=detail::series[index];
    const auto total=series.total.load(std::memory_order_acquire);
    result.total_samples=total;
    const auto begin=total>Window?total-Window:0;
    std::array<std::int64_t,Window> samples{};
    std::uint64_t latest_sequence=0;
    double sum=0;
    for(auto number=begin;number<total;++number) {
        const auto& slot=series.slots[number%Window];
        const auto before=slot.sequence.load(std::memory_order_acquire);
        if(before!=number+1)continue;
        const auto value=slot.ticks.load(std::memory_order_relaxed);
        if(slot.sequence.load(std::memory_order_acquire)!=before)continue;
        samples[result.window_samples++]=value;sum+=static_cast<double>(value);
        if(before>latest_sequence){latest_sequence=before;result.latest_ms=value*detail::MillisecondsPerTick();}
    }
    if(!result.window_samples)return result;
    const auto count=result.window_samples;
    std::sort(samples.begin(),samples.begin()+count);
    const double scale=detail::MillisecondsPerTick();
    auto percentile=[&](unsigned percent){const unsigned rank=(count*percent+99)/100;return samples[(rank?rank:1)-1]*scale;};
    result.mean_ms=sum/count*scale;
    result.p50_ms=percentile(50);result.p95_ms=percentile(95);result.p99_ms=percentile(99);result.max_ms=samples[count-1]*scale;
    return result;
}
// Disable collection before Reset to make its intended window boundary exact.
inline void Reset() noexcept {
    detail::previous_frame.store(0,std::memory_order_relaxed);
    for(auto& series:detail::series) {
        series.total.store(0,std::memory_order_relaxed);
        for(auto& slot:series.slots)slot.sequence.store(0,std::memory_order_relaxed);
    }
}
} // namespace ndship::perf
