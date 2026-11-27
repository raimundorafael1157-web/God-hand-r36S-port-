# Heavy PS2-era games on RK3326 handhelds: measured performance and lessons for the God Hand R36S port

Research date: 2026-11-27. Method: web search plus direct reading of three source repositories (Celosia, NFSHP2-R36S, PortMaster-New), all shallow-cloned on 2026-11-27. Several community sites (heldgames.com, handhelds.wiki, generationamiga.com) could not be fetched from this session (network allow-list), so claims from them come only from search-result snippets and are marked as such.

Repository revisions read:
- Celosia (SotC R36S fork): `github.com/amarqs182/celosia`, `main` @ `39ab080`, 2026-11-27.
- NFSHP2-R36S: `github.com/manuelpllull/NFSHP2-R36S`, HEAD of 2026-11-23.
- PortMaster-New: `github.com/PortsMaster/PortMaster-New`, `main` @ `6fee745`, 2026-11-27 (1,522 ports).

## 1. Measured results for Shadow of the Colossus on RK3326

### Takeaway
No one reports SotC running playably through PS2 *emulation* on any RK3326 device. A native port does exist: Celosia, a PS2Recomp static recompilation of SotC (USA, SCUS-97472). Its R36S fork measures about 30 to 33 FPS median in heavy gameplay and a locked 60 FPS in menus and light scenes, on an ArkOS-family Debian 13 image with clocks maxed. This is the same architecture as our God Hand port (PS2Recomp, recompiled EE, GLES on Mali-G31), so it is the closest analog that exists.

