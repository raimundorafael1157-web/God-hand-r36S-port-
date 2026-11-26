#include "gs_gles_backend.h"

#include "runtime/gs/gs_swizzle.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace gles
{
    namespace
    {
        constexpr uint32_t kTargetWidth = 1024u;  // every target: pixels map 1:1, the GS draws inside 1024x512
        constexpr uint32_t kTargetHeight = 512u;

        // GLSL ES 3.00. Colours and alpha are in GS units where 0x80 is 1.0 (hence the 255/128 factors).
        const char *kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aStq;
layout(location = 3) in float aFog;
uniform vec2 uTarget;
out vec4 vColor;
out vec3 vStq;
out float vFog;
void main()
{
    gl_Position = vec4(aPos.x / uTarget.x * 2.0 - 1.0, aPos.y / uTarget.y * 2.0 - 1.0, aPos.z * 2.0 - 1.0, 1.0);
    vColor = aColor;
    vStq = aStq;
    vFog = aFog;
}
)";

        // Colours are stored as bytes (x/255); alpha as GS alpha with 0x80 = 1.0, so GL blending with SRC/DST alpha
        // matches the GS. A texture decoded from VRAM keeps raw alpha (a/255): uTexAlpha rescales it.
        const char *kFragmentShader = R"(#version 300 es
precision highp float;
in vec4 vColor;
in vec3 vStq;
in float vFog;
uniform sampler2D uTex;
uniform sampler2D uClut;
uniform int uPal;
uniform vec2 uTexScale;
uniform float uTexAlpha;
uniform int uTme;
uniform int uTfx;
uniform int uTcc;
uniform int uFge;
uniform int uAte;
uniform int uAtst;
uniform float uAref;
uniform vec3 uFog;
out vec4 oColor;
void main()
{
    vec3 rgb = vColor.rgb;
    float a = vColor.a * (255.0 / 128.0);
    if (uTme == 1)
    {
        vec4 t;
        if (uPal == 1)
        {
            // T8H on a frame buffer: the pixel's alpha byte indexes the CLUT.
            float ta = texture(uTex, vStq.xy / vStq.z * uTexScale).a;
            int idx = int(clamp(floor(ta * 128.0 + 0.5), 0.0, 255.0));
            t = texelFetch(uClut, ivec2(idx, 0), 0);
            t.a *= 255.0 / 128.0;
        }
        else
        {
            t = texture(uTex, vStq.xy / vStq.z * uTexScale);
            t.a *= uTexAlpha;
        }
        if (uTfx == 0)      { rgb = rgb * t.rgb * (255.0 / 128.0); a = uTcc == 1 ? a * t.a : a; }
        else if (uTfx == 1) { rgb = t.rgb; a = uTcc == 1 ? t.a : a; }
        else if (uTfx == 2) { rgb = rgb * t.rgb * (255.0 / 128.0) + vColor.a; a = uTcc == 1 ? t.a + a : a; }
        else                { rgb = rgb * t.rgb * (255.0 / 128.0) + vColor.a; a = uTcc == 1 ? t.a : a; }
    }
    if (uAte == 1)
    {
        float ai = floor(a * 128.0 + 0.5);
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = ai < uAref;
        else if (uAtst == 3) pass = ai <= uAref;
        else if (uAtst == 4) pass = ai == uAref;
        else if (uAtst == 5) pass = ai >= uAref;
        else if (uAtst == 6) pass = ai > uAref;
        else if (uAtst == 7) pass = ai != uAref;
        if (!pass) discard;
    }
    if (uFge == 1)
        rgb = mix(uFog, rgb, vFog);
    oColor = clamp(vec4(rgb, a), 0.0, 1.0);
}
)";

        // Copies a depth texture out as bytes: the GS Z value, low byte in red.
        const char *kDepthVertexShader = R"(#version 300 es
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
        const char *kDepthFragmentShader = R"(#version 300 es
