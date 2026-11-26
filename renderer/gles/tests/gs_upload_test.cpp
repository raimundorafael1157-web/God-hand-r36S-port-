// Equivalence and speed test for GSCpuBackend::UploadImage (patch 0010): host-to-local transfers must leave exactly the
// same VRAM bytes as writing each pixel with WriteVram, the way the transfer rectangle orders them. It also checks the
// transfer position after partial uploads (data arriving in several pieces, cut at any byte).
//
// Built with the GLES renderer (renderer/gles/CMakeLists.txt): ninja -C build-host gs_upload_test, then run it.
// Exit code 0 = every case identical.
#include "runtime/gs/gs_cpu_backend.h"
#include "runtime/gs/gs_types.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace
{
    constexpr uint32_t kVramSize = 4u * 1024u * 1024u;

    uint32_t bitsPerPixel(uint8_t psm)
    {
        switch (psm)
        {
        case GS_PSM_CT32: case GS_PSM_Z32: return 32u;
        case GS_PSM_CT24: case GS_PSM_Z24: return 24u;
        case GS_PSM_CT16: case GS_PSM_CT16S: case GS_PSM_Z16: case GS_PSM_Z16S: return 16u;
        case GS_PSM_T8: case GS_PSM_T8H: return 8u;
        default: return 4u;
        }
    }

    // The reference: what the transfer means, pixel by pixel, through the public WriteVram.
    void referenceUpload(GSCpuBackend &backend, const GSTransferCommand &cmd, const std::vector<uint8_t> &data)
    {
        const uint8_t psm = cmd.bitbltbuf.dpsm;
        const uint32_t bpp = bitsPerPixel(psm);
        const uint32_t total = static_cast<uint32_t>(cmd.trxreg.rrw) * cmd.trxreg.rrh;
        for (uint32_t i = 0; i < total; ++i)
        {
            uint32_t value = 0;
            if (bpp == 4u)
            {
                if (i / 2u >= data.size())
                    break;
                value = (i & 1u) ? (data[i / 2u] >> 4u) & 0x0Fu : data[i / 2u] & 0x0Fu;
            }
            else
            {
                const uint32_t bytes = bpp / 8u, at = i * bytes;
                if (at + bytes > data.size())
                    break;
                for (uint32_t b = 0; b < bytes; ++b)
                    value |= static_cast<uint32_t>(data[at + b]) << (8u * b);
            }
            backend.WriteVram(psm, cmd.bitbltbuf.dbp, std::max<uint32_t>(cmd.bitbltbuf.dbw, 1u),
                              cmd.trxpos.dsax + i % cmd.trxreg.rrw, cmd.trxpos.dsay + i / cmd.trxreg.rrw, value);
        }
    }
}

