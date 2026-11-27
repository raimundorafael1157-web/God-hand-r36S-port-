# PS2-era games in PortMaster: catalogue as of 2026-11-27

Method: I cloned `PortsMaster/PortMaster-New` (blob-less, sparse: `ports/*/port.json` + `README.md`). Its HEAD was `6fee745f` from 2026-11-27 14:58 +0100, with 1,522 port directories. I read every port title and grepped `port.json`/`README.md` for "PS2"/"PlayStation 2". I took dates from `git log`, read deleted ports from git history, and checked open and closed PRs on the GitHub web UI. The GitHub REST/GraphQL API was blocked in this session, so PR data comes from the web pages.

Repo: [PortMaster-New](https://github.com/PortsMaster/PortMaster-New)

## Which PortMaster entries correspond to PS2 games (verified against the catalogue)

### Takeaway
Only one entry in the current catalogue is built from the PS2 version itself: **Need for Speed: Hot Pursuit 2 (`nfshp2`)**, added 2026-11-23/24. It is armhf-only and was tested on the R36S.

A second PS2-era port built from a PS2-lineage version was **Bully: Anniversary Edition (`bully`)**. It is based on the Android build and was merged on 2026-07-15, then **delisted on 2026-07-22**.

Several other entries are games that also came out on PS2, but their ports come from the PC, Android or Dreamcast versions (Max Payne, Rez, UT99, Quake III, Half-Life, RtCW). The catalogue has **no** re3/reVC/reSA (GTA III/VC/SA), OpenGOAL/Jak, Sly, Ratchet, Kingdom Hearts, Silent Hill, Ico/SotC, Okami, God Hand or other PS2 decomp/recomp entries.

### Cited Findings

**Tier A: port built from the PS2 version**
- **`nfshp2` — Need for Speed: Hot Pursuit 2** (EA Black Box, PS2 2002).
  - **Porter:** `manuelpllull`.
  - **Arch:** `["armhf"]` only. `reqs: []`, `rtr: false`, `exp: false`, genre racing.
  - **Description:** "a native port of the PS2 version of Need for Speed: Hot Pursuit 2, adapted from the New 3DS port".
  - **Install:** copy a PS2 disc `.iso` (USA SLUS-20362, Europe SLES-50731, Asia SLPM-65181 or Korea SLPM-67527) into `ports/nfshp2/gamedata/`. The first launch installs the game (about 10 min, 3 GB free, needs python3). — [port.json](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/port.json)
- **`nfshp2` source and devices:**
  - **Source base:** decompilation. The README credits "Octoblimp and the contributors to the decompilation and New 3DS port it is built from". The decompiled code is in `3DS Files/source` and the handheld layer is in `r36s/`. Source is GPL-3.0 at github.com/manuelpllull/NFSHP2-R36S.
  - **Devices:** "Needs PortMaster on an armhf device (32-bit ARM with SDL2 and OpenGL ES). The game can't run in 64-bit. Tested on an R36S with dArkOS."
  - **Graphics:** in-game Graphics setting (Full/Fast/Fastest) and Smoke setting (Full/Less/Least).
  - — [README](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/README.md); [NFSHP2-R36S](https://github.com/manuelpllull/NFSHP2-R36S)
- **`nfshp2` disc versions conflict:** `port.json` accepts 4 disc regions, but the README says "a PAL PS2 disc (SLES-50731)". This looks like a doc inconsistency inside the package. — [README](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/README.md) vs [port.json](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/port.json)
- **`nfshp2` dates:** a single commit "Add ports/nfshp2: Need for Speed: Hot Pursuit 2 (armhf)", 2026-11-23 12:40 +0100. It came in via PR #2890 by manuelpllull, shown merged on Nov 24, 2026. — [PR search "PS2"](https://github.com/PortsMaster/PortMaster-New/pulls?q=is%3Apr+PS2)

**Tier A′: PS2-era game whose port came from a later version; now delisted**
- **`bully` — Bully: Anniversary Edition** (Rockstar; the original is a PS2 2006 game).
  - **Porter:** `NextOS`. PR #2564 was opened by felc18-blip / NextOs-Ports.
  - **Arch:** `["aarch64"]`, `reqs: []`.
  - **Description:** "for AArch64 Linux handhelds through the NextOS native Android so-loader. Includes automatic low-memory and GLES2/GLES3 profiles…"
  - **Install:** needs a legally owned ARM64 Android copy, version 1.4.311 (APK/splits/APKS/APKM/XAPK).
  - **Dates:** added 2026-07-14, PR merged 2026-07-15 by Cebion. — [PR search "PS2"](https://github.com/PortsMaster/PortMaster-New/pulls?q=is%3Apr+PS2); port.json read from git history (commit 744790a09^2)
- **Bully delisting:** PR #2579 (Ganimoth, "Delisting Bully port") was merged 2026-07-22. Reason given: "unclear instructions for sourcing apk causing way too much issues for most users". The whole `ports/bully/` directory was deleted. — [PortMaster-New commit history](https://github.com/PortsMaster/PortMaster-New/commits/main)

**Tier B: multiplatform games that also had PS2 versions, but the port is based on another version**
- **`maxpayne` — Max Payne**
  - **Porter:** orktes.
  - **Arch:** aarch64, `reqs: []`.
  - **Base:** "Max Payne Mobile Android version ported to ARM64 Linux devices". Needs the Android APK v1.7 + OBB. Source is at github.com/orktes/max_amd64.
  - **Dates:** first added 2025-10-18 (v0.1.2). orktes updated it through v0.1.42 on 2025-11-25. Later fixes: dArkOS flashing-screen fix by jrosetto (2026-07-29/30) and a launcher update by Cebion (2026-08-16, latest).
  - — [port.json/README](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/maxpayne)
- **`rez` — Rez**
  - **Porter:** SerPbone; commits by Michael Pobega.
  - **Arch:** aarch64. `reqs: ["power","!lowres"]`.
  - **Base:** static recompilation of the **Dreamcast** version (tyrannotorus/recomp-dreamcast-rez, built on dream-recomp and dreamcomp). Needs a JP HDR-0178 or EU MK-5119250 V1.003 Dreamcast disc as .chd.
  - **Dates:** added 2026-11-16; latest update 2026-11-23 (gamedata placeholder).
  - — [rez README](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/rez)
- **`ut99` — Unreal Tournament '99**
  - **Porter:** ToastedFrog.
  - **Arch:** aarch64, `reqs: ["analog_2"]`.
  - **Base:** the OldUnreal 469e Linux ARM64 build with a GLES2 renderer. Needs PC game data.
  - **Dates:** added 2026-09-05; latest update 2026-09-09.
  - — [ut99](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/ut99)
- **`quake3` — Quake III Arena**
  - **Porter:** brooksytech. Arch: aarch64.
  - **Game data:** PC pak files + CD key.
  - **Dates:** latest update 2025-01-14. It predates the 2024-01-14 move into `ports/`.
  - — [quake3](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/quake3)
- **`half-life` — Half-Life**
  - **Porter:** kloptops. Arch: aarch64 + armhf.
  - **Game data:** Steam valve dir.
  - **Dates:** latest update 2025-09-27.
  - — [half-life](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/half-life)
- **`iortcw` — Return to Castle Wolfenstein**
  - **Porter:** Johnny on Flame. Arch: aarch64.
  - **Base:** iortcw; needs PC pk3 files.
  - **Dates:** latest update 2026-03-07 ("dynamically scale display").
  - — [iortcw](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/iortcw)

**Tier C: fan demakes of PS2 games (no original data)**
- **`megamanx816-bit` — Mega Man X8 16-bit demake**
  - **Porter:** Dia.
  - **Base:** fan game; the files are downloaded from sonicfangameshq.
  - **Dates:** added 2024-08-19; latest update 2025-08-21.
  - — [megamanx816-bit](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/megamanx816-bit)

**Searched and not present:** no directory or title in the 1,522 ports matches GTA III/VC/SA (re3/reVC), Jak/OpenGOAL, Sly, Ratchet & Clank, Kingdom Hearts, Silent Hill, Ico, Shadow of the Colossus, Okami, God Hand, Burnout, SSX, Tony Hawk, Katamari, Psychonauts and similar. The full-text grep for "PS2"/"PlayStation 2" matched only `nfshp2`. No such port appears in the deleted-ports history either; the only PS2-era deletion is `bully`. — [ports/ directory](https://github.com/PortsMaster/PortMaster-New/tree/main/ports)

### Inferences
- Release history I did not cite: Max Payne (PS2 2001), UT (PS2 2000), Quake III Revolution (PS2 2001), Half-Life (PS2 2001), RtCW: Operation Resurrection (PS2 2003) and Rez (PS2 2002) all had PS2 releases. These are general knowledge and should be checked before publishing.
- **Excluded, no PS2 release:** Doom 3 (Xbox/PC), Jedi Outcast/Academy (PC/GC/Xbox), MoH: Allied Assault (PC; PS2 got Frontline instead), Morrowind (PC/Xbox), Serious Sam TFE/TSE (PC/Xbox), AvP 1999, Counter-Strike 1.6 and GTA2 (a PS1-era game).
- `nfshp2` is in practice the first and only PS2-derived *decompilation* port in PortMaster. It is the closest precedent for a God Hand R36S port: armhf, built from a decompilation by way of a 3DS port, game installed from the user's ISO on first launch.

### Gaps
- I did not check the PortMaster "multiverse"/third-party sources or the portmaster.games website/ports.json directly. The repo is the source the website is built from.
- I could not fetch the PR bodies of #2890 and #2564 because the API was blocked.

## Which were added or updated most recently (2025–2026), in chronological order?

### Takeaway
Recent PS2-era activity, oldest first:
1. Max Payne (Oct 2025)
2. RtCW update (Mar 2026)
3. Bully added (Jul 15, 2026), then delisted (Jul 22, 2026)
4. Max Payne fixes (Jul–Aug 2026)
5. UT99 (Sep 2026)
6. Rez (Nov 16, 2026; based on Dreamcast)
7. NFS Hot Pursuit 2 (Nov 23–24, 2026), the most recent and the only port built from the PS2 version

### Cited Findings

| Date | Port | Event |
|---|---|---|
| 2025-01-14 | quake3 | update (bind_directories, pm_finish) |
| 2025-08-21 | megamanx816-bit | latest update |
| 2025-09-27 | half-life | update |
| 2025-10-18 | maxpayne | added (v0.1.2). Updates on 10-25, 10-29, 11-05, 11-19, 11-25 (v0.1.42) |
| 2026-03-07 | iortcw | display scaling update |
| 2026-07-14/15 | bully | added via PR #2564 |
| 2026-07-22 | bully | delisted via PR #2579 |
| 2026-07-29/30 | maxpayne | dArkOS flashing-screen fix |
| 2026-08-16 | maxpayne | launcher update |
| 2026-09-05 | ut99 | added; updated 2026-09-09 |
| 2026-11-16 | rez | added; updated 2026-11-23 |
| 2026-11-23 | nfshp2 | added; PR #2890 merged 2026-11-24 |

All dates come from `git log` on [PortMaster-New](https://github.com/PortsMaster/PortMaster-New/commits/main), cloned on 2026-11-27.

### Inferences
- The PS2-derived ports are very recent (late 2026) and are driven by single porters. Expect follow-up update commits on `nfshp2`.

### Gaps
- None beyond the PR-body gap above.

## Which PS2 recompilation/decompilation ports are pending (open PRs, announcements) but not merged?

### Takeaway
As of 2026-11-27 there are **no open PS2 port PRs** in PortMaster-New. The other recomp PRs in the queue are for other consoles: Sin and Punishment (N64 recomp, #2902) and DaytonaArm (#2886). PS2Recomp, the generic tool, was still described as immature in early 2026.

### Cited Findings
- There are 16 open PRs (#2703–#2906), and none is PS2-related. Recomp-related open PRs:
  - #2902 "Add Sin and Punishment (snprecomp GLES3)" (Pobega, Nov 27, 2026), an N64 game.
  - #2886 "Add DaytonaArm" (DanimanX, Nov 21).
  - Other open PRs include Streets of Rage 4, Little Big Adventure 2, Fear & Hunger and Airline Tycoon Deluxe.
  - — [open PRs](https://github.com/PortsMaster/PortMaster-New/pulls?q=is%3Apr+is%3Aopen); [recomp PRs](https://github.com/PortsMaster/PortMaster-New/pulls?q=is%3Apr+recomp)
- The "PS2" PR search returns only #2890 (nfshp2, merged) and #2564 (Bully, merged). — [PR search](https://github.com/PortsMaster/PortMaster-New/pulls?q=is%3Apr+PS2)
- **PS2Recomp (ran-j):** a static recompiler inspired by N64Recomp. Coverage in Jan–Feb 2026 said it was not yet a practical way to play games. — [Time Extension 2026-01](https://www.timeextension.com/news/2026/01/native-pc-ports-of-ps2-games-could-be-on-the-way-thanks-to-new-recompilation-experiment); [Obsolete Sony](https://obsoletesony.substack.com/p/what-is-ps2-recomp); [Retro Handhelds](https://retrohandhelds.gg/ps2recomp-porting-playstation-2-games-to-pc/)
- **Separate HP2 PC port:** search snippets report a Windows (Vulkan) port of NFS HP2 by "ZloiKILLER", described as made by static recompilation of the PS2 version. This is a different project from the PortMaster/3DS decompilation. I could not fetch the article because the host was blocked. — [GameGPU (snippet only)](https://en.gamegpu.com/news/igry/need-for-speed-hot-pursuit-2-s-playstation-2-poluchila-nativnyj-port-dlya-pk)
- **GTA:** re3/reVC/reLCS have native New 3DS ports (reGTA Collection, Sept 2026), but there is no PortMaster package. — [Generation Amiga](https://www.generationamiga.com/2026/09/16/gta-iii-vice-city-and-liberty-city-stories-get-native-new-nintendo-3ds-ports/)

### Inferences
- The 3DS homebrew scene (NFS HP2, reGTA) is acting as a pipeline for PS2-era decomp ports that could reach PortMaster later. Nothing is in PortMaster's queue yet.

### Gaps
- I did not search Discord or Reddit. PortMaster Discord WIP channels cannot be indexed, so pending work there is unknown.

## Which run on RK3326 (R36S/RG351) vs only stronger devices?

### Takeaway
`nfshp2` explicitly targets armhf and was tested on the R36S (RK3326). It cannot run on 64-bit-only systems. Rez needs `power` (excludes RK3326-class devices). Max Payne, UT99, Quake3 and RtCW are aarch64 with no `power` flag, and Half-Life ships both arches.

### Cited Findings
- **nfshp2:** "armhf device… can't run in 64-bit. Tested on an R36S with dArkOS". — [README](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/README.md)
- **rez:** `reqs: ["power","!lowres"]`. — [rez port.json](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/rez)
- **maxpayne:** `reqs: []`, aarch64. A dArkOS-specific fix in Jul 2026 implies it is used on dArkOS (R36S-class) devices. — [maxpayne history](https://github.com/PortsMaster/PortMaster-New/commits/main/ports/maxpayne)
- **ut99:** `reqs: ["analog_2"]`. **bully** (delisted) had `reqs: []` with auto low-memory profiles. **half-life:** aarch64 + armhf. — port.json files in the [repo](https://github.com/PortsMaster/PortMaster-New/tree/main/ports)

### Inferences
- In PortMaster, the `power` flag is the usual way to exclude weak RK3326/H700-class devices. Ports without it are nominally allowed on R36S, but real performance is unverified.

### Gaps
- I found no per-device benchmarks for Max Payne, UT99 or the delisted Bully on RK3326 vs H700/RK3566.
