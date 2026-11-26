#pragma once
// RK3326 port: a GS backend on OpenGL ES 3, written for low-end GPUs (Mali-G31) and in-order CPUs (Cortex-A35).
//
// What it does, and what it leaves out on purpose (first version, for measuring cost and checking the picture):
// - Each GS primitive becomes triangles in a CPU-side array; consecutive primitives with the same GL state go out as
//   one draw call (God Hand: ~14,100 triangles in ~520 runs per frame in a stage 1 fight).
// - One GL framebuffer per GS frame buffer (FBP); one depth buffer per ZBP. A texture whose TBP is a frame buffer
//   drawn on the GPU samples that framebuffer's colour texture.
// - Textures are decoded from the GS VRAM image into RGBA8 and cached. When their VRAM pages were written since, the
//   bytes there are hashed and compared: God Hand streams its textures through shared VRAM every frame, mostly
//   writing back the same bytes, so a hash replaces a decode and a GL upload.
// - GS blending (A-B)*C+D is mapped onto the GL blend modes for the common cases; the rest falls back to plain alpha.
// - Not handled yet: lines and points (counted), destination alpha test (DATE), FBA, partial frame buffer masks,
//   texture regions inside a frame buffer at an offset, mip maps, readback of GPU-drawn pixels into VRAM.
// - T8H textures on a frame buffer (PS2 colour tricks: the alpha byte through the CLUT) sample the framebuffer's
//   alpha and look it up in a 256x1 CLUT texture.
#include "runtime/gs/gs_backend.h"
#include "runtime/gs/gs_backend_state.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace gles
{
    struct FrameStats
    {
        uint32_t primitives = 0;
        uint32_t triangles = 0;
        uint32_t skipped = 0;        // lines, points: not drawn yet
        uint32_t drawCalls = 0;
        uint32_t targetSwitches = 0;
        uint32_t textureDecodes = 0; // cache misses: decoded from VRAM and sent to GL
        uint64_t textureBytes = 0;   // bytes sent to GL by those decodes
        uint32_t textureReuses = 0;  // pages written since last use, but the bytes were the same: no decode
        uint32_t uploads = 0;
        uint32_t clears = 0;
        uint32_t cpuDraws = 0;       // primitives the GPU cannot draw (reinterpreted formats, partial masks): GS CPU rasteriser
        uint32_t readbacks = 0;      // GPU images copied into the VRAM image for those draws (colour and depth)
        uint32_t targetUploads = 0;  // VRAM images copied to the GPU after CPU draws or game uploads
        uint32_t depthLost = 0;      // CPU draws that wrote depth: the GPU depth buffer does not see it yet
        // CPU-side work of the backend, counted exactly (the same on every run and every GPU): what the R36S pays in
        // CPU time scales with these, whatever a PC's GPU driver does.
        uint32_t textureLookups = 0; // TextureFor calls (one per textured primitive before caching)
        uint64_t hashedBytes = 0;    // VRAM bytes hashed to check whether a texture changed
        uint32_t keyApplies = 0;     // ApplyKey calls (GL state set for a new batch)
        uint32_t glStateCalls = 0;   // GL state calls those made (uniforms, blend, depth, texture parameters...)
    };

    class GSGlesBackend final : public GSRasterBackend, public GSBackendStateAccess
    {
    public:
        GSGlesBackend();
        ~GSGlesBackend() override;

        // Needs a current GLES 3 context.
        bool InitGL(std::string &error);

        void Initialize(uint8_t *vram, uint32_t vramSize) override;
        void Reset() override;
        void Submit(const GSPrimitiveBatch &batch) override;
        void LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut) override;
        void BeginTransfer(const GSTransferCommand &command) override;
        void UploadImage(const uint8_t *data, uint32_t sizeBytes) override;
        void Flush() override;
        void TextureFlush() override;
        void Sync(GSSyncReason reason) override;
        PresentationFrame Present(const GSPresentationRequest &request) override;
        bool ClearFramebuffer(const GSContext &context, uint32_t rgba) override;
        uint32_t ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) override;
        uint32_t ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const override;
        void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value) override;
        void SnapshotVram(std::vector<uint8_t> &out) const override;
        GSTransferSnapshot GetTransferSnapshot() const override;
        bool ExportState(GSBackendState &out) override;
        bool ImportState(const GSBackendState &state) override; // CLUT, CBP0/1, transfer state of a recording

        // Ends the frame: sends the pending batch. Stats since the last call.
        FrameStats TakeFrameStats();
        // RGBA8 rows of a frame buffer drawn on the GPU, top row first; false when that FBP was never drawn.
        bool ReadTarget(uint32_t fbp, uint32_t width, uint32_t height, std::vector<uint8_t> &rgba);

    private:
        struct Vertex
        {
            float x, y, z;      // pixels in the target, depth 0..1
            uint32_t rgba;      // GS colour, 0x80 = 1.0
            float s, t, q;      // normalised texture coordinates (s/q, t/q per fragment)
            float fog;          // 0..1
        };

        // A GS frame buffer. Its picture lives on the GPU (drawnOnGpu), in the VRAM image (vramValid), or both.
        struct Target
        {
            uint32_t fbp = 0, fbw = 0, width = 0, height = 0;
            uint32_t rows = 0;                 // GS rows in use (highest scissor seen)
            uint32_t colorTexture = 0;
            bool drawnOnGpu = false;           // the GPU texture holds the current picture
            bool vramValid = true;             // the VRAM image holds the current picture
        };

        struct Depth
        {
            uint32_t zbp = 0, fbw = 0, rows = 0, psm = 0;
            uint32_t texture = 0;              // DEPTH_COMPONENT24, sampled to copy it out
            bool gpuNewer = false;             // written on the GPU since the last copy into VRAM
        };

        struct Texture
        {
            uint32_t id = 0, width = 0, height = 0;
            uint32_t generation = 0;
            uint64_t clutHash = 0;
            uint64_t contentHash = 0;   // the VRAM pages it was decoded from
        };

        struct DrawKey
        {
            uint32_t fbp = 0, fbw = 0, zbp = 0, texture = 0;
            uint16_t scissor[4] = {};
            uint8_t zte = 0, ztst = 0, zmsk = 0, zpsm = 0;
            uint8_t abe = 0, blend[4] = {}, fix = 0, colorMask = 0xF;
            uint8_t tme = 0, tfx = 0, tcc = 0, fge = 0, ate = 0, atst = 0, aref = 0, linear = 0, wrapS = 0, wrapT = 0;
            uint8_t fogR = 0, fogG = 0, fogB = 0;
            uint16_t texW = 1, texH = 1;        // GS texture size, to scale s,t when the texture is a target
            uint8_t pal = 0;                    // T8H on a target: alpha through the CLUT
            uint32_t clutTag = 0;               // with pal: which CLUT
            bool operator==(const DrawKey &o) const;
        };

        Target &TargetFor(uint32_t fbp, uint32_t fbw);
        Depth &DepthFor(uint32_t zbp);
        bool NeedsCpu(const GSDrawState &state) const;
        void SubmitOnCpu(const GSPrimitiveBatch &batch);
        void SyncRangeToVram(uint32_t firstPage, uint32_t pageCount);
        void ReadbackTarget(Target &target);
        void ReadbackDepth(Depth &depth);
        void UploadTarget(Target &target);
        void BumpPages(uint32_t firstPage, uint32_t pageCount);
        // 8 KB pages under a buffer of width fbw*64 and the given rows: 64x32 pixels in 32-bit formats, 64x64 in 16-bit ones.
        static uint32_t PagesOf(uint32_t fbw, uint32_t rows, uint32_t psm = 0u)
        {
            const uint32_t pageRows = (psm & 0x2u) ? 64u : 32u;
            return std::max(1u, fbw) * ((std::max(1u, rows) + pageRows - 1u) / pageRows);
        }
        uint32_t FramebufferFor(Target &target, uint32_t depth);
        uint32_t TextureFor(const GSDrawState &state);
        void Decode(const GSTex0Reg &tex0, const GSTexaReg &texa, uint32_t w, uint32_t h, std::vector<uint32_t> &out) const;
        void PageRange(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t w, uint32_t h, uint32_t &first, uint32_t &count) const;
        uint32_t PageGeneration(uint32_t first, uint32_t count) const;
        void Touch(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
        void ApplyKey(const DrawKey &key);
        void FlushBatch();
        void RefreshClut();

        GSCpuBackend m_cpu; // keeps the VRAM image, transfers, CLUT and readbacks exactly as the CPU backend does
        uint8_t *m_vram = nullptr;

        uint32_t m_program = 0, m_vbo = 0, m_vao = 0;
        uint32_t m_depthProgram = 0, m_emptyVao = 0, m_scratchTexture = 0, m_scratchFramebuffer = 0;
        int m_uDepthScale = -1, m_uDepthMax = -1, m_uTexAlpha = -1;
        int m_uTarget = -1, m_uTfx = -1, m_uTcc = -1, m_uTme = -1, m_uFge = -1, m_uFog = -1, m_uAte = -1,
            m_uAtst = -1, m_uAref = -1, m_uTexScale = -1, m_uTex = -1, m_uPal = -1;
        uint32_t m_clutTexture = 0, m_clutTextureTag = 0;  // 256x1 RGBA CLUT for T8H-on-target draws
        bool m_texturePal = false;                         // set by TextureFor

        std::unordered_map<uint32_t, Target> m_targets;               // by FBP
        std::unordered_map<uint32_t, Depth> m_depths;                 // by ZBP
        std::unordered_map<uint64_t, uint32_t> m_framebuffers;        // by (colour texture, depth)
        std::unordered_map<uint64_t, Texture> m_textures;             // by TEX0 fields
        std::array<uint32_t, 512> m_pageGen{};                        // 8 KB GS pages
        // Hash of each page's bytes, and the page generation it was taken at: a page written since is hashed again,
        // once, however many textures sit on it (the game rewrites the same texture pages every frame).
        std::array<uint64_t, 512> m_pageHash{};
        std::array<uint32_t, 512> m_pageHashGen;                      // ~0u = never hashed (set in the constructor)
        uint64_t PagesHash(uint32_t first, uint32_t count);
        uint32_t m_genCounter = 1;
        std::array<uint16_t, 512> m_clut16{};                         // the CPU backend's CLUT buffer
        uint64_t m_clutHash = 0;

        GSTransferCommand m_transfer{};

        std::vector<Vertex> m_vertices;
        DrawKey m_key{}, m_applied{};
        bool m_haveKey = false, m_haveApplied = false;
        uint32_t m_boundFramebuffer = ~0u;
        FrameStats m_stats{};
        uint32_t m_frameIndex = 0;
    };
}