int main()
{
    const uint8_t formats[] = {GS_PSM_CT32, GS_PSM_CT24, GS_PSM_CT16, GS_PSM_CT16S, GS_PSM_Z32, GS_PSM_Z24, GS_PSM_Z16,
                               GS_PSM_Z16S, GS_PSM_T8, GS_PSM_T8H, GS_PSM_T4, GS_PSM_T4HL, GS_PSM_T4HH};
    std::mt19937 rng(12345);
    std::vector<uint8_t> vramA(kVramSize), vramB(kVramSize);
    int failures = 0, cases = 0;
    for (uint8_t psm : formats)
    {
        for (int round = 0; round < 40; ++round)
        {
            for (size_t i = 0; i < kVramSize; i += 4)
            {
                const uint32_t r = rng();
                std::memcpy(&vramA[i], &r, 4);
            }
            vramB = vramA;
            GSCpuBackend fast, reference;
            fast.Initialize(vramA.data(), kVramSize);
            reference.Initialize(vramB.data(), kVramSize);

            GSTransferCommand cmd{};
            cmd.bitbltbuf.dbp = rng() % 0x3000u;
            cmd.bitbltbuf.dbw = static_cast<uint8_t>(1u + rng() % 10u);
            cmd.bitbltbuf.dpsm = psm;
            cmd.trxpos.dsax = static_cast<uint16_t>(rng() % 200u);
            cmd.trxpos.dsay = static_cast<uint16_t>(rng() % 200u);
            cmd.trxreg.rrw = static_cast<uint16_t>(1u + rng() % 130u);   // odd widths included: T4 pairs cross rows
            cmd.trxreg.rrh = static_cast<uint16_t>(1u + rng() % 70u);
            cmd.direction = 0u;
            const uint32_t total = static_cast<uint32_t>(cmd.trxreg.rrw) * cmd.trxreg.rrh;
            const uint32_t bytes = (total * bitsPerPixel(psm) + 7u) / 8u;
            std::vector<uint8_t> data(bytes);
            for (auto &b : data)
                b = static_cast<uint8_t>(rng());

            fast.BeginTransfer(cmd);
            // Several uploads, cut at arbitrary bytes (also inside a pixel: the transfer must then stop and resume).
            uint32_t sent = 0;
            while (sent < bytes)
            {
                uint32_t piece = 1u + rng() % std::max<uint32_t>(1u, bytes / 3u + 1u);
                piece = std::min(piece, bytes - sent);
                const uint32_t bpp = bitsPerPixel(psm);
                if (bpp >= 16u)
                    piece = std::max<uint32_t>(bpp / 8u, piece - piece % (bpp / 8u)); // whole pixels per piece
                piece = std::min(piece, bytes - sent);
                fast.UploadImage(data.data() + sent, piece);
                sent += piece;
            }
            referenceUpload(reference, cmd, data);
            ++cases;
            if (std::memcmp(vramA.data(), vramB.data(), kVramSize) != 0)
            {
                size_t first = 0;
                while (vramA[first] == vramB[first])
                    ++first;
                std::fprintf(stderr, "FAIL psm=0x%02x dbp=0x%x dbw=%u at %u,%u %ux%u: first different byte %zu\n", psm,
                             cmd.bitbltbuf.dbp, cmd.bitbltbuf.dbw, cmd.trxpos.dsax, cmd.trxpos.dsay, cmd.trxreg.rrw,
                             cmd.trxreg.rrh, first);
                ++failures;
            }
        }
    }
    std::printf("[gs_upload_test] %d cases, %d different\n", cases, failures);

    // Speed: a 256x256 CT32 and a 512x512 T8 upload, the sizes God Hand streams, through UploadImage.
    for (uint8_t psm : {GS_PSM_CT32, GS_PSM_T8, GS_PSM_T4})
    {
        GSCpuBackend backend;
        backend.Initialize(vramA.data(), kVramSize);
        GSTransferCommand cmd{};
        cmd.bitbltbuf.dbp = 0x2800;
        cmd.bitbltbuf.dbw = 4;
        cmd.bitbltbuf.dpsm = psm;
        cmd.trxreg.rrw = psm == GS_PSM_CT32 ? 256 : 512;
        cmd.trxreg.rrh = psm == GS_PSM_CT32 ? 256 : 512;
        cmd.direction = 0u;
        const uint32_t total = static_cast<uint32_t>(cmd.trxreg.rrw) * cmd.trxreg.rrh;
        std::vector<uint8_t> data((total * bitsPerPixel(psm) + 7u) / 8u, 0x5A);
        constexpr int kRepeats = 50;
        const auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < kRepeats; ++r)
        {
            backend.BeginTransfer(cmd);
            backend.UploadImage(data.data(), static_cast<uint32_t>(data.size()));
        }
        const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / (kRepeats * double(total));
        std::printf("[gs_upload_test] speed psm=0x%02x %ux%u: %.2f ns/pixel\n", psm, cmd.trxreg.rrw, cmd.trxreg.rrh, ns);
    }
    return failures == 0 ? 0 : 1;
}
