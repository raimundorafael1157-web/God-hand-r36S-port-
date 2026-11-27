# Technical routes for Shadow of the Colossus (PS2) on the R36S (RK3326)

Researched 2026-11-27. Several hosts were blocked in this session (wiki.pcsx2.net, forums.pcsx2.net, gamingonlinux.com, dsogaming.com, gamingbible.com, handhelds.wiki), so some items rely on search-result snippets rather than full page reads; these are marked "(snippet)". GitHub READMEs were read directly via raw.githubusercontent.com. Repo-local prior art (`research/ports-r36s.md`, `research/prior-art.md` in this repository) was also used.

## Is there any public decompilation or static recompilation of SotC (PS2)? Status and license?

### Takeaway
Yes. There is an active matching decompilation (TheRealestNwah/sotc-decomp, CC0-1.0, ~89% of game-module functions matched as of late Nov 2026, native port not started), a smaller second decomp, and a dormant one-commit static-recompilation repo from 2024. No playable native build of SotC exists for any platform, ARM or x86, as of 2026-11-27.

### Cited Findings
- **TheRealestNwah/sotc-decomp**: "work-in-progress matching decompilation of Shadow of the Colossus (PlayStation 2, NTSC-U, `SCUS-97472`), with the long-term goal of a Native port to PC". License CC0-1.0 (per GitHub API), 55 stars, last push 2026-11-27 — [README](https://github.com/TheRealestNwah/sotc-decomp)
  - Structure: the loader `SCUS_974.72` (1,038 functions) splits with splat and rebuilds byte-for-byte; almost all game code lives in three overlay modules `KERNEL.XFF`, `MANAGER.XFF`, `GAMECORE.XFF` (custom "xff2" format). All four files rebuild byte-for-byte — [README](https://github.com/TheRealestNwah/sotc-decomp)
  - Compiler identified: Sony `ee-gcc` 2.96, `-O2 -G0` — [README](https://github.com/TheRealestNwah/sotc-decomp)
  - Progress table (auto-generated, may lag): Loader game code 113/123 (92%); KERNEL 1,004/1,070 (94%); MANAGER 2,436/2,867 (85%); GAMECORE 3,581/3,939 (91%); all three modules 7,021/7,876 (89%); **Native PC port: not started (0%)** — [README](https://github.com/TheRealestNwah/sotc-decomp)
  - "Overall, a playable build needs all three bars full." No game binaries/assets in repo; user must dump own disc. Development uses Claude Code and OpenAI Codex (AI disclosure in README) — [README](https://github.com/TheRealestNwah/sotc-decomp)
- **NathanFahey23/Shadow-of-the-Colossus-Decomp**: matching decomp (NTSC-U) started with decomp.wtf; "582 of 1043 functions (55.80%) and 36.65% of code bytes" match. The 1,043-function total suggests it covers only the boot ELF, not the XFF overlays (inference). No license file; README says "keep this tree to yourself unless you have the right to share it". Last push 2026-10-17 — [README](https://github.com/NathanFahey23/Shadow-of-the-Colossus-Decomp)
- **dylking2381/dylk-sotc-decomp**: "Manual Decomp of Shadow of the Colossus for PS2", 0 stars, no license, last push 2026-11-06 — [GitHub search result](https://github.com/dylking2381/dylk-sotc-decomp)
- **InconspicuousCactus/SotCStaticRecompilation**: "An in progress port of Shadow of the Colossus (PS2) to PC using static recompilation"; 1 commit, 13 stars, folders GAMECORE/KERNEL/MANAGER/MainElf, no license shown, last push 2024-06-08 (dormant) — [GitHub](https://github.com/InconspicuousCactus/SotCStaticRecompilation)
- No repository matched the PAL serial or "SCUS-97472" by name in GitHub repository search (search run 2026-11-27); the serial appears only inside the sotc-decomp README.
- **PS2Recomp (ran-j)**: generic PS2 static recompiler: ELF -> literal C++ (`addiu $r4,$r4,0x20` -> `ctx->r4 = ADD32(ctx->r4, 0X20);`), with runtime (memory, syscalls, GS, VU1, IOP HLE). Requirements list "C++20 compiler (currently tested mainly with MSVC)" and "SSE4/AVX host support for some vector paths" — [README](https://raw.githubusercontent.com/ran-j/PS2Recomp/main/README.md). GPL-3.0 per press coverage — [GIGAZINE](https://gigazine.net/gsc_news/en/20260130-ps2recomp) (snippet); early 2026 coverage said no playable games yet — [Time Extension, Jan 2026](https://www.timeextension.com/news/2026/01/native-pc-ports-of-ps2-games-could-be-on-the-way-thanks-to-new-recompilation-experiment) (snippet)
- **Sister-game prior art, ICO**: nathanialf/ico-recomp statically recompiles ICO (PAL SCES-50760) following N64Recomp; EE code to C, and "the five VU1 microprograms are statically recompiled as well"; GS via paraLLEl-GS (Vulkan compute) — pre-alpha, "boots, draws and plays with sound, measured on 2026-09-04" — [README](https://raw.githubusercontent.com/nathanialf/ico-recomp/HEAD/README.md). A separate matching ICO decomp exists (RossyDoubleUnderscore/ICO-decomp) — (snippet via search)
- SotC's VU1 does heavy work: "most of the shadow volume generation and outline of the 3d characters are done on the VU1" — [PCSX2 forum](https://forums.pcsx2.net/Thread-FPS-Issues-Shadow-of-the-Colossus) (snippet)

### Inferences
- The decomp route (b) is the only one with real momentum for SotC. Matching C for game logic does not solve rendering: SotC's VU1 microcode (shadow volumes, outlines, skinning, likely terrain/fur) and GS usage would have to be reimplemented in GLES for a Mali-G31, as the successful RK3326 ports did (BT3, THUG, Strikers per this repo's `research/ports-r36s.md`: none of the heavy 3D ports that run well on RK3326 emulate the original GPU; all draw directly in GLES).
- A PS2Recomp-style literal translation (as used in this repo's God Hand work) is technically applicable to SotC's loader, but SotC loads its game code from XFF overlay modules, so a recompiler must handle relocatable overlays; sotc-decomp has documented the xff2 format ([docs/xff2.md](https://github.com/TheRealestNwah/sotc-decomp)), which is useful input.
- SotC ran at a low, unstable framerate on real PS2 (see next section); even a perfect native port at PS2 parity on a Cortex-A35 would inherit heavy per-frame CPU/VU loads. Feasibility on the RK3326 is unknown until measured.

### Gaps
- License status: sotc-decomp is CC0 for its own code, but the game remains Sony's IP; no legal analysis done.
- No information on any SotC port targeting ARM, GLES or handhelds. No published CPU-time measurement of SotC game logic on any ARM core.
- Whether the PAL (SCES-53326) or NTSC-J versions have any project: none found.

## Do any PS2 emulators run on RK3326, and how fast? (AetherSX2/NetherSX2, ARMSX2, PCSX2 forks, Box64)

### Takeaway
No PS2 emulator is shipped or supported for RK3326 by any major handheld distro; ROCKNIX explicitly enables AetherSX2/ARMSX2 only from RK3399/RK3566 upward. No measured result of any PS2 game on an RK3326 was found. Realistic expectation for SotC under emulation on a Cortex-A35: unplayable.

### Cited Findings
- ROCKNIX's emulator manifest: for `H700|RK3326` it adds only `drastic-sa mednafen` (+ uae4arm); `aethersx2-sa` and `armsx2-sa` are added for RK3399, RK3566/RK3576, RK3588, SM6115, SM4450, SM8250 etc. PS2 system uses `armsx2-sa` (default) and `aethersx2-sa` — [ROCKNIX emulators/package.mk](https://raw.githubusercontent.com/ROCKNIX/distribution/next/projects/ROCKNIX/packages/virtual/emulators/package.mk) (read 2026-11-27)
- ROCKNIX's aethersx2-sa package is "Bring your own Arm PS2 Emulator" (binary not distributed); on panfrost it forces `MESA_GL_VERSION_OVERRIDE=3.3` — [aethersx2-sa/package.mk](https://raw.githubusercontent.com/ROCKNIX/distribution/next/projects/ROCKNIX/packages/emulators/standalone/aethersx2-sa/package.mk)
- ArkOS README contains no PS2/AetherSX2 mention; GitHub code search for "aethersx2" in christianhaitian/arkos returned 0 hits — [ArkOS repo](https://github.com/christianhaitian/arkos) (checked 2026-11-27)
- AetherSX2 is closed-source and abandoned; NetherSX2 is a patched build of the last AetherSX2 — [Emulation General Wiki](https://emulation.gametechwiki.com/index.php/AetherSX2) (snippet); [Android Authority](https://www.androidauthority.com/aethersx2-ps2-emulator-3088419/) (snippet)
- ARMSX2 (PCSX2-based) got an initial Linux ARM release in July 2026; the reviewer had no Linux Arm system "powerful enough to run it for testing" and warned some Arm Linux machines "simply won't have enough performance" — [GamingOnLinux, 2026-07](https://gamingonlinux.com/2026/07/playstation-2-emulator-for-arm-devices-armsx2-gets-an-initial-linux-release) (snippet); [Generation Amiga, 2026-07-08](https://www.generationamiga.com/2026/07/08/armsx2-lands-on-linux-and-arm-gaming-handhelds-could-benefit/) (snippet)
- ARMSX2-PC (SetiQyu): experimental PCSX2 port for aarch64 Linux, only tested on Asahi Linux, no releases — [GitHub](https://github.com/SetiQyu/ARMSX2-PC) (snippet)
- R36S community resources rate the device as max PSP/PS1 era; R36S compatibility list has no PS2 data — [retrocatalog](https://retrocatalog.com/retro-handhelds/r36s) (snippet); [handhelds.wiki R36S Compatibility Lists](https://handhelds.wiki/R36S_Compatibility_Lists) (snippet, page blocked)
- Hardware: RK3326 = 4x Cortex-A35 up to ~1.5 GHz, Mali-G31, 1 GB DDR3L — [tech-insider R36S ArkOS guide](https://tech-insider.org/?p=25936) (snippet). Same source says R36S ArkOS was archived 2026-06-09 in favor of dArkOSRE-R36/ArkOS4Clone (unverified, single source).
- No verified reports or videos of any PS2 game on RK3326 were found; searches for God of War on R36S produced nothing confirming or debunking such claims.

### Inferences
- RK3399 (2x A72 + 4x A53) is ROCKNIX's floor for PS2 and is still considered marginal; the A35 is an in-order, low-power core much slower than A72, so PCSX2-derived JITs on RK3326 would run heavy titles far below full speed. This is inference, not a measurement.
- Box64 + Wine + x86 PCSX2 adds a translation layer on top of an already-JIT-dependent emulator and needs desktop GL/Vulkan, which ArkOS's Mali-G31 (kernel 4.4 libmali, GLES) setup does not provide natively; this route is strictly worse than native ARM builds.
- Memory: AetherSX2/ARMSX2 need the 32 MB EE RAM + fastmem address-space reservation plus Qt6/GL drivers; 1 GB total RAM is tight but not obviously the limiting factor; CPU/GPU are.

### Gaps
- No measured FPS/speed% for any PS2 game on RK3326, A35, or Mali-G31 under any emulator.
- Whether AetherSX2/ARMSX2 Linux builds run at all on ArkOS (glibc 2.30, kernel 4.4, libmali without Vulkan/desktop GL) is untested; ARMSX2 Linux glibc and GPU-API requirements could not be read (gamingonlinux blocked).

## SotC's emulation requirements and known PCSX2 behavior

### Takeaway
SotC is one of the heaviest PS2 titles: it drops frames on real hardware, leans on VU1, and on PCSX2 historically needed the EE Cycle Skipping (VU cycle stealing) hack to reach full speed even on desktop PCs.

### Cited Findings
- PCSX2 wiki: EE cyclerate 0; EE Cycle Skipping 1 gives "100% speed in most areas, as opposed to 70% at 0"; values above 1 make rendering choppier; cycle skipping can make distant textures translucent — [PCSX2 wiki SotC](https://wiki.pcsx2.net/Shadow_of_the_Colossus) (snippet)
- Forum, 2014: VU cycle stealing "drastically reduces the requirements"; "nearly impossible to run the game without it" — [PCSX2 forum](https://forums.pcsx2.net/Thread-FPS-Issues-Shadow-of-the-Colossus) (snippet)
- "SotC drops a lot of frames on a REAL PS2. VU stealing will exacerbate this" — [PCSX2 forum thread](https://forums.pcsx2.net/Thread-So-was-Shadow-of-the-Colossus-fps-limited) (snippet)
- Shadow volumes and character outlines are generated on VU1 — [PCSX2 forum](https://forums.pcsx2.net/Thread-FPS-Issues-Shadow-of-the-Colossus) (snippet)
- MTVU recommended on in a wiki revision; another user reported better results with it off — [PCSX2 wiki diff](https://wiki.pcsx2.net/Special:MobileDiff/99016...109828) (snippet)

### Inferences
- Because these findings are mostly 2012–2015 era, modern PCSX2 is faster, but SotC still requires significant EE+VU1 throughput; on an A35 the VU1 recompiler alone would likely exceed the frame budget.
- For a native/recomp route, the VU1 workload is the main risk: ico-recomp's approach (recompile VU1 microprograms statically) and this repo's God Hand result (precompiled VU1 raised intro speed from 10–23% to 93–100% on PC, per README) show static VU1 is necessary but not sufficient.

### Gaps
- Could not read the current PCSX2 wiki page or GitHub GameDB entry for SCUS-97472 (host blocked). No modern PCSX2 or ARMSX2 Android benchmark for SotC found.

## Remaster/HD versions and fan remakes (routes c and d), PortMaster

### Takeaway
The PS4 remaster now has an unofficial PC port (Oct 2026) via a shadPS4-derived layer, but it targets modern x86 GPUs and is irrelevant to RK3326. Fan remakes (Tribute, Unity; Beyond the Forbidden Lands, Unreal) target PC; none is on PortMaster. PortMaster has no SotC/ICO entry.

### Cited Findings
- AjncSystem's "ShadowOfTheColossus-PC" port of the 2018 PS4 remake, Oct 2026; sources disagree on the method: "built on the Bloodborne PC port and relies heavily on shadPS4" / static recompilation on shadPS4 framework vs. "not a full-fledged native port or a recompile" (unchanged PS4 executable, calls converted to Vulkan). Up to 60 FPS at 4K on RTX 5060 Ti; drops to ~40 FPS on RTX 2060 — [GameGPU](https://en.gamegpu.com/news/igry/nativnyj-port-shadow-of-the-colossus-dlya-pk-vydaet-do-60-fps-v-4k-na-rtx-5060-ti) (snippet); [DSOGaming](https://www.dsogaming.com/news/shadow-of-the-colossus-ps4-remake-gets-unofficial-pc-port/) (snippet); [BornCity](https://borncity.com/news/shadow-of-the-colossus-fan-port-von-ajncsystem-laeuft-nativ-unter-windows/) (snippet)
- "Tribute to The Shadow of the Colossus": Brazilian fan remake in Unity, planned for Windows/Mac/Linux; covered as a free PC remaster in April 2026 with day/night, weather etc. — [Team Ico Wiki](https://teamico.fandom.com/wiki/Tribute_to_Shadow_of_the_Colossus) (snippet); [GamingBible 2026-04-10](https://www.gamingbible.com/news/platform/pc/ps2-shadow-of-the-colossus-free-pc-remaster-992203-20260410) (snippet)
- "Beyond the Forbidden Lands": non-profit Unreal fan remake — [Team Ico Wiki](https://teamico.fandom.com/wiki/Beyond_the_Forbidden_Lands) (snippet)
- Dormin (Lenbok): SotC model/animation viewer and audio decoder, an asset tool, not a game port — [GitHub](https://github.com/Lenbok/dormin) (snippet)
- PortMaster ports.json (1,563 ports, fetched 2026-11-27) has no entry matching "colossus"/"sotc". It does list PS2-derived native ports for r36s: Need for Speed: Hot Pursuit 2 ("native port of the PS2 version ... adapted from the New 3DS port", armhf, min glibc 2.17, added 2026-11-24, needs user's ISO) — [PortMaster-Info ports.json](https://raw.githubusercontent.com/PortsMaster/PortMaster-Info/main/ports.json)

### Inferences
- PS4 remaster on RK3326: no realistic path (x86-64 Jaguar binary, GNM/Vulkan GPU workload designed for 1.84 TFLOPS GPU, 8 GB RAM vs Mali-G31's few tens of GFLOPS and 1 GB). PS3 HD version: RPCS3 has no ARM path suitable for this class of device (ROCKNIX ships rpcs3-sa only for SM8250-class); no realistic path.
- Unity/Unreal fan remakes built for desktop GPUs would not run on Mali-G31 even if Linux ARM builds existed; Unity's Linux ARM64 player exists but these projects target PC hardware. A "demake" would be a new game, not a port.

### Gaps
- Tribute's current release state (demo/full), license and whether a Linux build exists could not be verified (gamingbible blocked).
- AjncSystem repo contents/license not opened.

## Cloud/streaming and prior art for heavy PS2 games on R36S

### Takeaway
Streaming (Moonlight from a PC running PCSX2) is the only route that runs SotC "on" an R36S today; prior heavy-PS2 results on RK3326 come from decompilation/recompilation ports with custom GLES renderers, not emulation.

### Cited Findings
- Heavy 3D ports that run well on RK3326 (DBZ BT3/4 PS2, Super Mario Strikers GC, THUG Xbox) use decompiled code with renderers rewritten in GLES for Mali; THUG 25–40 FPS with simplified shadows — per this repo's survey [research/ports-r36s.md](../../research/ports-r36s.md), sources [tenkaichi-r36](https://github.com/impressivestay6355-cmyk/tenkaichi-r36), [thug-r36s](https://github.com/tuliocareli/thug-r36s), [smstrikers-rk3326](https://github.com/jbcgames/smstrikers-rk3326)
- God Hand (this repo): PS2Recomp-derived static recompilation built for ARM64/glibc 2.30; runs under qemu-aarch64, device measurement pending — [README](../../README.md)
- God Hand Android ARM64 port on a Snapdragon 7+ Gen 2 ran crowd scenes at ~42–58 fields/s with PCSX2 GS over Vulkan — this repo's [research/prior-art.md](../../research/prior-art.md) citing [Ptraaah/godhand-recompiled-android](https://github.com/Ptraaah/godhand-recompiled-android)

### Inferences
- Moonlight-embedded / Moonlight clients exist for ArkOS-class devices via PortMaster (not verified in this session); streaming SotC from a host PC with PCSX2 is technically trivial but is not running on the R36S.
- Any "God of War/SotC on R36S" video claim should be presumed streaming, a different device, or fake unless it shows a decomp/recomp project.

### Gaps
- Did not verify the presence of a Moonlight port in PortMaster for r36s in this session.
- No primary source found for any viral "PS2 on R36S" claim.