### Cited Findings
**Emulation route (none found working)**
- No source in the searches reports an SotC emulation attempt on an RG351, OGA/OGS, RGB10 or R36S with numbers. The closest mobile data point is SotC under AetherSX2 on a Snapdragon 855 phone, "up to 60fps at times" (impression, not a benchmark). — [DroidGamers](https://www.droidgamers.com/news/shadow-of-the-colossus-emulator/)
- AetherSX2 video demos on phones included SotC (no FPS given). — [Inverse](https://www.inverse.com/input/gaming/this-android-ps2-emulator-can-run-devil-may-cry-on-your-phone)

**Celosia native port (static recompilation). Measured on device, data from the repo docs**
- What it is: "A native port of Shadow of the Colossus (PS2, USA, SCUS-97472) ... built by statically recompiling the game's MIPS code to C++ with PS2Recomp. No emulator runs the game's CPU code." The EE code from the boot ELF and three XFF modules is translated ahead of time. A native renderer reads the game's model data directly, and DMA, IOP and SPU2 are reimplemented as host code. — [Celosia README](https://github.com/amarqs182/celosia)
- The README status row for R36S (RK3326, aarch64): "builds and runs (`32.34 avg / 33.00 med FPS` on matched Phase A Shrine walk, `59.8–60.1 FPS` in menus/light scenes; GLES 3.2 via ARM libmali over SDL2 KMSDRM)". For comparison, Mac M4 Pro: 60 fps at 1536×1344. Vita in Vita3K intro: 18–25 fps. — [Celosia README](https://github.com/amarqs182/celosia)
- Device and firmware recorded by the port: RK3326 (`rockchip,rk3326-odroidgo3-linux`), 4× Cortex-A35 at 1008–1512 MHz (`fp asimd`, no FMA/fp16/SVE). Mali-G31 Bifrost r0p0 at 400–600 MHz via ARM libmali `OpenGL ES 3.2 v1.r13p0-01rel0`, `-gbm` variant. 895 MiB RAM (~708–726 MiB available) plus 511 MiB zram. 640×480 KMSDRM at 60 Hz. aarch64 Debian 13.7 (trixie), glibc 2.41, kernel 4.4.189. The SSH user is `ark`, so this is an ArkOS-family image. Whether it is dArkOS specifically is my inference, not stated. Platform measurements are dated 2026-11-15. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Overclock and clock settings in the launcher: performance governor, CPU `scaling_max_freq` 1512000, GPU and DMC devfreq set to performance with min=max (comment: "CPU (1.5 GHz), GPU (520 MHz) e DMC/DRAM (666 MHz)"). zram is enabled, swappiness 60, and the page cache is dropped at launch. — [Celosia ports/r36s/sotc.sh](https://github.com/amarqs182/celosia/blob/main/ports/r36s/sotc.sh)
- FPS progression measured on device (deterministic replay, gp 4458..5298, 900 frames, 838–1130 draws):

  | Stage | Median FPS |
  | --- | --- |
  | Pass 8A, Release, no PGO | 25.6 |
  | Pass 8B, + PGO | 27.0 |
  | Pass 9, Mali TBDR fixes | 27.3 |
  | Pass 9, + PGO | 29.4 |
  | Phase 6, + meshoptimizer | 30.1 |
  | Phase 6B | 31.6 |
  | Phase A, all watch/trace/log instrumentation compiled out, `-mtune=cortex-a35+crypto+crc`, new PGO | 33.0 (32.34 avg). Frame p50 30.3 ms, p90 34.2 ms, p99 40.8 ms |

  — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Earlier real-play logs: the Pass 6 gameplay median was 26.8 FPS (game thread 36.0 ms). Pass 7 (the `SOTC_EE_TRUST` "lean" function copies with trusted floats) raised it to 35.5 FPS median (+32.5%), with the game thread at 27.0 ms. — [Celosia SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/main/SOTC_OPTIMIZATION_LOG.md)
- Bottleneck: the game (EE) thread is "still the sole frame limiter on Cortex-A35 (`wait 0.0`)". The profile is flat: the top functions are collision callbacks at about 2% each, then `DispDispObjGroupModel`, `ExecCharaGeometry`, `GetMotionMatrix` and so on. The game thread is 99.5–99.9% busy in 30 fps windows. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md); [r36s_native_audit.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_native_audit.md)
- The game-thread profile breaks down as 66.4% hand-written native overrides (351 addresses in `game/overrides/`), 31.7% recompiled MIPS and 2% lifted code (26,375 samples). Most of the hot code had already been rewritten by hand. — [Celosia r36s_native_audit.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_native_audit.md)
- The audit says 60 FPS would need about 47% off the game thread (31.6 → ≤16.67 ms) and about 44% off the render thread (29.7 ms). Optimizing CPU alone would hit a render/GPU ceiling of about 35 FPS. — [Celosia r36s_native_audit.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_native_audit.md)
- An optional quality-for-speed menu ("sacrifices"): half particles, LOD chosen as if 1.6× farther, fewer camera collision checks, distant characters at half rate, effects at half rate, shorter draw distance, and a "Steady 20" frame-rate mode with a fixed 50 ms game step. — [Celosia README](https://github.com/amarqs182/celosia)
- Upstream Celosia's VU1 plan measured that under Vita3K the VU1 path ran about 450 programs/s at about 2 ms CPU each, about 0.6 fps. The cost was in the layers (VIF unpack, VU interpreter setup, XGKICK, GIF decode, GS per-triangle state), not the transform maths. The fix is to let the GPU vertex shader do VU1's job for known routines and emit one draw per VU batch. — [Celosia docs/vu1_gpu_path.md](https://github.com/amarqs182/celosia/blob/main/docs/vu1_gpu_path.md)

### Inferences
- A PS2Recomp-style SotC reaches about 30–33 FPS median on an R36S only after many optimization passes and roughly 350 hand-native function overrides. Plain recompiled MIPS plus a generic runtime started in the mid-20s or lower. God Hand is a lighter world than SotC (arena stages, no open world or streaming terrain), but it has more characters and effects on screen. A 30 FPS target with a 20 FPS fallback looks realistic. 60 FPS in combat looks unlikely without very heavy native rewriting.
- God Hand ran at 30 fps on the PS2 itself (to be confirmed by the project; I did not research it here). If so, a stable 30 with a fixed step is the right target, and Celosia's "Steady 20 / Cap 30" frame pacing modes are the relevant pattern.

### Gaps
- Celosia does not say which firmware image it runs on (ArkOS, dArkOS or another). It shows only the `ark` user, Debian 13 and kernel 4.4.189.
- I found no independent (third-party) video or Reddit measurements of Celosia on an R36S. All numbers are self-reported by the developers' logs.
- I found no data for SotC (native or emulated) on RG351P/V/MP, OGA/OGS, RGB10, R36H or R36 Max.

## 2. Fastest PS2 emulation on Cortex-A35/A53-class devices (RK3326 vs RK3566 vs H700)

### Takeaway
PS2 emulation is effectively not viable on RK3326, and it is marginal-to-unviable on RK3566 and H700. Credible playable PS2 emulation starts at Snapdragon 845/855/865-class hardware. The only PS2-era 3D games that run well on RK3326 run as native ports (recompilation or decompilation), not under emulation.

### Cited Findings
- A guide places RK3326 at "PS1 and some Dreamcast", RK3566 and H700 at "everything up to PS1 and some N64", and says H700-class chips "lack the GPU throughput and sustained CPU performance that PS2 and GameCube emulators require". These are affiliate-style guides, so they give impressions, not measurements. — [Evetech deep dive](https://evezone.evetech.co.za/deep-dives/what-each-handheld-soc-tier-can-emulate-psp-vs-ps2-vs-switch-explained); [RetroHandheldHQ PS2](https://retrohandheldhq.com/emulators/ps2/)
- Snippets from R36S buyer guides rate PS2 "Not suitable". One seller page claims EmuELEC gives "playable performance on most titles". That claim conflicts with every other source and comes from a seller, so it is unreliable. — [androidpctv R36S review](https://androidpctv.com/?p=42621); [AliExpress wiki article](https://www.aliexpress.com/s/wiki-ssr/article/can-r36s-play-ps2); [r36s.co.uk](https://r36s.co.uk/blogs/news/does-r36s-support-ps2-games)
- A Scribd tutorial installs "YAPS2" on R36S/ArkOS, says the device is "far below the ideal specs" and expects only light 2D games near full speed. Low-quality source, snippet only. — [Scribd YAPS2 R36S](https://id.scribd.com/document/1068076810/Tutorial-YAPS2-R36S)
- AetherSX2 reference point: MGS3 at 30 FPS on a Snapdragon 860 (POCO X3 Pro). AetherSX2 is abandoned. Its successors are NetherSX2 and ARMSX2 (the latter "slowly progressing", Aug 2025). — [Notebookcheck MGS3](https://www.notebookcheck.net/The-AetherSX2-PlayStation-2-emulator-for-Android-has-no-trouble-running-Metal-Gear-Solid-3-at-30-FPS-on-the-Snapdragon-860-equipped-POCO-X3-Pro.596358.0.html); [Notebookcheck ARMSX2](https://www.notebookcheck.net/New-PlayStation-2-emulator-for-Android-promises-better-PS2-emulation-than-AetherSX2.1098980.0.html)
- PS2 emulators on Android need Android, which RK3326 Linux CFWs do not run. The handhelds.wiki R36S compatibility page lists NetherSX2 for PS2 only in its generic tables (search snippet). — [handhelds.wiki R36S Compatibility Lists](https://handhelds.wiki/R36S_Compatibility_Lists)
- Celosia's platform audit lists what the R36S stack lacks for emulators like these. Vulkan exists in the libmali `-gbm` blob (1.0.108) but has "NO display WSI" (no surface or swapchain extensions). Panfrost Vulkan fails on kernel 4.4. There is no desktop GL. So Vulkan-based emulator backends cannot present. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)

### Inferences
- On RK3326 the practical choice is native recompilation or a decompilation port. Emulation is not a fallback worth benchmarking for God Hand.

### Gaps
- I found no measured per-game PS2 FPS for RK3566 (RG353 series under Android/AetherSX2) or H700 (RG35XX family). The search returned only guides and impressions.
- I found no evidence that Play! (PS2 emulator) has been benchmarked on RK3326.

## 3. Native ports of heavy 3D games on RK3326 via PortMaster, and the techniques that made them run

### Takeaway
Several N64-, PC- and PS2-era 3D games run natively on RK3326 through PortMaster: Perfect Dark decomp, Half-Life (Xash3D), Quake 3, SRB2Kart, SuperTuxKart, RVGL, Serious Sam TSE, and the PS2 Need for Speed: Hot Pursuit 2. The heaviest are flagged `power` (Doom 3, Jedi Academy), which keeps them off weak devices. The techniques that recur are: a GLES2/GLES3 backend (or gl4es only for desktop-GL engines), fewer draw calls, armhf builds where data layouts are 32-bit, a separate render/present thread, shader-program caches, quality presets that cut reflections, particles and resolution, and pinning clocks.

### Cited Findings
**PortMaster catalogue facts (port.json, read 2026-11-27)**
- `perfectdark` (n64decomp port by fgsfdsfgs): arch aarch64, no `power` requirement. The README says "gl4es is included with the port. However, the binary will fall back on GLES on platforms where that is necessary." — [PortMaster-New ports/perfectdark](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/perfectdark)
- `half-life`: Xash3D FWGS, launched with `-ref gles2`, aarch64 and armhf. — [PortMaster-New ports/half-life](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/half-life)
- `doom3` (d3es-multithread) and `jediacademy` (OpenJK) both carry `"reqs": ["power"]`. `openmw` (Morrowind) requires `!lowres` and `analog_2`, which excludes 480p-class screens. — [PortMaster-New ports/doom3](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/doom3); [ports/openmw](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/openmw)
- No Ship of Harkinian, 2Ship2Harkinian, SM64, Starship, SpaghettiKart or OpenGOAL entry exists in the PortMaster-New `ports/` tree as of 2026-11-27. SoH builds for R36S circulate outside PortMaster (e.g. fpasteau/Shipwright_R36S, a fork last committed 2024-04-07 with no R36S notes in its README). A search snippet from that repo's release notes says the old ArkOS build is slow and "Compatibility mode is too slow", so use the "Performance" binaries. — [PortMaster-New repo](https://github.com/PortsMaster/PortMaster-New); [fpasteau/Shipwright_R36S releases](https://github.com/fpasteau/Shipwright_R36S/releases)
- A vendor blog claims SoH on R36S can "expect gains over 20 FPS" but won't hold high targets everywhere. This is an impression from a seller-adjacent site. — [2bluebox Zelda on R36S](https://2bluebox.com/blogs/r36s/zelda-ocarina-of-time-majoras-mask-r36s-guide)

**Need for Speed: Hot Pursuit 2 (PS2) on R36S: the other PS2-era native port with device measurements**
- In the PortMaster catalogue (`nfshp2`, porter manuelpllull): "a native port of the PS2 version ... adapted from the New 3DS port" (a decompilation). It uses the user's PS2 ISO, is armhf only ("The game can't run in 64-bit"), and was "Tested on an R36S with dArkOS". — [PortMaster-New ports/nfshp2](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/nfshp2)
- Device run 1 (2026-11-07, R36S, dArkOSRE, Mali-G31 blob r13p0, clocks at top step): 10.1 / 16.8 / 21.6 FPS (p10/median/p90). There were 1,022 draws/frame median, and the frame cost fit was "18.8 ms + 42 µs per draw (r² 0.93)". World simulation took only 2.4 ms median. The profile was 49.8% Mali driver, 19% libc, 30% game plus GL layer. — [NFSHP2-R36S docs/R36S_OPTIMISATION_BACKLOG.md](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md)
- Device run 7 (2026-11-14, dArkOSRE, 640×480 at 77 Hz, armv7 build, quality 0): 27.3 FPS median (23.7–32.2 p10–p90). That was 744 draws, the GL thread busy 19.6 ms, the swap 16.9 ms (0.4 ms of it CPU) and the GPU 54% busy at 520 MHz. Slow stretches (smoke, traffic) were GPU-bound at 55–68 ms. A user saw "lots of particles and vegetation drops to 16 fps or so" (run 6, slowest window 14.9 fps, median 26.0, RSS 275 MB). — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md)
- The tester post summary: "R36S (640x480): runs, about 27 fps in races". On the RG34XX (H700, Knulli) it was installed but there is no result yet. — [NFSHP2-R36S docs/R36S_TESTERS_POST.md](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_TESTERS_POST.md)
- For comparison, the New 3DS build runs at 30 FPS and drops to 20 in heavy stretches. — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md)
- Techniques used or measured:
  - Moving baked meshes into VBOs saved only about 3 µs/draw (−1 to −3 ms), not the 10–18 ms estimated.
  - Grouping draws plus a dedicated GL thread took the median from 16.8 to 27.7 FPS (run 3).
  - A program (shader) cache was added after shader compiles of 73–135 ms per program caused stutters (up to 263.8 ms in one frame).
  - A present thread with a second GL context and GLES 3 fences overlaps GPU work with the next frame's submission (estimated 27 → 33–38 fps, not yet measured).
  - In-game Graphics presets: Fast redraws car reflections and the mirror less often. Fastest also renders 3D at 75% size. A Smoke setting thins particles.
  - Run-time merging of adjacent draws merged under 1% of draws (also seen in a Burnout 3 port), so it was dropped. Sorting blended draws by texture breaks ordering.
  
  — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md); [PortMaster nfshp2 README](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/nfshp2)
- Why armhf: 91 of the disc's in-place records hold pointers laid out for 4-byte pointers. The aarch64 build read the car table 8 bytes short per car. "The A35 runs 32-bit ARM code at full speed", and the draws, not the ISA, are the limit. — [NFSHP2-R36S docs/R36S_PORT.md](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_PORT.md)

**Celosia (SotC) renderer techniques measured on R36S**
- Mali TBDR hygiene took the render median from 17.30 to 14.90 ms. It used fewer scene copies, depth/stencil sync tracking, and `glInvalidateFramebuffer` on overwritten targets and at swap. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- `GL_TEXTURE_2D_ARRAY` buckets plus static per-mesh VAOs cut GL draw calls by about 40–43% and CPU render time by about 36–40% (18.62 → 11.97 ms heavy peak). Promoting 32–128 px textures to a 128×128 bucket plus `glDrawRangeElements` cut per-draw cost from 35.3–37.5 to 30.9–34.1 µs/call. Sorting by program/bucket cut Shrine draws from 183 to 124. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- These were measured and rejected: UBOs (+15 µs/call on libmali r13p0), and a no-discard variant (+1.2 to +4 ms CPU). A worker-thread split for character processing (`SOTC_CHAR104`) was also rejected: the game thread waited 3.4 ms/frame on synchronization on 4× A35. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Build-level gains: PGO alone gave about +2 FPS (25.6 → 27.0 and 27.3 → 29.4). Other measures were function-section ordering from on-device profiles, `-O3` on hot code and `-Os` on cold recompiled batches, selective LTO, and 4 GiB "fastmem" guest mapping with an mmap + SIGSEGV trap. Compiling out guest read-watch, write-trace and runtime logs, which defaulted ON for non-Vita builds, was part of the Phase A gain (31.06 → 33.0 median). — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md); [r36s_native_audit.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_native_audit.md)
- A CPU probe on the R36S measured: L1 hit 2.8 ns, RAM (16 MB stride) 195.1 ns, hot call 32.7 ns, and a "cold over 151 functions" call 4,021.5 ns. Code-size and cache locality dominate on the A35. — [Celosia SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/main/SOTC_OPTIMIZATION_LOG.md)

**Other recompilation data point**
- Takedawn (Burnout 3, Xbox static recompilation, x86-64 and arm64) lists no R36S support. Its weakest tested handheld, the Retroid Pocket Classic (Snapdragon G1 Gen 2), "Starts, but 30 fps at most, so the game runs slow". The game advances one step per frame, and the README offers a "Potato mode" to keep real time below 60 fps. — [Takedawn README](https://github.com/xenophobentx/takedawn)

### Inferences
- In both PS2-era R36S ports with device measurements, the cost per draw through libmali is about 30–42 µs. That caps the frame at roughly 400–500 draws for 60 fps, or about 800–1,000 for 30 fps, before any game logic. God Hand's renderer must batch aggressively (texture arrays, static VAOs, sort by program) from day one.
- A PS2 frame emitted primitive-by-primitive from GIF/VU1 output will be far over budget. The VU1 pre-compilation path should emit one GL draw per VU1 batch, with the transform done in the vertex shader, as Celosia planned.
- Measure CPU and GPU threads separately. NFSHP2 was driver-thread-bound and Celosia is game-thread-bound, so the fixes differ.

### Gaps
- I found no measured FPS for Perfect Dark, Half-Life, Quake 3 or SoH on RK3326 specifically. Only catalogue metadata and impressions were available. Per-port device test results are kept on PortMaster's Discord, which is not reachable.
- The exact meaning of the PortMaster `power` requirement tag is not documented in the files read. By its use on Doom 3 and Jedi Academy, it appears to exclude low-power SoCs such as RK3326, but that is my inference.

## 4. OpenGOAL (Jak and Daxter) on RK3326 / PortMaster

### Takeaway
No OpenGOAL build exists in PortMaster or for RK3326 as of 2026-11-27. OpenGOAL has historically required x86-64 with AVX. A July 2026 article reports ARM64 progress but says it is not complete. So OpenGOAL is not a measured data point on RK3326. Celosia (SotC) and NFSHP2 are the real analogs.

### Cited Findings
- The PortMaster-New `ports/` tree (1,522 ports, 2026-11-27) has no `opengoal` or `jak*` entry. — [PortMaster-New](https://github.com/PortsMaster/PortMaster-New)
- Handheld guides (snippets only, sites unreachable) state that OpenGOAL needs "x86-64 CPU with AVX" and has "no ARM build". — [heldgames Jak OpenGOAL handheld](https://heldgames.com/guides/jak-daxter-opengoal-handheld); [heldgames decomp ports](https://heldgames.com/guides/decomp-ports-portmaster-handhelds)
- A July 2026 article title and snippet: "OpenGOAL update brings Jak and Daxter PC port closer to ARM64". It says the port "isn't fully there on ARM64 yet" (snippet only). — [Generation Amiga, 2026-07-04](https://www.generationamiga.com/2026/07/04/opengoal-update-brings-jak-and-daxter-pc-port-closer-to-arm64/)
- OpenGOAL's own description: it decompiles the GOAL code and recompiles it for PC. Jak 1 is complete, and Jak 2 and 3 were in beta at the time of the reports. — [open-goal/jak-project](https://github.com/open-goal/jak-project); [PC Gamer](https://www.pcgamer.com/games/action/the-most-hardcore-jak-and-daxter-heads-in-the-world-have-finished-their-native-pc-ports-of-the-entire-trilogy/)

### Inferences
- OpenGOAL's renderer targets desktop OpenGL 4.x-class features. Even with an ARM64 CPU build, it would need a GLES backend before it could run on libmali GLES 3.2. Its absence on RK3326 is therefore not evidence about recompiled PS2 code per se.

### Gaps
- I could not read the Generation Amiga article or the OpenGOAL issue tracker for current ARM64 or GLES status (host blocked in this session).

## 5. Mali-G31 / ArkOS-family driver specifics that ports work around

### Takeaway
On stock R36S CFWs (ArkOS, dArkOS, dArkOSRE, kernel 4.4), the only accelerated 3D path is the proprietary libmali r13p0 blob: GLES 3.2 (plus 1.1), reached through SDL2 KMSDRM. The rest of the stack has four problems:
- There is no desktop GL, and gl4es is used only by desktop-GL engines.
- Vulkan cannot present.
- Calling raw EGL-on-GBM on the render node crashes the kernel.
- Swap interval cannot be turned off, and some panels refresh at about 77 Hz instead of 60.

### Cited Findings
- Celosia's measured capability table (2026-11-15, R36S, Debian 13, kernel 4.4.189) — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md):

  | Item | Measured behaviour |
  | --- | --- |
  | GLES | GLES 3.2 / ES 1.1 via `libMali.so` is "the only hardware-accelerated 3D graphics path" |
  | Display path | SDL2 2.32.10 KMSDRM, with rotation through `/dev/rga` |
  | `SDL_GL_SetSwapInterval(0)` | Succeeds, but the actual interval stays 1 (60 Hz flip). Present takes about 16–18.5 ms, but asynchronously (`wait 0.0`). A `clock_nanosleep` FPS cap was used instead |
  | EGL on GBM | `eglInitialize` on `EGL_PLATFORM_GBM_KHR` on `/dev/dri/renderD128` gives a "Fatal Kernel Oops ... hangs the device until hard power-off; always use SDL2 KMSDRM" |
  | Vulkan | libmali Vulkan 1.0.108 has no surface or swapchain extensions. Panfrost Vulkan fails (needs kernel ≥5.2) |
  | Desktop GL / Mesa / X11 / Wayland | Absent. gl4es is not installed system-wide |
  | VPU video decode | Unusable |
  | Missing syscalls (ENOSYS) | `clone3`, `statx`, `io_uring`, `pidfd_open`, while glibc 2.41 headers expect them |
  | Missing kernel features | memory cgroups, PSI, overlayfs |
  | Device-freezing actions | Scanning `/proc/*/fd`; `ffmpeg` kmsgrab or fbdev capture is risky |

- NFSHP2 on an R36S with dArkOSRE found the panel locked at 12.9 ms/frame with vsync, so a 77.4 Hz panel. Steady rates are then 77.4 / 38.7 / 25.8 fps, and "30 fps needs a 60 Hz mode, or frames under 25.8 ms". This conflicts with Celosia's 60 Hz KMSDRM report. R36S units or firmware display modes differ, so the panel rate must be measured per unit. — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md); [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- libmali r13p0 per-draw cost was measured at about 28 µs on the submitting thread plus about 11 µs in the swap, "within the NFS ports' 22–34 µs". Client-side vertex arrays force the driver to scan indices and copy vertices on every draw. — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md)
- Shader compiles on the device take 73–135 ms per program, so a persistent program binary cache tied to the driver version is needed. Celosia also keeps on-disk Mali shader, texture and mesh caches. — [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md); [Celosia sotc.sh](https://github.com/amarqs182/celosia/blob/main/ports/r36s/sotc.sh)
- PortMaster's general guidance: native ARM ports target GLES2 or GLES3. gl4es is a translation layer for desktop-GL ports. Panfrost is preferred where available. DRM/KMS is recommended but some devices are fbdev-only (noted for H700). zram/swap is recommended for memory-hungry ports. — [bmdhacks PortMaster CFW compatibility guide](https://gist.github.com/bmdhacks/f26dbb60dc0ff0b9bc8e043fac8b813b)
- PortMaster launchers handle 32-bit ports with `PORT_32BIT="Y"` and `/usr/lib32`. R36S firmwares ship the 32-bit libMali for armhf ports. — [PortMaster nfshp2 launcher](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/NFSHP2.sh); [NFSHP2-R36S docs/R36S_PORT.md](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_PORT.md)
- Memory: about 708–726 MiB of 895 MiB is usable. Celosia enables 511 MiB zram. NFSHP2's RSS levelled at 275 MB with no swap after a leak fix. — [Celosia docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md); [NFSHP2-R36S backlog](https://github.com/manuelpllull/NFSHP2-R36S/blob/main/docs/R36S_OPTIMISATION_BACKLOG.md)

### Inferences (lessons for the God Hand port)
- **Graphics stack.** Use GLES 3.x through SDL2 KMSDRM only. Never call EGL on GBM directly on the R36S. Skip Vulkan. Test swap-interval behaviour and the panel's real refresh rate on each target unit.
- **Pacing.** Pace with a monotonic-clock cap or a fixed game step (Celosia's Steady-20 / Cap-30), not with `SetSwapInterval`.
- **Per-draw cost.** Budget about 30–40 µs per draw. Use texture arrays, static VAOs and VBOs, `glDrawRangeElements`, program-sorted opaque draws, and `glInvalidateFramebuffer`. Avoid UBOs on r13p0, and do not expect run-time merging of adjacent draws to help.
- **Shader caching.** Ship or precompute a shader program binary cache.
- **Runtime build.** Strip the PS2Recomp runtime's debug instrumentation from release builds: `PS2X_GUEST_READ_WATCH`, `PS2X_GUEST_WRITE_TRACE`, runtime logs, aggressive logs and IOP RPC trace default ON outside the Vita. Then add on-device PGO and hot-function ordering. These were cheap, measured wins of several FPS in Celosia.
- **Clocks and memory.** Pin CPU to 1512 MHz, and GPU and DMC to their maximums. Enable zram. Keep RSS well under about 700 MiB.
- **Threading.** On 4× A35, game logic plus one render/GL thread (and an optional present thread) works. Finer-grained worker splits of game logic lost time to synchronization in Celosia.

### Gaps
- No public measured comparison of ArkOS vs dArkOS vs ROCKNIX (Panfrost, mainline kernel) for the same 3D port on RK3326 was found. ROCKNIX, with mainline Panfrost GLES 3.1, may behave differently in per-draw cost.
- No data was found for R36H or R36 Max (both RK3326) or for the OGA/OGS/RG351 family with these specific ports, apart from catalogue arch flags.
