# Shadow of the Colossus (PS2) "portado" para o R36S: alegação e procedência

Pesquisa feita em 2026-11-27. Fontes primárias: o repositório GitHub `amarqs182/celosia`, clonado e lido localmente (HEAD `39ab080`, commit de 2026-11-27 11:07 -0300), páginas de PR no GitHub e o upstream `HenryKun55/celosia`. As buscas no YouTube e no Reddit não acharam nada que ligue o jogo ao R36S. `playported.com` e `allrecomps.com` não puderam ser abertos nesta sessão (DNS bloqueado pela rede do ambiente).

## Qual é a origem da alegação e qual artefato concreto existe?

### Takeaway
O único artefato concreto encontrado é o repositório GitHub **`amarqs182/celosia`**. É um fork do projeto **Celosia** (`HenryKun55/celosia`), um port nativo de SotC PS2 (NTSC-U, SCUS-97472) para macOS e PS Vita feito por **recompilação estática** com PS2Recomp. O fork adapta o Celosia exclusivamente ao R36S (aarch64, SDL2 + KMSDRM + GLES 3.2 via libMali). Não é emulação de PS2, não usa o remaster de PS4 e não é streaming. Também não é uma decompilação "matching". Não há release, não há entrada no PortMaster e nenhum vídeo ou post de comunidade foi achado.

