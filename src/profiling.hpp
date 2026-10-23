#pragma once
#include <array>
namespace performance {
enum Stage { Update, Combat, World, Queries, Supercharge, AutoBattle, Prepare, Discovery, Hud, SuperDiscovery, SuperUnits, SuperPoints, SuperSet, Count };
struct Counter { std::atomic<unsigned long long> calls{0},ticks{0},peak{0}; };
static std::array<Counter,Count> counters;
static bool enabled=false;
static LONGLONG frequency=1;
static void configure(bool value) {
    enabled=value;
    LARGE_INTEGER result{};
    if(QueryPerformanceFrequency(&result)) frequency=result.QuadPart;
}
struct Scope {
    Stage stage;
    LARGE_INTEGER start{};
    explicit Scope(Stage value):stage(value) { if(enabled) QueryPerformanceCounter(&start); }
    ~Scope() {
        if(!enabled) return;
        LARGE_INTEGER end{}; QueryPerformanceCounter(&end);
        auto elapsed=static_cast<unsigned long long>(end.QuadPart-start.QuadPart);
        auto& counter=counters[stage];
        counter.ticks.fetch_add(elapsed,std::memory_order_relaxed);
        counter.calls.fetch_add(1,std::memory_order_relaxed);
        auto peak=counter.peak.load(std::memory_order_relaxed);
        while(elapsed>peak && !counter.peak.compare_exchange_weak(peak,elapsed,std::memory_order_relaxed)) {}
    }
};
static void report() {
    if(!enabled) return;
    static ULONGLONG previous=0;
    auto now=GetTickCount64();
    if(now-previous<5000) return;
    previous=now;
    constexpr const char* names[]{"field update","combat update","world timing","scene/battle queries",
        "Supercharge","Auto-Battle","Auto-Battle prepare","worker discovery","game HUD",
        "Supercharge metadata","Supercharge units","Supercharge cached points","Supercharge refill setters"};
    for(int i=0;i<Count;++i) {
        auto calls=counters[i].calls.exchange(0);
        auto ticks=counters[i].ticks.exchange(0);
        auto peak=counters[i].peak.exchange(0);
        if(!calls) continue;
        char line[180];
        std::snprintf(line,sizeof(line),"Performance %s: calls=%llu avg=%.3fms max=%.3fms.",
            names[i],calls,ticks*1000.0/frequency/calls,peak*1000.0/frequency);
        log(line);
    }
}
}
