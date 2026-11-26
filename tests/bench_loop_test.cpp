// Test of ps2x_bench::runLoop without the game: a fake runtime whose "game thread" advances the vsync tick at 60 Hz,
// toggles DISPFB1.DBY every field and swaps DISPFB1.FBP every 2nd field (an interlaced 30 FPS game) and burns one core. Expected: fields/s ~60, speed ~100%, flips/s ~30, disp/s ~60, cpu ~100%.
// Build from the repo root after scripts/setup_base.sh:
//   c++ -std=c++20 -O2 -Isource/godhand-recomp/runtime/ps2xRuntime/src/lib tests/bench_loop_test.cpp -lpthread
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
    FakeRuntime runtime;
    std::atomic<bool> stop{false};
    std::thread game([&] {
        pthread_setname_np(pthread_self(), "GameThread");
        auto next = std::chrono::steady_clock::now();
        while (!stop)
        {
            next += std::chrono::microseconds(16683);
            while (std::chrono::steady_clock::now() < next) {} // busy: one full core
            const uint64_t tick = runtime.mem.regs.vsyncTick.fetch_add(1) + 1;
            // Interlaced, 30 FPS game: DBY (bit 43) toggles every field, FBP (bits 0-8) swaps every 2nd field.
            const uint64_t fbp = (tick / 2) % 2 ? 0x100u : 0x0C0u;
            const uint64_t dby = static_cast<uint64_t>(tick % 2) << 43;
            __atomic_store_n(&runtime.mem.regs.dispfb1, fbp | dby, __ATOMIC_RELAXED);
        }
    });
    const auto start = std::chrono::steady_clock::now();
    ps2x_bench::runLoop(runtime, [&] { return std::chrono::steady_clock::now() - start > std::chrono::seconds(6); }, [](double) {}, [] {});
    stop = true;
    game.join();
}