### Cited Findings
- Descrição do repositório: "Shadow of the Colossus (PS2) native port for the R36S handheld (aarch64) — static recompilation, SDL2+KMSDRM+GLES. Contributions go to 'main'; 'r36s' is the port's working branch." — [amarqs182/celosia](https://github.com/amarqs182/celosia)
- O README do fork mantém o texto do upstream: "No emulator runs the game's CPU code. The EE code from the boot ELF and the game's three XFF modules is translated into C++ ahead of time and compiled natively." O hardware restante (DMA, IOP, SPU2) foi reimplementado como código do host, e um renderer nativo lê os modelos do jogo diretamente — [README de amarqs182/celosia](https://github.com/amarqs182/celosia)
- Pipeline do port: (1) recompila o ELF de boot; (2) roda o jogo até ele saltar para código ainda não recompilado e captura da RAM os módulos KERNEL.XFF, MANAGER.XFF e GAMECORE.XFF; (3) junta tudo num `SOTC.elf`; (4) compila para C++ os microcódigos de VU1 gravados durante a execução. Nada gerado é commitado — [README](https://github.com/amarqs182/celosia)
- O usuário precisa fornecer o próprio disco: "A disc image of Shadow of the Colossus (USA) (SCUS_974.72), as an .iso or a .zip". "No game data is included." — [README](https://github.com/amarqs182/celosia)
- O fork declara: "This fork targets **exclusively the R36S** (RK3326 aarch64, Mali-G31 MP2 GLES 3.2, SDL2 KMSDRM, Linux 4.4.189)." O mesmo arquivo trata a tabela de macOS/Vita do upstream como contexto histórico — [docs/STATUS.md (fork)](https://github.com/amarqs182/celosia/blob/main/docs/STATUS.md)
- Linha do README sobre o R36S: "✅ builds and runs (32.34 avg / 33.00 med FPS on matched Phase A Shrine walk, 59.8–60.1 FPS in menus/light scenes; GLES 3.2 via ARM libmali over SDL2 KMSDRM)" — [README](https://github.com/amarqs182/celosia)
- Upstream: `HenryKun55/celosia` se descreve como "Native port of Shadow of the Colossus (PS2) to macOS and PS Vita: static recompilation with PS2Recomp, a native Metal/OpenGL renderer, and a decompilation in progress". Tinha cerca de 90 estrelas, 8 forks e 966 commits na `main`, sob GPL-3.0. A página visível não menciona o R36S. Status declarado: gameplay ✅, Mac M4 Pro a 60 fps, Vita a 18–25 fps no Vita3K — [HenryKun55/celosia](https://github.com/HenryKun55/celosia)
- O README do fork orienta não abrir PRs contra o upstream ("Do not push to or open pull requests against HenryKun55/celosia"). Portanto, o trabalho para o R36S é do fork e não do autor original — [README](https://github.com/amarqs182/celosia)
- Autoria, pelo `git log` do clone (profundidade 300, a partir de 2026-10-30): 271 commits de "HenryKun55" (upstream), 104 de "Alisson Marques" (o dono do fork, `amarqs182`), 100 de "Celosia R36S Port", 3 de "Claude" e 1 de "Alisson da Silva Marques". O fork é desenvolvido com agentes de IA: o commit `f14de17` cita "twenty OpenCode agents under .opencode/agents/", e há commits assinados por "Claude" — [amarqs182/celosia](https://github.com/amarqs182/celosia) (verificação local do histórico)
- Releases: a página informa "There aren't any releases here". Não há build para baixar, e o usuário compila a partir do código e do próprio ISO — [releases](https://github.com/amarqs182/celosia/releases)
- PortMaster: não existe entrada. O repositório só traz um launcher "estilo PortMaster" em `ports/r36s/`: `sotc.sh`, `control.txt` com `PLATFORM="RG351MP"` e `gamelist.xml`, para copiar manualmente em `/roms/ports/`. A guia de agentes reserva "Select + Start solely for PortMaster exit" — [ports/r36s](https://github.com/amarqs182/celosia/tree/main/ports/r36s); [AGENTS.md](https://github.com/amarqs182/celosia/blob/main/AGENTS.md)

### Inferences
- A frase "SotC foi portado para o R36S" corresponde a um projeto de código aberto real e verificável, mas ainda em andamento e sem distribuição binária. O trabalho sobre o qual ele se apoia (recompilação, renderer nativo, áudio) é do Celosia upstream. A contribuição própria do fork é o porte para aarch64 Linux, Mali-G31 e KMSDRM, mais as otimizações para Cortex-A35.
- Não achei post, vídeo ou notícia anterior ao repositório. Por isso, a "origem" verificável da alegação é o próprio repositório e seus PRs, de novembro de 2026.

### Gaps
- Não consegui ver a data de criação do fork no GitHub: a API do GitHub foi negada nesta sessão e o clone é raso. O commit `f14de17` (2026-11-10) diz restaurar trabalho de PRs anteriores já mesclados (#2–#17). O PR #7 foi aberto em 2026-11-09, então o fork existe desde, no máximo, cerca de 2026-11-09.
- A data de criação do upstream também não foi confirmada. O commit mais antigo no clone raso é de 2026-10-30, mas o histórico é mais longo (966 commits).

## Linha do tempo e evolução do estado (do "não chega ao gameplay" ao gameplay medido)

### Takeaway
Em cerca de duas semanas, o fork passou de "não chega ao gameplay" (2026-11-09) a gameplay medido no aparelho a cerca de 30–36 fps (meados e fim de novembro de 2026). Todos os números de desempenho são medições do próprio autor, sem vídeo público ou verificação independente.

### Cited Findings
- PR #7, "docs(r36s): documentar o port R36S para contribuintes": aberto em 2026-11-09 e mesclado em 2026-11-10. Diz: "O port ainda nao chega ao gameplay (guest nao executa apos o jalr do loader)." — [PR #7](https://github.com/amarqs182/celosia/pull/7)
- Em 2026-11-10, a `main` do fork sofreu force-push e perdeu 8 PRs mesclados. O commit `f14de17` restaura o toolchain de cross-build `r36s-toolchain.cmake` (PR #6) e outras partes — [amarqs182/celosia](https://github.com/amarqs182/celosia) (mensagem de commit lida localmente)
- `SOTC_OPTIMIZATION_LOG.md` registra sessões de "real user gameplay" no aparelho (Pass 6 → Pass 7, log de "Nov 14 11:27"), com datas de 2026-11-15 a 2026-11-19 — [SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/main/SOTC_OPTIMIZATION_LOG.md)
- O PR #42 traz medições "on real R36S" do cenário Shrine. O thread do jogo ficou em 36,15–36,42 FPS, com pico de 38,90, e uma busca resumiu "59.71 fps mean" em Shrine com limite de fps. Os números de tela, porém, ficam limitados por draw + present — [PR #42](https://github.com/amarqs182/celosia/pull/42); [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Tabela de progressão em `docs/r36s_port.md`, sempre na mesma rota determinística: Pass 8A a 25,6 fps de média; Phase 6 a 29,87; Phase 6B a 31,65; Phase A a 32,34 de média e 33,00 de mediana (p50 30,3 ms, p99 40,8 ms). O texto conclui que o thread de jogo na CPU continua sendo "the sole frame limiter on Cortex-A35" — [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Datas dos launchers: PR #60, "Add R36S port launcher scripts and control metadata", em 2026-11-20. PR #62, que endurece o launcher, em 2026-11-21. PR #97, com perfis Max Performance / Medium / Original, em 2026-11-24 — [PR #60](https://github.com/amarqs182/celosia/pull/60) (datas pelo `git log` local)
- Commits mais recentes (25 a 27/11/2026): o PR #118, "Lock 30fps game pacing...", trava o ritmo do jogo em 30 fps. Os PRs #109–#114 adicionam culling por oclusão Hi-Z na CPU, e o #120, "Fix Mali renderer visual parity..." — [amarqs182/celosia](https://github.com/amarqs182/celosia) (`git log` local)
- Algumas mudanças recentes ficam pendentes de validação no aparelho ("device-side validation is left to the maintainer"), segundo um resumo de busca dos PRs — [PR #44](https://github.com/amarqs182/celosia/pull/44)

### Inferences
- A meta atual parece ser 30 fps estáveis (PR #118), e não os "60 FPS estáveis" que `r36s_port.md` declara como alvo. Fora de menus e cenas leves, 60 fps não foram atingidos no gameplay.
- Como o jogo original de PS2 roda a cerca de 30 fps ou menos, ~30 fps no R36S equivale ao ritmo original. Isso é uma inferência: não verifiquei aqui o frame rate do original.

### Gaps
- Não há vídeo, captura ou build público que permita a terceiros reproduzir os números. Todas as medições vêm de logs do autor citados nos docs e PRs.
- Não verifiquei se o jogo pode ser terminado no port (todos os colossos) nem se há crashes conhecidos em fases avançadas. Os docs só mencionam a Shrine, o pátio, a intro e uma cutscene corrigida (o travamento do pássaro após o Wander).

## Que aparelho e firmware aparecem? É mesmo um RK3326?

### Takeaway
Sim. Os docs trazem dados de hardware sondados no aparelho que batem com o RK3326 do R36S: DTS `rockchip,rk3326-odroidgo3-linux`, 4× Cortex-A35 de até 1512 MHz, Mali-G31 Bifrost (GPU ID 0x7093), libMali r13p0, cerca de 895 MiB de RAM e tela de 640×480. O firmware é um userland **Debian 13 "trixie"** sobre o kernel **4.4.189**, ou seja, ao que tudo indica um firmware da família ArkOS/dArkOS, embora o repositório não diga o nome. O controle é identificado no código como "R36S / ArkOS built-in pad".

### Cited Findings
- Alvo: "SoC: Rockchip RK3326 (rockchip,rk3326-odroidgo3-linux)"; "CPU: quad-core Cortex-A35 (fp asimd, no FMA/fp16/SVE; 1008–1512 MHz)"; "GPU: Mali-G31 (Bifrost r0p0, GPU ID 0x7093...) via ARM libmali (OpenGL ES 3.2 v1.r13p0-01rel0, -gbm variant)"; "Memory: 895 MiB"; "Display: 640×480 KMSDRM scanout at 60 Hz" — [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- "Platform: aarch64 Debian 13.7 (trixie) Linux, glibc 2.41, systemd 257, kernel 4.4.189 #1 SMP". Valores "Probed directly on the R36S" — [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md); [AGENTS.md](https://github.com/amarqs182/celosia/blob/main/AGENTS.md)
- Limitações medidas no aparelho em 2026-11-15: Vulkan da libMali sem WSI de exibição; Panfrost indisponível no kernel 4.4; `eglInitialize` em GBM/renderD128 causa kernel Oops; faltam as syscalls `io_uring`, `clone3` e `statx` (ENOSYS) — [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)
- Código: `// R36S / ArkOS built-in pad (Vendor=484b Product=1100).` — `PS2Recomp/ps2xRuntime/platform/linux/raylib_linux.cpp` em [amarqs182/celosia](https://github.com/amarqs182/celosia)
- O launcher assume `/roms/ports/`, reinicia o EmulationStation via `/usr/bin/emulationstation/emulationstation.sh` e para `batt_led.service`. A instalação manual vai para `/roms/ports/sotc_data/` — [ports/r36s/sotc.sh](https://github.com/amarqs182/celosia/blob/main/ports/r36s/sotc.sh)
- `control.txt` declara `PLATFORM="RG351MP"`, um identificador de plataforma RK3326 do ecossistema ArkOS/PortMaster — [ports/r36s/control.txt](https://github.com/amarqs182/celosia/blob/main/ports/r36s/control.txt)
- O toolchain de cross-compilação usa uma sysroot Debian trixie arm64 "matches the device exactly: Debian GNU/Linux 13 'trixie', glibc 2.41-12+deb13u4" — [PS2Recomp/cmake/r36s-toolchain.cmake](https://github.com/amarqs182/celosia/blob/main/PS2Recomp/cmake/r36s-toolchain.cmake)

### Inferences
- A combinação de Debian trixie, kernel BSP 4.4.189, DTS odroidgo3, `/roms/ports` e EmulationStation em `/usr/bin/emulationstation` aponta para o **dArkOS** (o sucessor do ArkOS baseado em Debian) ou para um ArkOS atualizado. O ArkOS clássico usa Ubuntu, então o fork provavelmente não roda nele sem ajustes. AmberELEC e ROCKNIX não aparecem como testados, e o launcher depende do caminho do EmulationStation do ArkOS. Nada disso foi confirmado por fonte explícita.
- Não há sinal de confusão com R36H, R36 Max ou RG35XX: o DTS e o GPU ID citados são de RK3326. Os dados vêm do próprio autor, porém, e não de terceiros.

### Gaps
- Nenhum documento do repositório cita "dArkOS" pelo nome, e a versão exata do firmware não é informada.
- Compatibilidade com AmberELEC, ROCKNIX e clones de R36S com outro painel ou outra revisão: não documentada.

## Existem decompilações ou recompilações de SotC PS2, e em que estado?

### Takeaway
Sim, e várias. O Celosia é a recompilação estática "funcional" mais avançada e gerou vários forks: R36S, Switch, PC/Android e outros. Em paralelo, há decompilações "matching" separadas em andamento, nenhuma completa.

### Cited Findings
- Celosia (upstream): recompilação via PS2Recomp, mais uma "decompilação funcional, não matching". As funções reescritas em C++ são provadas iguais às recompiladas por difftest. Em 2026-10-30 (round 35), 5,4% do código MIPS estava reescrito, mas 48% das chamadas e 43% das instruções do thread de jogo do Vita já rodavam em código nativo — [README](https://github.com/amarqs182/celosia); [HenryKun55/celosia docs/decomp.md](https://github.com/HenryKun55/celosia/blob/main/docs/decomp.md)
- PS2Recomp, o recompilador estático usado — [ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp)
- Outros forks e derivados do Celosia:
  - `GenissonEmilio/ShadowOfTheColossusPC`: port Linux/Windows x86_64 e Android ARM64, NTSC-U e PAL, com 62 estrelas e releases v0.2.7 a v0.9.5 mais um APK. Não menciona R36S nem PortMaster — [GenissonEmilio/ShadowOfTheColossusPC](https://github.com/GenissonEmilio/ShadowOfTheColossusPC)
  - `lemasterxar5/celosia-nx`: Nintendo Switch, com deko3d — [celosia-nx](https://github.com/lemasterxar5/celosia-nx)
  - `felipmz/ShadowOfTheColossusPC-fork`, `braito4/celosia` e `otavix010/shadow-of-the-colossus-pc` (PAL, em início) — [felipmz](https://github.com/felipmz/ShadowOfTheColossusPC-fork); [braito4](https://github.com/braito4/celosia); [tópico GitHub](https://github.com/topics/shadow-of-the-colossus)
- Decompilações matching (independentes do Celosia), com números de resumos de busca não reconferidos:
  - `TheRealestNwah/sotc-decomp`: NTSC-U, com meta de um port nativo para PC, cerca de 89% dos módulos do jogo segundo a barra de progresso — [sotc-decomp](https://github.com/TheRealestNwah/sotc-decomp)
  - `mattheuspaivadini/ShadowDecomp`: 763 de 1.042 funções do executável principal (73,2%) — [ShadowDecomp](https://github.com/mattheuspaivadini/ShadowDecomp)
  - `NathanFahey23/Shadow-of-the-Colossus-Decomp`: arquivado em 2026-10-21 — [repo](https://github.com/NathanFahey23/Shadow-of-the-Colossus-Decomp)
  - `Fantaskink/SOTC` e `lucascionis/SOTC`: versão preview SCPS_150.97, cerca de 12,87% segundo um rastreador — [Fantaskink/SOTC](https://github.com/Fantaskink/SOTC); [recompsoftware](https://recompsoftware.com/p/sotc-decomp/)
- Nenhum projeto encontrado parte do remaster de PS4 (Bluepoint, 2018). Todos partem do binário de PS2 — [tópico GitHub](https://github.com/topics/shadow-of-the-colossus)

### Inferences
- O port para o R36S só é viável porque herda do Celosia uma recompilação estática completa e um renderer nativo. Nenhuma decompilação matching está completa o bastante para sustentar um port sozinha.

### Gaps
- As percentagens das decompilações matching vêm de resumos de busca e não foram conferidas nos repositórios (a API do GitHub foi negada nesta sessão).

## O que a comunidade diz (autenticidade e desempenho)? Há vídeos ou posts?

### Takeaway
Não achei nenhuma discussão de comunidade, vídeo do YouTube, thread do Reddit ou matéria de site de portáteis sobre SotC no R36S. Buscas de "R36S Shadow of the Colossus" no Reddit, YouTube e na web geral só devolveram o repositório GitHub e seus PRs. O consenso público sobre o RK3326 é que emular PS2 nele não é prático, o que reforça que o fork é um port nativo e não emulação.

### Cited Findings
- Busca no YouTube por "R36S" + PS2 + "Shadow of the Colossus": só retornou gameplays de PS2 e PCSX2 e um vídeo em português, "Shadow of the Colossus de PS2 agora roda NATIVAMENTE no PC!", sobre o port de PC/Android (data e autor não verificados) — [vídeo](https://www.youtube.com/watch?v=S--N6KXXoIk)
- Busca no Reddit (r/SBCGaming, r/R36S): nenhum resultado relevante, só anúncios do eBay — (sem URL útil; ausência de evidência)
- Contexto de hardware: um artigo da CNX Software de 2020 sobre outro console RK3326 diz que emular PS2 "may still not be practical... one can only hope for a dozen or so fps". Outras fontes dizem que o R36S não emula PS2 — [CNX Software](https://www.cnx-software.com/2020/12/26/kt-r1-high-performance-amlogic-s922x-portable-gaming-console/amp/); [handhelds.wiki R36S](https://www.handhelds.wiki/R36S); [Retro Catalog R36S](https://retrocatalog.com/retro-handhelds/r36s)
- Os docs do fork registram que, no R36S, o Vulkan da libMali não tem swapchain e o Panfrost não funciona no kernel 4.4. Isso também exclui caminhos de emulação de PS2 baseados em Vulkan, como NetherSX2 e ARMSX2 — [docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/main/docs/r36s_port.md)

### Inferences
- Como não há vídeos ou posts, não há como apontar footage falsa ou de outro aparelho: a alegação aparentemente ainda não circulou fora do GitHub. Se surgir um vídeo de "SotC no R36S", o artefato a conferir é este fork, ou um port reempacotado a partir dele.
- O ceticismo continua necessário sobre os números de fps. São medições internas do autor, cujos docs e PRs são escritos em grande parte com ajuda de agentes de IA (OpenCode, Claude), e algumas mudanças estão marcadas como não validadas no aparelho.

### Gaps
- Sem acesso ao Discord do PortMaster nem a fóruns russos, brasileiros ou chineses: as buscas não retornaram nada desses lugares, e `playported.com` e `allrecomps.com` não puderam ser abertos (DNS bloqueado nesta sessão). O `playported.com` lista "Shadow of the Colossus PC Port (celosia)", mas não verifiquei se cita o R36S.
- Não há comentários de terceiros sobre a autenticidade do port no R36S.
