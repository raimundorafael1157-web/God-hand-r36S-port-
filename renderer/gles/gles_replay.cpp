// gles_replay: plays a GS recording (PS2X_GS_RECORD) through the GLES backend with no window and reports, per game
// frame, the CPU time spent turning GS commands into GL calls and the time the GPU needs to finish them.
//
//   gles_replay <recording.gsr> [--dump <dir>] [--dump-every <n>] [--fbp <hex>] [--size <w>x<h>] [--repeat <n>]
//
// The recording is read one record at a time (a 2 s recording is ~640 MB; the R36S has 1 GB): reading the file is
// not counted in the times. --dump writes the frame buffer (default FBP 0x70, God Hand's draw buffer) as PPM.
#include "egl_headless.h"
#include "gs_gles_backend.h"
#include "runtime/gs/gs_cpu_backend.h"
#include "runtime/gs/gs_recording.h"

#define XXH_INLINE_ALL
#include "third_party/xxhash.h" // BSD-2-Clause, xxHash v0.8.2

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

    double percentile(std::vector<double> v, double p)
    {
        if (v.empty())
            return 0.0;
        std::sort(v.begin(), v.end());
        return v[std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1) + 0.5))];
    }

    bool writePpm(const std::string &path, const std::vector<uint8_t> &rgba, uint32_t w, uint32_t h)
    {
        std::FILE *f = std::fopen(path.c_str(), "wb");
        if (!f)
            return false;
        std::fprintf(f, "P6\n%u %u\n255\n", w, h);
        std::vector<uint8_t> row(static_cast<size_t>(w) * 3u);
        for (uint32_t y = 0; y < h; ++y)
        {
            const uint8_t *src = rgba.data() + static_cast<size_t>(y) * w * 4u;
            for (uint32_t x = 0; x < w; ++x)
                std::memcpy(&row[x * 3u], src + x * 4u, 3);
            std::fwrite(row.data(), 1, row.size(), f);
        }
        std::fclose(f);
        return true;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: gles_replay <recording.gsr> [--dump <dir>] [--dump-every <n>] [--fbp <hex>] "
                             "[--size <w>x<h>] [--repeat <n>] [--frames <n>]\n"
                             "       gles_replay --gl-info   (the device's EGL/GLES: vendor, version, extensions)\n"
                             "       gles_replay --hash-bench (page hash: current vs XXH3, on this CPU)\n");
        return 2;
    }
    // --gl-info: what the driver offers, for collect_hw_info.sh (no recording needed). Answers which driver the
    // firmware ships (ARM blob or Panfrost), whether the context comes from GBM, surfaceless or the default display,
    // and which extensions exist (EXT_buffer_storage, EXT_disjoint_timer_query...).
    // --hash-bench: the renderer hashes 8 KB VRAM pages (~3.8 MB per frame in the stage 1 fight). Times the current hash
    // and XXH3 on this CPU, so the R36S decides GLES_PAGE_HASH from its own numbers. No GL needed.
    if (std::string(argv[1]) == "--hash-bench")
    {
        constexpr size_t kPages = 512, kPage = 8192;
        std::vector<uint8_t> vram(kPages * kPage);
        uint64_t x = 0x9E3779B97F4A7C15ull;
        for (size_t i = 0; i < vram.size(); i += 8)
        {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
            std::memcpy(&vram[i], &x, 8);
        }
        const auto chain = [](const uint8_t *data, size_t size) {
            uint64_t h = 0x7Eu ^ (size * 0x100000001B3ull);
            for (size_t i = 0; i + 8 <= size; i += 8)
            {
                uint64_t w;
                std::memcpy(&w, data + i, 8);
                h = (h ^ w) * 0x100000001B3ull;
                h ^= h >> 29;
            }
            return h;
        };
        for (int round = 0; round < 3; ++round)
            for (int which = 0; which < 2; ++which)
            {
                uint64_t sink = 0;
                constexpr int kPasses = 40;
                const auto t0 = std::chrono::steady_clock::now();
                for (int pass = 0; pass < kPasses; ++pass)
                    for (size_t p = 0; p < kPages; ++p)
                        sink ^= which == 0 ? chain(vram.data() + p * kPage, kPage) : XXH3_64bits(vram.data() + p * kPage, kPage);
                const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                const double perPage = sec * 1e9 / (kPasses * kPages);
                std::printf("[hash-bench] round %d %-8s %8.1f ns/page %6.2f GB/s -> %5.2f ms per frame at 3.8 MB (%llx)\n", round,
                            which == 0 ? "current" : "xxh3", perPage, kPage / perPage, perPage * (3.8e6 / kPage) / 1e6,
                            static_cast<unsigned long long>(sink & 0xF));
            }
        return 0;
    }
    if (std::string(argv[1]) == "--gl-info")
    {
        gles::HeadlessContext context;
        std::string error;
        if (!context.Create(error))
        {
            std::printf("[gl-info] no context: %s\n", error.c_str());
            return 1;
        }
        const auto str = [](GLenum name) { const GLubyte *v = glGetString(name); return v ? reinterpret_cast<const char *>(v) : "(null)"; };
        std::printf("[gl-info] context: %s\n", context.description.c_str());
        std::printf("[gl-info] GL_VENDOR: %s\n[gl-info] GL_RENDERER: %s\n[gl-info] GL_VERSION: %s\n[gl-info] GLSL: %s\n",
                    str(GL_VENDOR), str(GL_RENDERER), str(GL_VERSION), str(GL_SHADING_LANGUAGE_VERSION));
        EGLDisplay display = static_cast<EGLDisplay>(context.display);
        const char *eglVendor = eglQueryString(display, EGL_VENDOR);
        const char *eglVersion = eglQueryString(display, EGL_VERSION);
        const char *eglExt = eglQueryString(display, EGL_EXTENSIONS);
        std::printf("[gl-info] EGL_VENDOR: %s\n[gl-info] EGL_VERSION: %s\n[gl-info] EGL_EXTENSIONS: %s\n",
                    eglVendor ? eglVendor : "(null)", eglVersion ? eglVersion : "(null)", eglExt ? eglExt : "(null)");
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        std::printf("[gl-info] GL_EXTENSIONS (%d):", count);
        for (GLint i = 0; i < count; ++i)
            std::printf(" %s", reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i))));
        std::printf("\n");
        // The ones the renderer's next decisions hang on (docs: technique 8 persistent vertex buffers, GPU timing,
        // framebuffer fetch for blending, debug output).
        const char *wanted[] = {"GL_EXT_buffer_storage", "GL_EXT_disjoint_timer_query", "GL_EXT_shader_framebuffer_fetch",
                                "GL_ARM_shader_framebuffer_fetch", "GL_EXT_color_buffer_float", "GL_KHR_debug",
                                "GL_EXT_texture_border_clamp", "GL_OES_texture_buffer"};
        std::printf("[gl-info] key extensions:");
        for (const char *name : wanted)
        {
            bool found = false;
            for (GLint i = 0; i < count && !found; ++i)
                found = std::strcmp(reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i))), name) == 0;
            std::printf(" %s=%s", name + 3, found ? "yes" : "no");
        }
        std::printf("\n");
        GLint maxTexture = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
        std::printf("[gl-info] GL_MAX_TEXTURE_SIZE: %d\n", maxTexture);
        context.Destroy();
        return 0;
    }
    std::string path = argv[1], dumpDir;
    int dumpEvery = 10, repeat = 1;
    uint32_t maxFrames = ~0u;
    bool cpuReference = false; // --cpu: the CPU backend draws into VRAM instead (the picture to compare against)
    uint32_t dumpFbp = 0x70, dumpW = 512, dumpH = 448;
    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--dump" && i + 1 < argc) dumpDir = argv[++i];
        else if (a == "--dump-every" && i + 1 < argc) dumpEvery = std::max(1, std::atoi(argv[++i]));
        else if (a == "--fbp" && i + 1 < argc) dumpFbp = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 16));
        else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%ux%u", &dumpW, &dumpH);
        else if (a == "--repeat" && i + 1 < argc) repeat = std::max(1, std::atoi(argv[++i]));
        else if (a == "--cpu") cpuReference = true;
        else if (a == "--frames" && i + 1 < argc) maxFrames = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
        else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 2; }
    }

    gles::HeadlessContext context;
    std::string error;
    if (!context.Create(error))
    {
        std::fprintf(stderr, "[gles] %s\n", error.c_str());
        return 1;
    }
    std::fprintf(stderr, "[gles] %s\n", context.description.c_str());

    std::vector<double> cpuMs, gpuMs, totalMs;
    std::vector<double> draws, tris, decodes, switches, lookups, hashedKb, applies, glCalls;
    for (int pass = 0; pass < repeat; ++pass)
    {
        GSRecording::Reader reader;
        if (!reader.Open(path))
        {
            std::fprintf(stderr, "[gles] %s\n", reader.Error().c_str());
            return 1;
        }
        gles::GSGlesBackend glesBackend; // a fresh backend per pass: pass 1 pays every texture decode, later passes too
        GSCpuBackend cpuBackend;
        GSRasterBackend &backend = cpuReference ? static_cast<GSRasterBackend &>(cpuBackend) : glesBackend;
        if (!cpuReference && !glesBackend.InitGL(error))
        {
            std::fprintf(stderr, "[gles] %s\n", error.c_str());
            return 1;
        }
        std::vector<uint8_t> vram;
        GSRecording::Record record;
        Clock::duration busy{};
        uint32_t frame = 0;
        bool initialized = false;
        while (reader.Next(record))
        {
            if (record.op == GSRecording::Op::State)
            {
                GSRecording::Checkpoint initial;
                if (!GSRecording::ParseCheckpoint(record, initial))
                {
                    std::fprintf(stderr, "[gles] bad initial state\n");
                    return 1;
                }
                vram = std::move(initial.vram);
                backend.Initialize(vram.data(), static_cast<uint32_t>(vram.size()));
                if (cpuReference)
                    cpuBackend.ImportState(initial.state);
                else
                    glesBackend.ImportState(initial.state); // CLUT loaded before the recording started, CBP0/1, transfers
                initialized = true;
                continue;
            }
            if (!initialized)
                continue;
            if (record.op == GSRecording::Op::Field || record.op == GSRecording::Op::End)
            {
                const auto t0 = Clock::now();
                const gles::FrameStats s = cpuReference ? gles::FrameStats{} : glesBackend.TakeFrameStats();
                const auto t1 = Clock::now();
                glFinish();
                const auto t2 = Clock::now();
                busy += t1 - t0;
                const double cpu = ms(busy), gpu = ms(t2 - t1);
                std::fprintf(stderr,
                             "[frame] pass=%d n=%u cpu=%.2fms finish=%.2fms prims=%u tris=%u skipped=%u draws=%u "
                             "targets=%u decodes=%u (%llu KB) reuses=%u uploads=%u clears=%u cpuDraws=%u readbacks=%u targetUploads=%u depthLost=%u "
                             "lookups=%u hashedKB=%llu applies=%u glStateCalls=%u\n",
                             pass + 1, frame, cpu, gpu, s.primitives, s.triangles, s.skipped, s.drawCalls,
                             s.targetSwitches, s.textureDecodes, static_cast<unsigned long long>(s.textureBytes / 1024u),
                             s.textureReuses, s.uploads, s.clears, s.cpuDraws, s.readbacks, s.targetUploads, s.depthLost,
                             s.textureLookups, static_cast<unsigned long long>(s.hashedBytes / 1024u), s.keyApplies, s.glStateCalls);
                if (frame >= 2u) // the first frames decode every texture once
                {
                    cpuMs.push_back(cpu);
                    gpuMs.push_back(gpu);
                    totalMs.push_back(cpu + gpu);
                    draws.push_back(s.drawCalls);
                    tris.push_back(s.triangles);
                    decodes.push_back(s.textureDecodes);
                    switches.push_back(s.targetSwitches);
                    lookups.push_back(s.textureLookups);
                    hashedKb.push_back(static_cast<double>(s.hashedBytes) / 1024.0);
                    applies.push_back(s.keyApplies);
                    glCalls.push_back(s.glStateCalls);
                }
                if (!dumpDir.empty() && pass == 0 && frame % static_cast<uint32_t>(dumpEvery) == 0u)
                {
                    std::vector<uint8_t> rgba;
                    bool have = false;
                    if (cpuReference)
                    {
                        rgba.resize(static_cast<size_t>(dumpW) * dumpH * 4u);
                        for (uint32_t y = 0; y < dumpH; ++y)
                            for (uint32_t x = 0; x < dumpW; ++x)
                            {
                                const uint32_t v = cpuBackend.ReadVram(0u /* CT32 */, dumpFbp * 32u, dumpW / 64u, x, y);
                                std::memcpy(&rgba[(static_cast<size_t>(y) * dumpW + x) * 4u], &v, 4);
                            }
                        have = true;
                    }
                    else
                        have = glesBackend.ReadTarget(dumpFbp, dumpW, dumpH, rgba);
                    if (have)
                        writePpm(dumpDir + "/frame-" + std::to_string(frame) + ".ppm", rgba, dumpW, dumpH);
                }
                busy = {};
                ++frame;
                if (record.op == GSRecording::Op::End || frame >= maxFrames)
                    break;
                continue;
            }
            if (record.op == GSRecording::Op::Hash || record.op == GSRecording::Op::Mxcsr)
                continue;
            const auto t0 = Clock::now();
            const GSRecording::ApplyResult r = GSRecording::Apply(backend, record);
            busy += Clock::now() - t0;
            if (!r.ok)
            {
                std::fprintf(stderr, "[gles] record failed: %s\n", r.message.c_str());
                return 1;
            }
        }
        if (!reader.Error().empty())
            std::fprintf(stderr, "[gles] reader: %s\n", reader.Error().c_str());
    }
    if (totalMs.empty())
    {
        std::fprintf(stderr, "[gles] no frames\n");
        return 1;
    }
    std::fprintf(stderr,
                 "[gles-summary] frames=%zu | cpu median=%.2fms p95=%.2fms | gpu finish median=%.2fms p95=%.2fms | "
                 "total median=%.2fms p95=%.2fms (30 FPS needs < 33.3) | draws median=%.0f | tris median=%.0f | "
                 "decodes median=%.0f | target switches median=%.0f\n",
                 totalMs.size(), percentile(cpuMs, 0.5), percentile(cpuMs, 0.95), percentile(gpuMs, 0.5),
                 percentile(gpuMs, 0.95), percentile(totalMs, 0.5), percentile(totalMs, 0.95), percentile(draws, 0.5),
                 percentile(tris, 0.5), percentile(decodes, 0.5), percentile(switches, 0.5));
    // The backend's CPU-side work, exact per frame (any GPU, any machine): what the optimisations are measured by.
    std::fprintf(stderr,
                 "[gles-cpu-work] per frame, median: texture lookups=%.0f | VRAM hashed=%.0f KB | state applies=%.0f | "
                 "GL state calls=%.0f\n",
                 percentile(lookups, 0.5), percentile(hashedKb, 0.5), percentile(applies, 0.5), percentile(glCalls, 0.5));
    context.Destroy();
    return 0;
}