precision highp float;
precision highp int;
uniform highp sampler2D uDepth;
uniform float uScale;
uniform float uMax;
out vec4 oColor;
void main()
{
    float d = texelFetch(uDepth, ivec2(gl_FragCoord.xy), 0).r;
    uint z = uint(min(floor(d * uScale + 0.5), uMax));
    oColor = vec4(float(z & 255u), float((z >> 8) & 255u), float((z >> 16) & 255u), 0.0) / 255.0;
}
)";

        uint32_t compile(GLenum type, const char *source, std::string &error)
        {
            const GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);
            GLint ok = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = {};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                error = std::string(type == GL_VERTEX_SHADER ? "vertex" : "fragment") + " shader: " + log;
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        uint64_t mix64(uint64_t h, uint64_t v)
        {
            h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            return h;
        }

        uint64_t hashBytes(const uint8_t *data, size_t size, uint64_t seed)
        {
            uint64_t h = seed ^ (size * 0x100000001B3ull);
            size_t i = 0;
            for (; i + 8 <= size; i += 8)
            {
                uint64_t w;
                std::memcpy(&w, data + i, 8);
                h = (h ^ w) * 0x100000001B3ull;
                h ^= h >> 29;
            }
            for (; i < size; ++i)
                h = (h ^ data[i]) * 0x100000001B3ull;
            return h;
        }

        bool isPaletted(uint32_t psm) { return psm == GS_PSM_T8 || psm == GS_PSM_T4 || psm == 0x1B || psm == 0x24 || psm == 0x2C; }

        uint32_t ct16ToRgba(uint32_t v, const GSTexaReg &texa)
        {
            const uint32_t r = (v & 31u) << 3, g = ((v >> 5) & 31u) << 3, b = ((v >> 10) & 31u) << 3;
            const uint32_t a = (v & 0x8000u) ? texa.ta1 : ((texa.aem && (v & 0x7FFFu) == 0u) ? 0u : texa.ta0);
            return r | (g << 8) | (b << 16) | (a << 24);
        }

        GLenum glWrap(uint32_t mode) { return (mode == 0u || mode == 3u) ? GL_REPEAT : GL_CLAMP_TO_EDGE; }

        // Debug switches (GLES_DEBUG=letters): d no depth test, t no sampling from targets, b no blending,
        // s skip sprites, 3 skip triangles, x skip primitives that sample a frame buffer.
        bool debugFlag(char c)
        {
            static const std::string flags = [] { const char *v = std::getenv("GLES_DEBUG"); return std::string(v ? v : ""); }();
            return flags.find(c) != std::string::npos;
        }
    }

    bool GSGlesBackend::DrawKey::operator==(const DrawKey &o) const { return std::memcmp(this, &o, sizeof(DrawKey)) == 0; }

    GSGlesBackend::GSGlesBackend()
    {
        m_vertices.reserve(65536);
        m_pageHashGen.fill(~0u);
        std::memset(&m_key, 0, sizeof(m_key)); // keys are compared byte for byte: padding must be zero too
        std::memset(&m_applied, 0, sizeof(m_applied));
    }

    GSGlesBackend::~GSGlesBackend()
    {
        for (auto &[fbp, t] : m_targets)
            glDeleteTextures(1, &t.colorTexture);
        for (auto &[key, t] : m_textures)
            glDeleteTextures(1, &t.id);
        for (auto &[zbp, d] : m_depths)
            glDeleteTextures(1, &d.texture);
        if (m_scratchFramebuffer)
            glDeleteFramebuffers(1, &m_scratchFramebuffer);
        if (m_scratchTexture)
            glDeleteTextures(1, &m_scratchTexture);
        if (m_emptyVao)
            glDeleteVertexArrays(1, &m_emptyVao);
        if (m_depthProgram)
            glDeleteProgram(m_depthProgram);
        for (auto &[key, fb] : m_framebuffers)
            glDeleteFramebuffers(1, &fb);
        if (m_vbo)
            glDeleteBuffers(1, &m_vbo);
        if (m_vao)
            glDeleteVertexArrays(1, &m_vao);
        if (m_program)
            glDeleteProgram(m_program);
        if (m_clutTexture)
            glDeleteTextures(1, &m_clutTexture);
    }

    namespace
    {
        uint32_t link(const char *vsSource, const char *fsSource, std::string &error)
        {
            const GLuint vs = compile(GL_VERTEX_SHADER, vsSource, error);
            if (!vs)
                return 0;
            const GLuint fs = compile(GL_FRAGMENT_SHADER, fsSource, error);
            if (!fs)
            {
                glDeleteShader(vs);
                return 0;
            }
            const GLuint program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);
            glDeleteShader(vs);
            glDeleteShader(fs);
            GLint ok = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                char log[1024] = {};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                error = std::string("link: ") + log;
                glDeleteProgram(program);
                return 0;
            }
            return program;
        }
    }

    bool GSGlesBackend::InitGL(std::string &error)
    {
        m_program = link(kVertexShader, kFragmentShader, error);
        if (!m_program)
            return false;
        m_depthProgram = link(kDepthVertexShader, kDepthFragmentShader, error);
        if (!m_depthProgram)
            return false;
        glUseProgram(m_depthProgram);
        glUniform1i(glGetUniformLocation(m_depthProgram, "uDepth"), 2);
        m_uDepthScale = glGetUniformLocation(m_depthProgram, "uScale");
        m_uDepthMax = glGetUniformLocation(m_depthProgram, "uMax");
        glGenVertexArrays(1, &m_emptyVao);
        glGenTextures(1, &m_scratchTexture);
        glBindTexture(GL_TEXTURE_2D, m_scratchTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTargetWidth, kTargetHeight);
        glGenFramebuffers(1, &m_scratchFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_scratchFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_scratchTexture, 0);
        glUseProgram(m_program);
        m_uTarget = glGetUniformLocation(m_program, "uTarget");
        m_uTex = glGetUniformLocation(m_program, "uTex");
        m_uTfx = glGetUniformLocation(m_program, "uTfx");
        m_uTcc = glGetUniformLocation(m_program, "uTcc");
        m_uTme = glGetUniformLocation(m_program, "uTme");
        m_uFge = glGetUniformLocation(m_program, "uFge");
        m_uFog = glGetUniformLocation(m_program, "uFog");
        m_uAte = glGetUniformLocation(m_program, "uAte");
        m_uAtst = glGetUniformLocation(m_program, "uAtst");
        m_uAref = glGetUniformLocation(m_program, "uAref");
        m_uTexScale = glGetUniformLocation(m_program, "uTexScale");
        m_uPal = glGetUniformLocation(m_program, "uPal");
        m_uTexAlpha = glGetUniformLocation(m_program, "uTexAlpha");
        glUniform1i(glGetUniformLocation(m_program, "uClut"), 1);
        glGenTextures(1, &m_clutTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_clutTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glActiveTexture(GL_TEXTURE0);
        glUniform2f(m_uTarget, static_cast<float>(kTargetWidth), static_cast<float>(kTargetHeight));
        glUniform1i(m_uTex, 0);

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glGenBuffers(1, &m_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, x)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, rgba)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, s)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, fog)));
        glEnable(GL_SCISSOR_TEST);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        return glGetError() == GL_NO_ERROR || (error = "GL error during setup", false);
    }

    void GSGlesBackend::Initialize(uint8_t *vram, uint32_t vramSize)
    {
        m_vram = vram;
        m_cpu.Initialize(vram, vramSize);
    }

    void GSGlesBackend::Reset() { m_cpu.Reset(); }

    // ---------------------------------------------------------------------------------------------------- VRAM pages
    void GSGlesBackend::PageRange(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t w, uint32_t h, uint32_t &first,
                                  uint32_t &count) const
    {
        const GSSwizzle::Format &f = GSSwizzle::GetFormat(psm);
        const uint32_t pageW = 1u << f.pageShiftX, pageH = 1u << f.pageShiftY;
        const uint32_t cols = std::max(1u, (std::max(bw, 1u) * 64u + pageW - 1u) / pageW);
        const uint32_t rows = std::max(1u, (h + pageH - 1u) / pageH);
        first = bp / 32u;
        count = std::max(cols, (w + pageW - 1u) / pageW) * rows + 1u;
        if (first >= m_pageGen.size())
            count = 0u;
        else
            count = std::min<uint32_t>(count, static_cast<uint32_t>(m_pageGen.size()) - first);
    }

    uint32_t GSGlesBackend::PageGeneration(uint32_t first, uint32_t count) const
    {
        uint32_t gen = 0;
        for (uint32_t p = first; p < first + count; ++p)
            gen = std::max(gen, m_pageGen[p]);
        return gen;
    }

    void GSGlesBackend::Touch(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
    {
        const GSSwizzle::Format &f = GSSwizzle::GetFormat(psm);
        if (f.kind == GSSwizzle::Kind::Invalid)
            return;
        const uint32_t pageW = 1u << f.pageShiftX, pageH = 1u << f.pageShiftY;
        const uint32_t cols = std::max(1u, (std::max(bw, 1u) * 64u + pageW - 1u) / pageW);
        const uint32_t rows = std::max(1u, (y + h + pageH - 1u) / pageH);
        const uint32_t first = bp / 32u, count = cols * rows + 1u;
        (void)x;
        BumpPages(first, count);
        for (auto &[fbp, target] : m_targets) // the game wrote pixels into a frame buffer: the VRAM image wins
            if (fbp < first + count && first < fbp + PagesOf(target.fbw, target.rows))
            {
                target.drawnOnGpu = false;
                target.vramValid = true;
            }
    }

    void GSGlesBackend::BumpPages(uint32_t first, uint32_t count)
    {
        ++m_genCounter;
        for (uint32_t p = first; p < first + count && p < m_pageGen.size(); ++p)
            m_pageGen[p] = m_genCounter;
    }

    // Before anything reads or writes the VRAM image there, copy out what only the GPU holds.
    void GSGlesBackend::SyncRangeToVram(uint32_t first, uint32_t count)
    {
        for (auto &[fbp, target] : m_targets)
            if (target.drawnOnGpu && !target.vramValid && fbp < first + count && first < fbp + PagesOf(target.fbw, target.rows))
                ReadbackTarget(target);
        for (auto &[zbp, depth] : m_depths)
            if (depth.gpuNewer && zbp < first + count && first < zbp + PagesOf(depth.fbw, depth.rows, depth.psm))
                ReadbackDepth(depth);
    }

    void GSGlesBackend::ReadbackTarget(Target &t)
    {
        FlushBatch();
        const uint32_t w = std::min(kTargetWidth, std::max(1u, t.fbw) * 64u), h = std::min(kTargetHeight, std::max(1u, t.rows));
        const uint32_t fb = FramebufferFor(t, 0u);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        m_boundFramebuffer = fb;
        m_haveApplied = false;
        static thread_local std::vector<uint32_t> px;
        px.resize(static_cast<size_t>(w) * h);
        glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                const uint32_t c = px[static_cast<size_t>(y) * w + x];
                const uint32_t a = ((c >> 24) * 128u + 127u) / 255u; // stored with 0x80 = 1.0
                m_cpu.WriteVram(GS_PSM_CT32, t.fbp * 32u, t.fbw, x, y, (c & 0xFFFFFFu) | (a << 24));
            }
        BumpPages(t.fbp, PagesOf(t.fbw, t.rows));
        t.vramValid = true;
        ++m_stats.readbacks;
    }

    void GSGlesBackend::ReadbackDepth(Depth &d)
    {
        FlushBatch();
        const uint32_t w = std::min(kTargetWidth, std::max(1u, d.fbw) * 64u), h = std::min(kTargetHeight, std::max(1u, d.rows));
        const bool z16 = (d.psm & 0xFu) >= 2u;
        const bool z32 = d.psm == 0x30u;
        glBindFramebuffer(GL_FRAMEBUFFER, m_scratchFramebuffer);
        m_boundFramebuffer = m_scratchFramebuffer;
        m_haveApplied = false;
        glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(m_depthProgram);
        glUniform1f(m_uDepthScale, z16 ? 65536.0f : 16777216.0f);
        glUniform1f(m_uDepthMax, z16 ? 65535.0f : 16777215.0f);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, d.texture);
        glBindVertexArray(m_emptyVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        static thread_local std::vector<uint32_t> px;
        px.resize(static_cast<size_t>(w) * h);
        glReadPixels(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glBindVertexArray(m_vao);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(m_program);
        glEnable(GL_SCISSOR_TEST);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                uint32_t z = px[static_cast<size_t>(y) * w + x] & 0xFFFFFFu;
                if (z32)
                    z <<= 8u; // only 24 bits kept on the GPU
                m_cpu.WriteVram(d.psm, d.zbp * 32u, d.fbw, x, y, z);
            }
        BumpPages(d.zbp, PagesOf(d.fbw, d.rows, d.psm));
        d.gpuNewer = false;
        ++m_stats.readbacks;
    }

    // The VRAM image is newer (CPU draws, game uploads, or never drawn here): send it to the target's texture.
    // No FlushBatch here: ApplyKey calls this while sending the pending batch.
    void GSGlesBackend::UploadTarget(Target &t)
    {
        const uint32_t w = std::min(kTargetWidth, std::max(1u, t.fbw) * 64u), h = std::min(kTargetHeight, std::max(1u, t.rows));
        static thread_local std::vector<uint32_t> px;
        px.resize(static_cast<size_t>(w) * h);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x)
            {
                const uint32_t c = m_cpu.ReadVram(GS_PSM_CT32, t.fbp * 32u, t.fbw, x, y);
                const uint32_t a = std::min(255u, ((c >> 24) * 255u + 64u) / 128u);
                px[static_cast<size_t>(y) * w + x] = (c & 0xFFFFFFu) | (a << 24);
            }
        glBindTexture(GL_TEXTURE_2D, t.colorTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        m_haveApplied = false;
        t.drawnOnGpu = true;
        ++m_stats.targetUploads;
    }

    // ---------------------------------------------------------------------------------------------------- transfers
    void GSGlesBackend::LoadClut(const GSTex0Reg &tex0, const GSTexClutReg &texclut)
    {
        // The CPU backend applies the CLD rules and the CSM1/CSM2 layouts; take its CLUT buffer as is, so the two
        // can never disagree.
        m_cpu.LoadClut(tex0, texclut);
        RefreshClut();
    }

    void GSGlesBackend::RefreshClut()
    {
        GSBackendState state;
        m_cpu.ExportState(state);
        m_clut16 = state.clut;
        m_clutHash = hashBytes(reinterpret_cast<const uint8_t *>(m_clut16.data()), sizeof(m_clut16), 0xC1u);
    }

    bool GSGlesBackend::ExportState(GSBackendState &out) { return m_cpu.ExportState(out); }

    bool GSGlesBackend::ImportState(const GSBackendState &state)
    {
        const bool ok = m_cpu.ImportState(state);
        RefreshClut();
        return ok;
    }

    void GSGlesBackend::BeginTransfer(const GSTransferCommand &command)
    {
        FlushBatch();
        const GSBitBltBuf &b = command.bitbltbuf;
        uint32_t first = 0, count = 0;
        PageRange(b.dbp, b.dbw, b.dpsm, command.trxpos.dsax + command.trxreg.rrw, command.trxpos.dsay + command.trxreg.rrh, first, count);
        if (count > 1u)
            SyncRangeToVram(first, count - 1u);
        PageRange(b.sbp, b.sbw, b.spsm, command.trxpos.ssax + command.trxreg.rrw, command.trxpos.ssay + command.trxreg.rrh, first, count);
        if (count > 1u)
            SyncRangeToVram(first, count - 1u); // local-to-host or local-to-local reads
        m_cpu.BeginTransfer(command);
        m_transfer = command;
    }

    void GSGlesBackend::UploadImage(const uint8_t *data, uint32_t sizeBytes)
    {
        m_cpu.UploadImage(data, sizeBytes);
        ++m_stats.uploads;
        const GSBitBltBuf &b = m_transfer.bitbltbuf;
        Touch(b.dbp, b.dbw, b.dpsm, m_transfer.trxpos.dsax, m_transfer.trxpos.dsay, m_transfer.trxreg.rrw,
              m_transfer.trxreg.rrh);
    }

    void GSGlesBackend::Flush() { FlushBatch(); }
    // The CPU backend models the GS texture cache, which its CLUT loads read through: it must see every TEXFLUSH.
    void GSGlesBackend::TextureFlush() { m_cpu.TextureFlush(); }
    void GSGlesBackend::Sync(GSSyncReason) { FlushBatch(); }
    PresentationFrame GSGlesBackend::Present(const GSPresentationRequest &) { FlushBatch(); return {}; }
    uint32_t GSGlesBackend::ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) { return m_cpu.ConsumeLocalToHostBytes(dst, maxBytes); }
    uint32_t GSGlesBackend::ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const { return m_cpu.ReadVram(psm, base, bw, x, y); }
    void GSGlesBackend::WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value)
    {
        m_cpu.WriteVram(psm, base, bw, x, y, value);
        Touch(base, bw, psm, x, y, 1, 1);
    }
    void GSGlesBackend::SnapshotVram(std::vector<uint8_t> &out) const { m_cpu.SnapshotVram(out); }
    GSTransferSnapshot GSGlesBackend::GetTransferSnapshot() const { return m_cpu.GetTransferSnapshot(); }

    // ---------------------------------------------------------------------------------------------------- targets
    GSGlesBackend::Target &GSGlesBackend::TargetFor(uint32_t fbp, uint32_t fbw)
    {
        Target &t = m_targets[fbp];
        if (!t.colorTexture)
        {
            t.fbp = fbp;
            t.fbw = fbw;
            t.width = kTargetWidth;
            t.height = kTargetHeight;
            glGenTextures(1, &t.colorTexture);
            glBindTexture(GL_TEXTURE_2D, t.colorTexture);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kTargetWidth, kTargetHeight);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            m_haveApplied = false;
        }
        else if (t.fbw != fbw)
        {
            // The same memory seen with another width: pixels move. Go through the VRAM image.
            if (t.drawnOnGpu && !t.vramValid)
                ReadbackTarget(t);
            t.fbw = fbw;
            t.drawnOnGpu = false;
        }
        return t;
    }

    GSGlesBackend::Depth &GSGlesBackend::DepthFor(uint32_t zbp)
    {
        Depth &d = m_depths[zbp];
        if (!d.texture)
        {
            d.zbp = zbp;
            glGenTextures(1, &d.texture);
            glBindTexture(GL_TEXTURE_2D, d.texture);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, kTargetWidth, kTargetHeight);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            m_haveApplied = false;
        }
        return d;
    }

    uint32_t GSGlesBackend::FramebufferFor(Target &target, uint32_t depthTexture)
    {
        const uint64_t key = (static_cast<uint64_t>(target.colorTexture) << 32) | depthTexture;
        uint32_t &fb = m_framebuffers[key];
        if (!fb)
        {
            glGenFramebuffers(1, &fb);
            glBindFramebuffer(GL_FRAMEBUFFER, fb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.colorTexture, 0);
            if (depthTexture)
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
            m_boundFramebuffer = fb;
        }
        return fb;
    }

    // ---------------------------------------------------------------------------------------------------- textures
    void GSGlesBackend::Decode(const GSTex0Reg &tex0, const GSTexaReg &texa, uint32_t w, uint32_t h,
                               std::vector<uint32_t> &out) const
    {
        out.resize(static_cast<size_t>(w) * h);
        const GSSwizzle::Format &f = GSSwizzle::GetFormat(tex0.psm);
        if (f.kind == GSSwizzle::Kind::Invalid || !m_vram)
        {
            std::fill(out.begin(), out.end(), 0xFFFF00FFu);
            return;
        }
        const uint32_t bw = tex0.tbw ? tex0.tbw : 1u;
        const bool paletted = isPaletted(tex0.psm);
        const bool fourBit = tex0.psm == GS_PSM_T4 || tex0.psm == 0x24 || tex0.psm == 0x2C;
        const bool clut16 = tex0.cpsm == GS_PSM_CT16 || tex0.cpsm == 0x0A;
        // CLUT buffer as the CPU backend keeps it: CT16 entries 0..511; CT32 entries 0..255, low half at i, high at
        // i + 256. CSA selects 16 entries for 4-bit textures (32 positions with CT16, 16 with CT32).
        const uint32_t clutBase = fourBit ? (static_cast<uint32_t>(tex0.csa) & (clut16 ? 31u : 15u)) << 4u : 0u;
        for (uint32_t y = 0; y < h; ++y)
        {
            uint32_t *row = out.data() + static_cast<size_t>(y) * w;
            for (uint32_t x = 0; x < w; ++x)
            {
                const GSSwizzle::Location loc = GSSwizzle::Locate(f, tex0.tbp0, bw, x, y);
                const uint32_t raw = GSSwizzle::LoadAt(f, m_vram + loc.byte, loc.shift);
                uint32_t c;
                if (paletted)
                {
                    if (clut16)
                        c = ct16ToRgba(m_clut16[(clutBase + raw) & 511u], texa);
                    else
                    {
                        const uint32_t i = (clutBase + raw) & 255u;
                        c = static_cast<uint32_t>(m_clut16[i]) | (static_cast<uint32_t>(m_clut16[i + 256u]) << 16);
                    }
                }
                else if (tex0.psm == GS_PSM_CT32)
                    c = raw;
                else if (tex0.psm == GS_PSM_CT24)
                    c = (raw & 0xFFFFFFu) | (((texa.aem && (raw & 0xFFFFFFu) == 0u) ? 0u : texa.ta0) << 24);
                else
                    c = ct16ToRgba(raw, texa);
                if (!tex0.tcc)
                    c |= 0xFF000000u; // alpha unused (TCC=0): keep it out of the picture
                row[x] = c;
            }
        }
    }

    // The bytes of pages first..first+count-1, as one number: each page's hash is kept with the page generation it was
    // taken at, so a page is read again only after something wrote to it.
    uint64_t GSGlesBackend::PagesHash(uint32_t first, uint32_t count)
    {
        if (!m_vram)
            return 0u;
        uint64_t h = mix64(0x7Eu, count);
        for (uint32_t p = first; p < first + count && p < m_pageGen.size(); ++p)
        {
            if (m_pageHashGen[p] != m_pageGen[p])
            {
                m_pageHash[p] = hashBytes(m_vram + static_cast<size_t>(p) * 8192u, 8192u, 0x7Eu);
                m_pageHashGen[p] = m_pageGen[p];
                m_stats.hashedBytes += 8192u;
            }
            h = mix64(h, m_pageHash[p]);
        }
        return h;
    }

    uint32_t GSGlesBackend::TextureFor(const GSDrawState &state)
    {
        ++m_stats.textureLookups;
        const GSTex0Reg &tex0 = state.context.tex0;
        const uint32_t w = std::min<uint32_t>(1024u, std::max<uint32_t>(1u, state.textureWidth));
        const uint32_t h = std::min<uint32_t>(1024u, std::max<uint32_t>(1u, state.textureHeight));
        // A texture sitting on a frame buffer drawn on the GPU: sample that framebuffer.
        if (tex0.tbp0 % 32u == 0u)
        {
            auto it = m_targets.find(tex0.tbp0 / 32u);
            if (it != m_targets.end() && it->second.drawnOnGpu && it->second.fbw == tex0.tbw && !debugFlag('t'))
            {
                if (!isPaletted(tex0.psm))
                    return it->second.colorTexture | 0x80000000u; // marker: a target, scaled in ApplyKey
                if (tex0.psm == 0x1Bu) // T8H: the target's alpha byte through the CLUT
                {
                    m_texturePal = true;
                    return it->second.colorTexture | 0x80000000u;
                }
            }
        }
        uint64_t key = mix64(tex0.tbp0, tex0.tbw);
        key = mix64(key, tex0.psm);
        key = mix64(key, (w << 16) | h);
        key = mix64(key, tex0.tcc);
        const bool paletted = isPaletted(tex0.psm);
        if (paletted)
            key = mix64(key, (static_cast<uint64_t>(tex0.cpsm) << 8) | tex0.csa);
        if (tex0.psm == GS_PSM_CT24 || tex0.psm == GS_PSM_CT16 || paletted)
            key = mix64(key, (static_cast<uint64_t>(state.texa.ta0) << 16) | (state.texa.ta1 << 8) | state.texa.aem);
        Texture &t = m_textures[key];
        uint32_t first = 0, count = 0;
        PageRange(tex0.tbp0, tex0.tbw, tex0.psm, w, h, first, count);
        if (count > 1u)
            SyncRangeToVram(first, count - 1u); // a frame buffer read some other way: through the VRAM image
        const uint32_t gen = PageGeneration(first, count);
        const bool clutSame = !paletted || t.clutHash == m_clutHash;
        if (t.id && t.generation >= gen && clutSame)
            return t.id;
        // Its pages were written since the last look. The game streams many textures through the same VRAM area
        // each frame and mostly writes back the same bytes: compare what is there now with what was decoded.
        const uint64_t content = PagesHash(first, count);
        if (t.id && clutSame && content == t.contentHash && t.width == w && t.height == h)
        {
            t.generation = m_genCounter;
            ++m_stats.textureReuses;
            return t.id;
        }
        // Re-decoding: the pending batch may still use the old picture of this texture.
        if (t.id && !m_vertices.empty())
            FlushBatch();
        static thread_local std::vector<uint32_t> pixels;
        Decode(tex0, state.texa, w, h, pixels);
        if (!t.id || t.width != w || t.height != h)
        {
            if (t.id)
                glDeleteTextures(1, &t.id);
            glGenTextures(1, &t.id);
            glBindTexture(GL_TEXTURE_2D, t.id);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
            t.width = w;
            t.height = h;
        }
        else
            glBindTexture(GL_TEXTURE_2D, t.id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        if (const char *dir = std::getenv("GLES_DUMP_TEXTURES")) // debug: every decode as a PPM
        {
            static uint32_t n = 0;
            char name[512];
            std::snprintf(name, sizeof(name), "%s/tex-%04u-tbp%x-psm%x-cpsm%x-csa%u-csm%u-tcc%u-%ux%u.ppm", dir, n++, tex0.tbp0, tex0.psm, tex0.cpsm, tex0.csa, tex0.csm, tex0.tcc, w, h);
            if (std::FILE *f = std::fopen(name, "wb"))
            {
                std::fprintf(f, "P6\n%u %u\n255\n", w, h);
                for (uint32_t p : pixels)
                {
                    const uint8_t rgb[3] = {static_cast<uint8_t>(p), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p >> 16)};
                    std::fwrite(rgb, 1, 3, f);
                }
                std::fclose(f);
            }
        }
        t.generation = m_genCounter;
        t.clutHash = m_clutHash;
        t.contentHash = content;
        ++m_stats.textureDecodes;
        m_stats.textureBytes += static_cast<uint64_t>(w) * h * 4u;
        m_haveApplied = false; // texture binding changed under the applied key
        return t.id;
    }

    // ---------------------------------------------------------------------------------------------------- drawing
    void GSGlesBackend::Submit(const GSPrimitiveBatch &batch)
    {
        ++m_stats.primitives;
        const GSDrawState &st = batch.state;
        const GSContext &cx = st.context;
        const GSPrimType type = st.prim.type;
        const bool sprite = type == GS_PRIM_SPRITE;
        const bool triangle = type == GS_PRIM_TRIANGLE || type == GS_PRIM_TRISTRIP || type == GS_PRIM_TRIFAN;
        if ((sprite && debugFlag('s')) || (triangle && debugFlag('3')) ||
            (!(sprite && batch.vertexCount >= 2) && !(triangle && batch.vertexCount >= 3)))
        {
            ++m_stats.skipped;
            return;
        }

        if (NeedsCpu(st))
        {
            SubmitOnCpu(batch);
            return;
        }
        {
            Target &t = TargetFor(cx.frame.fbp, cx.frame.fbw);
            t.rows = std::max<uint32_t>(t.rows, std::min<uint32_t>(kTargetHeight, cx.scissor.y1 + 1u));
        }

        DrawKey key;
        std::memset(&key, 0, sizeof(key)); // compared byte for byte (padding included)
        key.texW = key.texH = 1;
        key.fbp = cx.frame.fbp;
        key.fbw = cx.frame.fbw;
        const uint64_t test = cx.test;
        key.zte = static_cast<uint8_t>((test >> 16) & 1u);
        key.ztst = static_cast<uint8_t>((test >> 17) & 3u);
        key.zmsk = cx.zbuf.zmask ? 1u : 0u;
        key.zpsm = cx.zbuf.psm;
        const bool useDepth = key.zte && !(key.zmsk && key.ztst == 1u);
        key.zbp = useDepth ? cx.zbuf.zbp : 0u;
        if (key.zte)
        {
            Depth &d = DepthFor(cx.zbuf.zbp);
            d.fbw = cx.frame.fbw;
            d.psm = cx.zbuf.psm;
            d.rows = std::max<uint32_t>(d.rows, std::min<uint32_t>(kTargetHeight, cx.scissor.y1 + 1u));
        }
        key.scissor[0] = cx.scissor.x0;
        key.scissor[1] = cx.scissor.x1;
        key.scissor[2] = cx.scissor.y0;
        key.scissor[3] = cx.scissor.y1;
        key.abe = st.prim.abe ? 1u : 0u;
        key.blend[0] = cx.alpha & 3u;
        key.blend[1] = (cx.alpha >> 2) & 3u;
        key.blend[2] = (cx.alpha >> 4) & 3u;
        key.blend[3] = (cx.alpha >> 6) & 3u;
        key.fix = static_cast<uint8_t>((cx.alpha >> 32) & 0xFFu);
        const uint32_t fbmsk = cx.frame.fbmsk;
        key.colorMask = fbmsk == 0xFFFFFFFFu ? 0u : ((fbmsk & 0xFF000000u) == 0xFF000000u ? 0x7u : 0xFu);
        key.ate = static_cast<uint8_t>(test & 1u);
        key.atst = static_cast<uint8_t>((test >> 1) & 7u);
        key.aref = static_cast<uint8_t>((test >> 4) & 0xFFu);
        if (key.ate && ((test >> 12) & 3u) != 0u)
            key.ate = 0; // AFAIL keeps part of the pixel: not a plain discard; drawn without the test for now
        key.fge = st.prim.fge ? 1u : 0u;
        key.fogR = st.fogR;
        key.fogG = st.fogG;
        key.fogB = st.fogB;
        key.tme = st.prim.tme ? 1u : 0u;
        float tw = 1.0f, th = 1.0f;
        if (key.tme)
        {
            key.tfx = cx.tex0.tfx;
            key.tcc = cx.tex0.tcc;
            key.linear = st.linearFilter ? 1u : 0u;
            key.wrapS = cx.clamp & 3u;
            key.wrapT = (cx.clamp >> 2) & 3u;
            m_texturePal = false;
            key.texture = TextureFor(st);
            if (m_texturePal)
            {
                key.pal = 1;
                key.clutTag = static_cast<uint32_t>(m_clutHash) ^ (static_cast<uint32_t>(cx.tex0.cpsm) << 24);
            }
            if (debugFlag('x') && (key.texture & 0x80000000u))
            {
                ++m_stats.skipped; // debug: leave out passes that sample a frame buffer (post effects)
                return;
            }
            tw = static_cast<float>(std::max<uint32_t>(1u, st.textureWidth));
            th = static_cast<float>(std::max<uint32_t>(1u, st.textureHeight));
            key.texW = static_cast<uint16_t>(tw);
            key.texH = static_cast<uint16_t>(th);
        }
        if (!m_haveKey || !(key == m_key))
        {
            FlushBatch();
            m_key = key;
            m_haveKey = true;
            if (key.pal && key.clutTag != m_clutTextureTag) // the pending batch is out: now the CLUT texture may change
            {
                std::array<uint32_t, 256> rgba{};
                const bool clut16 = cx.tex0.cpsm == GS_PSM_CT16 || cx.tex0.cpsm == 0x0A;
                for (uint32_t i = 0; i < 256u; ++i)
                    rgba[i] = clut16 ? ct16ToRgba(m_clut16[i], st.texa)
                                     : static_cast<uint32_t>(m_clut16[i]) | (static_cast<uint32_t>(m_clut16[i + 256u]) << 16);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_clutTexture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                glActiveTexture(GL_TEXTURE0);
                m_clutTextureTag = key.clutTag;
                m_haveApplied = false;
            }
        }

        const float ofx = static_cast<float>(cx.xyoffset.ofx) / 16.0f;
        const float ofy = static_cast<float>(cx.xyoffset.ofy) / 16.0f;
        const double zScale = key.zpsm == 0x30u ? 4294967296.0 : (key.zpsm == 0x31u ? 16777216.0 : 65536.0);
        const bool flat = !st.prim.iip;
        const GSVertex &last = batch.vertices[batch.vertexCount - 1];
        auto make = [&](const GSVertex &v, const GSVertex &colorFrom) {
            Vertex o;
            o.x = v.x - ofx;
            o.y = v.y - ofy;
            o.z = static_cast<float>(std::min(1.0, v.z / zScale));
            o.rgba = static_cast<uint32_t>(colorFrom.r) | (static_cast<uint32_t>(colorFrom.g) << 8) |
                     (static_cast<uint32_t>(colorFrom.b) << 16) | (static_cast<uint32_t>(colorFrom.a) << 24);
            if (st.prim.fst)
            {
                o.s = (static_cast<float>(v.u) / 16.0f) / tw;
                o.t = (static_cast<float>(v.v) / 16.0f) / th;
                o.q = 1.0f;
            }
            else
            {
                o.s = v.s;
                o.t = v.t;
                o.q = v.q != 0.0f ? v.q : 1.0f;
            }
            o.fog = static_cast<float>(v.fog) / 255.0f;
            return o;
        };
        if (sprite)
        {
            const GSVertex &a = batch.vertices[0];
            const GSVertex &b = batch.vertices[1];
            Vertex p0 = make(a, b), p1 = make(b, b);
            if (!st.prim.fst) // per-corner s/q, t/q with q = 1
            {
                p0.s /= p0.q; p0.t /= p0.q; p0.q = 1.0f;
                p1.s /= p1.q; p1.t /= p1.q; p1.q = 1.0f;
            }
            p0.z = p1.z;
            Vertex c01 = p0, c10 = p0;
            c01.x = p1.x; c01.s = p1.s;          // top right
            c10.y = p1.y; c10.t = p1.t;          // bottom left
            Vertex c11 = p1;
            m_vertices.insert(m_vertices.end(), {p0, c01, c10, c01, c11, c10});
        }
        else
        {
            for (uint8_t i = 0; i < 3; ++i)
                m_vertices.push_back(make(batch.vertices[i], flat ? last : batch.vertices[i]));
        }
        ++m_stats.triangles;
        if (sprite)
            ++m_stats.triangles;
        if (m_vertices.size() >= 60000u)
            FlushBatch();
    }

    // Draws the GPU path cannot reproduce: frame buffers in other formats or with partial write masks, frame or
    // depth buffers read as textures in another format or width, destination alpha test.
    bool GSGlesBackend::NeedsCpu(const GSDrawState &st) const
    {
        if (debugFlag('c'))
            return false;
        const GSContext &cx = st.context;
        if (cx.frame.psm != GS_PSM_CT32 && cx.frame.psm != GS_PSM_CT24)
            return true;
        const uint32_t m = cx.frame.fbmsk;
        if (m != 0u && m != 0xFF000000u && m != 0x00FFFFFFu && m != 0xFFFFFFFFu)
            return true;
        if ((cx.test >> 14) & 1u)
            return true;
        if (!st.prim.tme)
            return false;
        const GSTex0Reg &tex0 = cx.tex0;
        const uint32_t page = tex0.tbp0 / 32u;
        for (const auto &[fbp, t] : m_targets)
            if ((t.drawnOnGpu && !t.vramValid) && page >= fbp && page < fbp + PagesOf(t.fbw, t.rows))
            {
                const bool plain = tex0.tbp0 == fbp * 32u && tex0.tbw == t.fbw &&
                                   (tex0.psm == GS_PSM_CT32 || tex0.psm == GS_PSM_CT24 || tex0.psm == 0x1Bu);
                return !plain;
            }
        for (const auto &[zbp, d] : m_depths)
            if (d.gpuNewer && page >= zbp && page < zbp + PagesOf(d.fbw, d.rows, d.psm))
                return true;
        return false;
    }

    void GSGlesBackend::SubmitOnCpu(const GSPrimitiveBatch &batch)
    {
        FlushBatch();
        const GSDrawState &st = batch.state;
        const GSContext &cx = st.context;
        const uint32_t rows = std::min<uint32_t>(kTargetHeight, cx.scissor.y1 + 1u);
        const uint32_t framePages = PagesOf(cx.frame.fbw, rows);
        SyncRangeToVram(cx.frame.fbp, framePages);
        const bool zte = ((cx.test >> 16) & 1u) != 0u;
        if (zte)
            SyncRangeToVram(cx.zbuf.zbp, PagesOf(cx.frame.fbw, rows, cx.zbuf.psm));
        if (st.prim.tme)
        {
            uint32_t first = 0, count = 0;
            PageRange(cx.tex0.tbp0, cx.tex0.tbw, cx.tex0.psm, std::max<uint32_t>(1u, st.textureWidth),
                      std::max<uint32_t>(1u, st.textureHeight), first, count);
            if (count > 1u)
                SyncRangeToVram(first, count - 1u);
        }
        m_cpu.Submit(batch);
        ++m_stats.cpuDraws;
        if (debugFlag('r')) // debug: why each CPU draw went there
            std::fprintf(stderr, "[cpu-draw] frame=%u fbp=%x fbw=%u psm=%x fbmsk=%08x test=%llx tme=%u tbp=%x tbw=%u tpsm=%x zbp=%x zmsk=%u\n",
                         m_frameIndex, cx.frame.fbp, cx.frame.fbw, cx.frame.psm, cx.frame.fbmsk,
                         static_cast<unsigned long long>(cx.test), st.prim.tme ? 1u : 0u, cx.tex0.tbp0, cx.tex0.tbw,
                         cx.tex0.psm, cx.zbuf.zbp, cx.zbuf.zmask ? 1u : 0u);
        if (cx.frame.fbmsk != 0xFFFFFFFFu)
        {
            BumpPages(cx.frame.fbp, framePages);
            for (auto &[fbp, t] : m_targets)
                if (fbp < cx.frame.fbp + framePages && cx.frame.fbp < fbp + PagesOf(t.fbw, t.rows))
                {
                    t.drawnOnGpu = false; // the VRAM image is the picture now
                    t.vramValid = true;
                }
            Target &t = TargetFor(cx.frame.fbp, cx.frame.fbw);
            t.rows = std::max(t.rows, rows);
            t.drawnOnGpu = false;
            t.vramValid = true;
        }
        if (zte && !cx.zbuf.zmask)
        {
            BumpPages(cx.zbuf.zbp, PagesOf(cx.frame.fbw, rows, cx.zbuf.psm));
            ++m_stats.depthLost;
        }
    }

    void GSGlesBackend::ApplyKey(const DrawKey &k)
    {
        ++m_stats.keyApplies;
        Target &target = TargetFor(k.fbp, k.fbw);
        if (!target.drawnOnGpu)
            UploadTarget(target);
        const uint32_t depth = k.zte ? DepthFor(k.zbp).texture : 0u;
        const uint32_t fb = FramebufferFor(target, (k.zte && (k.zbp || !k.zmsk)) ? depth : 0u);
        if (fb != m_boundFramebuffer)
        {
            ++m_stats.glStateCalls; glBindFramebuffer(GL_FRAMEBUFFER, fb);
            ++m_stats.glStateCalls; glViewport(0, 0, static_cast<GLsizei>(target.width), static_cast<GLsizei>(target.height));
            m_boundFramebuffer = fb;
            ++m_stats.targetSwitches;
        }
        ++m_stats.glStateCalls; glScissor(k.scissor[0], k.scissor[2], k.scissor[1] - k.scissor[0] + 1, k.scissor[3] - k.scissor[2] + 1);

        if (k.zte && !debugFlag('d'))
        {
            ++m_stats.glStateCalls; glEnable(GL_DEPTH_TEST);
            static const GLenum funcs[4] = {GL_NEVER, GL_ALWAYS, GL_GEQUAL, GL_GREATER};
            ++m_stats.glStateCalls; glDepthFunc(funcs[k.ztst & 3u]);
            ++m_stats.glStateCalls; glDepthMask(k.zmsk ? GL_FALSE : GL_TRUE);
        }
        else
        {
            ++m_stats.glStateCalls;
            glDisable(GL_DEPTH_TEST);
        }

        if (k.abe && !debugFlag('b'))
        {
            ++m_stats.glStateCalls; glEnable(GL_BLEND);
            const uint8_t A = k.blend[0], B = k.blend[1], C = k.blend[2], D = k.blend[3];
            const GLenum factor = C == 0u ? GL_SRC_ALPHA : (C == 1u ? GL_DST_ALPHA : GL_CONSTANT_ALPHA);
            const GLenum oneMinus = C == 0u ? GL_ONE_MINUS_SRC_ALPHA : (C == 1u ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_CONSTANT_ALPHA);
            if (C == 2u)
                ++m_stats.glStateCalls, glBlendColor(0.0f, 0.0f, 0.0f, std::min(1.0f, static_cast<float>(k.fix) / 128.0f));
            GLenum src = GL_SRC_ALPHA, dst = GL_ONE_MINUS_SRC_ALPHA, eq = GL_FUNC_ADD;
            if (A == B)
            {
                src = D == 0u ? GL_ONE : GL_ZERO;
                dst = D == 1u ? GL_ONE : GL_ZERO;
            }
            else if (A == 0u && B == 1u && D == 1u) { src = factor; dst = oneMinus; }      // (Cs-Cd)*C+Cd
            else if (A == 0u && B == 2u && D == 1u) { src = factor; dst = GL_ONE; }       // Cs*C+Cd
            else if (A == 0u && B == 2u && D == 2u) { src = factor; dst = GL_ZERO; }      // Cs*C
            else if (A == 2u && B == 0u && D == 1u) { src = factor; dst = GL_ONE; eq = GL_FUNC_REVERSE_SUBTRACT; } // Cd-Cs*C
            else if (A == 1u && B == 0u && D == 0u) { src = oneMinus; dst = factor; }     // (Cd-Cs)*C+Cs
            else if (A == 1u && B == 2u && D == 0u) { src = GL_ONE; dst = factor; }       // Cd*C+Cs
            ++m_stats.glStateCalls; glBlendEquation(eq);
            ++m_stats.glStateCalls; glBlendFunc(src, dst);
        }
        else
        {
            ++m_stats.glStateCalls;
            glDisable(GL_BLEND);
        }
        ++m_stats.glStateCalls; glColorMask((k.colorMask & 1u) != 0u, (k.colorMask & 2u) != 0u, (k.colorMask & 4u) != 0u, (k.colorMask & 8u) != 0u);

        ++m_stats.glStateCalls; glUniform1i(m_uTme, k.tme);
        ++m_stats.glStateCalls; glUniform1i(m_uPal, k.pal);
        ++m_stats.glStateCalls; glUniform1i(m_uTfx, k.tfx);
        ++m_stats.glStateCalls; glUniform1i(m_uTcc, k.tcc);
        ++m_stats.glStateCalls; glUniform1i(m_uFge, k.fge);
        ++m_stats.glStateCalls; glUniform3f(m_uFog, k.fogR / 255.0f, k.fogG / 255.0f, k.fogB / 255.0f);
        ++m_stats.glStateCalls; glUniform1i(m_uAte, k.ate);
        ++m_stats.glStateCalls; glUniform1i(m_uAtst, k.atst);
        ++m_stats.glStateCalls; glUniform1f(m_uAref, static_cast<float>(k.aref));
        if (k.tme)
        {
            const bool fromTarget = (k.texture & 0x80000000u) != 0u;
            const uint32_t id = k.texture & 0x7FFFFFFFu;
            ++m_stats.glStateCalls; glBindTexture(GL_TEXTURE_2D, id);
            const GLint filter = k.linear ? GL_LINEAR : GL_NEAREST;
            ++m_stats.glStateCalls; glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            ++m_stats.glStateCalls; glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            ++m_stats.glStateCalls; glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, fromTarget ? GL_CLAMP_TO_EDGE : glWrap(k.wrapS));
            ++m_stats.glStateCalls; glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, fromTarget ? GL_CLAMP_TO_EDGE : glWrap(k.wrapT));
            // s, t are normalised to the GS texture (tw x th); a target is 1024x512, so they shrink to its part.
            ++m_stats.glStateCalls;
            if (fromTarget)
                glUniform2f(m_uTexScale, static_cast<float>(k.texW) / kTargetWidth, static_cast<float>(k.texH) / kTargetHeight);
            else
                glUniform2f(m_uTexScale, 1.0f, 1.0f);
            ++m_stats.glStateCalls; glUniform1f(m_uTexAlpha, fromTarget ? 1.0f : 255.0f / 128.0f);
        }
    }

    void GSGlesBackend::FlushBatch()
    {
        if (m_vertices.empty() || !m_haveKey)
        {
            m_vertices.clear();
            return;
        }
        if (!m_haveApplied || !(m_applied == m_key))
        {
            ApplyKey(m_key);
            m_applied = m_key;
            m_haveApplied = true;
        }
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)), m_vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
        // Debug: GLES_TRACE_FRAME=<n> GLES_TRACE_DIR=<dir> logs every draw call of frame n and dumps FBP 0x70 after it.
        static const long traceFrame = std::getenv("GLES_TRACE_FRAME") ? std::atol(std::getenv("GLES_TRACE_FRAME")) : -1;
        if (traceFrame >= 0 && m_frameIndex == static_cast<uint32_t>(traceFrame))
        {
            static uint32_t n = 0;
            const char *dir = std::getenv("GLES_TRACE_DIR");
            const Vertex &v0 = m_vertices[0];
            std::fprintf(stderr, "[draw %u] fbp=%x zbp=%x verts=%zu tme=%u tex=%x pal=%u tfx=%u tcc=%u abe=%u blend=%u%u%u%u fix=%u ate=%u atst=%u zte=%u ztst=%u mask=%x wrap=%u%u tw=%u th=%u v0=(%.1f,%.1f z=%.4f st=%.3f,%.3f q=%.3f rgba=%08x)\n",
                         n, m_key.fbp, m_key.zbp, m_vertices.size(), m_key.tme, m_key.texture, m_key.pal, m_key.tfx, m_key.tcc,
                         m_key.abe, m_key.blend[0], m_key.blend[1], m_key.blend[2], m_key.blend[3], m_key.fix, m_key.ate,
                         m_key.atst, m_key.zte, m_key.ztst, m_key.colorMask, m_key.wrapS, m_key.wrapT, m_key.texW, m_key.texH,
                         v0.x, v0.y, v0.z, v0.s, v0.t, v0.q, v0.rgba);
            if (dir && m_key.fbp == 0x70u)
            {
                std::vector<uint8_t> px(512u * 448u * 4u);
                glReadPixels(0, 0, 512, 448, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                char name[512];
                std::snprintf(name, sizeof(name), "%s/draw-%04u.ppm", dir, n);
                if (std::FILE *f = std::fopen(name, "wb"))
                {
                    std::fprintf(f, "P6\n512 448\n255\n");
                    for (size_t i = 0; i < px.size(); i += 4)
                        std::fwrite(&px[i], 1, 3, f);
                    std::fclose(f);
                }
            }
            ++n;
        }
        ++m_stats.drawCalls;
        if (m_key.colorMask)
        {
            Target &t = m_targets[m_key.fbp];
            t.drawnOnGpu = true;
            t.vramValid = false;
        }
        if (m_key.zte && !m_key.zmsk)
            m_depths[m_key.zbp].gpuNewer = true;
        m_vertices.clear();
    }

    bool GSGlesBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
    {
        m_cpu.ClearFramebuffer(context, rgba);
        FlushBatch();
        Target &target = TargetFor(context.frame.fbp, context.frame.fbw);
        target.rows = std::max<uint32_t>(target.rows, std::min<uint32_t>(kTargetHeight, context.scissor.y1 + 1u));
        if (!target.drawnOnGpu)
            UploadTarget(target);
        const uint32_t fb = FramebufferFor(target, 0u);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        m_boundFramebuffer = fb;
        m_haveApplied = false;
        glViewport(0, 0, static_cast<GLsizei>(target.width), static_cast<GLsizei>(target.height));
        glScissor(context.scissor.x0, context.scissor.y0, context.scissor.x1 - context.scissor.x0 + 1,
                  context.scissor.y1 - context.scissor.y0 + 1);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor((rgba & 0xFFu) / 255.0f, ((rgba >> 8) & 0xFFu) / 255.0f, ((rgba >> 16) & 0xFFu) / 255.0f,
                     std::min(1.0f, ((rgba >> 24) & 0xFFu) / 128.0f));
        glClear(GL_COLOR_BUFFER_BIT);
        target.drawnOnGpu = true;
        target.vramValid = false;
        ++m_stats.clears;
        return true;
    }

    FrameStats GSGlesBackend::TakeFrameStats()
    {
        FlushBatch();
        ++m_frameIndex;
        FrameStats out = m_stats;
        m_stats = {};
        return out;
    }

    bool GSGlesBackend::ReadTarget(uint32_t fbp, uint32_t width, uint32_t height, std::vector<uint8_t> &rgba)
    {
        FlushBatch();
        auto it = m_targets.find(fbp);
        if (it == m_targets.end())
            return false;
        if (!it->second.drawnOnGpu)
            UploadTarget(it->second);
        width = std::min(width, it->second.width);
        height = std::min(height, it->second.height);
        const uint32_t fb = FramebufferFor(it->second, 0u);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        m_boundFramebuffer = fb;
        m_haveApplied = false;
        rgba.resize(static_cast<size_t>(width) * height * 4u);
        glReadPixels(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        return true;
    }
}
