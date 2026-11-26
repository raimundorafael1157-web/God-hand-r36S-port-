// Test of ps2x_bench::runLoop without the game: a fake runtime whose "game thread" advances the vsync tick at 60 Hz,
// toggles DISPFB1.DBY every field and swaps DISPFB1.FBP every 2nd field (an interlaced 30 FPS game) and burns one core. Expected: fields/s ~60, speed ~100%, frames/s ~30, flips/s ~30, disp/s ~60, worst ~33 ms, cpu ~100%.
// The fake game thread also marks sections (ps2_prof.h): each field spends its first quarter in vu1 (exact counters),
// then 2000 short "dispatch" calls of ~1 us each (sampled counters, ~12% of a field), and the rest in ee.
// Expected on the game: part of each [bench] line and in [bench-sections]: vu1 ~25%, dispatch ~12%, ee ~63%.
// Build from the repo root after scripts/setup_base.sh:
//   c++ -std=c++20 -O2 -Isource/godhand-recomp/runtime/ps2xRuntime/src/lib -Isource/godhand-recomp/runtime/ps2xRuntime/include tests/bench_loop_test.cpp -lpthread
#include "ps2_bench.h"
#include <atomic>
#include <thread>

struct FakeGs
{
    std::atomic<uint64_t> vsyncTick{0};
    uint64_t dispfb1 = 0;
    uint64_t dispfb2 = 0;
};
struct FakeMemory
{
    FakeGs regs;
    FakeGs &gs() { return regs; }
};
struct FakeRuntime
{
    FakeMemory mem;
    FakeMemory &memory() { return mem; }
};

int main()
{
    setenv("PS2X_BENCH", "1", 1);          // the section counters run only in bench mode,
    setenv("PS2X_PROF", "1", 1);           // and only when asked for
    setenv("PS2X_BENCH_WARMUP", "2", 0);   // a 6 s run: keep 4 s for the summary
    FakeRuntime runtime;
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> frames{0};
    std::thread game([&] {
        pthread_setname_np(pthread_self(), "GameThread");
        ps2x_prof::registerGameThread();
        auto next = std::chrono::steady_clock::now();
        while (!stop)
        {
            const auto fieldStart = next;
            next += std::chrono::microseconds(16683);
            {
                const ps2x_prof::Scope vu1(ps2x_prof::VU1);
                while (std::chrono::steady_clock::now() < fieldStart + std::chrono::microseconds(4171)) {}
            }
            for (int call = 0; call < 2000; ++call)
            {
                ps2x_prof::SampledScope dispatch(ps2x_prof::Dispatch);
                const auto until = std::chrono::steady_clock::now() + std::chrono::nanoseconds(1000);
                while (std::chrono::steady_clock::now() < until) {}
            }
            while (std::chrono::steady_clock::now() < next) {} // busy: one full core
            const uint64_t tick = runtime.mem.regs.vsyncTick.fetch_add(1) + 1;
            // Interlaced, 30 FPS game: DBY (bit 43) toggles every field, FBP (bits 0-8) swaps every 2nd field.
            const uint64_t fbp = (tick / 2) % 2 ? 0x100u : 0x0C0u;
            const uint64_t dby = static_cast<uint64_t>(tick % 2) << 43;
            __atomic_store_n(&runtime.mem.regs.dispfb1, fbp | dby, __ATOMIC_RELAXED);
            if (tick % 2 == 0)
                frames.fetch_add(1); // the port's per-frame hook (God Hand: its flip copy builder)
        }
    });
    const auto start = std::chrono::steady_clock::now();
    ps2x_bench::runLoop(runtime, [&] { return std::chrono::steady_clock::now() - start > std::chrono::seconds(6); }, [](double) {}, [] {},
                        [&] { return frames.load(); });
    stop = true;
    game.join();
}
