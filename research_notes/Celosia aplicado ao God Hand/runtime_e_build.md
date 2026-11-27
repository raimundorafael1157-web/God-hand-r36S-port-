# Celosia (SotC R36S) → God Hand: técnicas de runtime e build que dá para copiar e testar (estado em 2026-11-27)

Escopo: o que o port Celosia de Shadow of the Colossus para o R36S (amarqs182/celosia, GPL-3.0) fez no
PS2Recomp/ps2xRuntime e no build, comparado com a nossa base (godhand-recomp @14e956a da LasVola + patches
0001-0010). Fonte principal: o clone raso do Celosia no commit `39ab080c85ee6ee35d3a54b142dc79132edd9dfb`
(2026-11-27 11:07 -0300, "Fix Mali renderer visual parity... (#120)"). Os links `C:` abaixo apontam para os
arquivos nesse commit, em `https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/<caminho>`.
Os arquivos locais do nosso lado são citados pelo caminho absoluto. Não usei a API do GitHub (o `gh` não respondeu
JSON), então as páginas de PR não foram lidas. Os números de PR citados vêm do próprio log do Celosia.

Atalhos de link:
- CMake do runtime do Celosia: [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt)
- CMake raiz do PS2Recomp do Celosia: [C:PS2Recomp/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/CMakeLists.txt)
- Log de otimização: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md)
- Notas do port: [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md)
- Memória: [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp)
- Macros: [C:ps2_runtime_macros.h](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/include/ps2_runtime_macros.h)
- Toolchain: [C:cmake/r36s-toolchain.cmake](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/cmake/r36s-toolchain.cmake)
- Launcher: [C:ports/r36s/sotc.sh](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/ports/r36s/sotc.sh)
- Gerador da ordem de código quente: [C:tools/linux_hot_order.py](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/tools/linux_hot_order.py)
- Dump do PGO e log de frame: [C:game/overrides/sotc_native.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/sotc_native.cpp)
- Fixação de threads: [C:native_renderer.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/native/native_renderer.cpp)

## 0. Ancestralidade comum: as duas bases descendem de ran-j/PS2Recomp, por ramos que divergiram muito

### Takeaway
Os dois runtimes vêm de ran-j/PS2Recomp, mas por forks diferentes. O nosso vem de `sotc-port` @ac9efa0
(2026-10-01) + as mudanças do port do Urban Reign. O do Celosia é uma git subtree de ran-j com as "rounds" do
HenryKun55. As camadas que importam aqui (macros de acesso à memória, CMake, alocação de RAM) já não têm código em
comum. Cada técnica é um port manual, não um cherry-pick.

### Cited Findings
- O nosso `runtime/` é o PS2Recomp de "TaylorNAlbarnaz/PS2Recomp, branch `sotc-port` (o fork usado pelo port de SotC
  LightVelox/sotc-vibe-pc, a partir de ran-j/PS2Recomp), commit `ac9efa0` (2026-10-01)", mais as mudanças do Urban
  Reign (GS threading, MTVU timing, VU1 AOT, IOP/SIF...) — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/README.md`.
  O CMakeLists raiz chama o mesmo fork de "LightVelox/PS2Recomp sotc-port ac9efa0": os dois documentos dão donos
  diferentes para o fork — `/home/user/God-hand-r36S-port-/source/godhand-recomp/CMakeLists.txt` (linhas 11-12).
- O Celosia mantém `PS2Recomp/` como "git subtree of ran-j/PS2Recomp, with our changes", e amarqs182/celosia é fork
  de HenryKun55/celosia (upstream só para leitura) — [C:README.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/README.md) (linhas 15, 666), [C:AGENTS.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/AGENTS.md).
- Exemplo de divergência: no Celosia os acessos passam por `PS2_GUEST_READ/WRITE`, com `ps2_guest_mem::load/store`,
  `readWatch` e `trace` — [C:ps2_runtime_macros.h](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/include/ps2_runtime_macros.h) (linhas ~660-830). No nosso, `READ*/WRITE*` testam
  `PS2Runtime::isSpecialAddress` e caem em `Ps2FastRead*/Ps2FastWrite*` com máscara `PS2_RAM_MASK` e tratamento de
  acesso que cruza o fim da RAM — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/include/ps2_runtime_macros.h` (linhas ~430-600).

### Inferences
- O ancestral comum fica antes das "rounds" do HenryKun55 e antes de `ac9efa0`. Nada do Celosia aplica com `git am`
  na nossa árvore. Cada item abaixo precisa ser reescrito sobre o nosso código.

### Gaps
- O commit ancestral exato não foi identificado: o clone do Celosia é raso (1 commit) e não fiz `--unshallow` nem
  clonei ran-j ou TaylorNAlbarnaz.

## 1. PS2X_LINUX_RELEASE (desliga READ_WATCH, WRITE_TRACE, RUNTIME_LOGS, AGRESSIVE_LOGS, IOP_RPC_TRACE) e PS2X_ARM64_MTUNE

### Takeaway
No Celosia o `PS2X_LINUX_RELEASE=ON` desliga toda a instrumentação em tempo de compilação e define `SOTC_RELEASE=1`.
Sem PGO novo o ganho medido ficou no ruído; com PGO recoletado ficou em +1,96 FPS. Para nós sobra pouco:
1. O WRITE_TRACE já é no-op na nossa base, e o READ_WATCH não existe.
2. `PS2X_ENABLE_RUNTIME_LOGS` continua ligado por padrão. Desligá-lo é uma linha de CMake e vale testar.
3. O `-mtune` já está coberto pelo nosso `-mcpu=cortex_a35`.

### Cited Findings
- **O que o Celosia faz.**
  - Cria `option(PS2X_LINUX_RELEASE ...)`. Cada definição (`PS2_RUNTIME_LOGS`, `AGRESSIVE_LOGS` +
    `PS2_FUNCTION_LOG_TRACKER`, `PS2X_ENABLE_IOP_RPC_TRACE`, `PS2X_GUEST_READ_WATCH`, `PS2X_GUEST_WRITE_TRACE`) só é
    ligada `AND NOT PS2X_LINUX_RELEASE`, e as três últimas recebem `=0` explícito.
  - Com RELEASE, `-DSOTC_RELEASE=1` vai para `ps2_runtime`, `ps2_iop` e `overrides/*.cpp` — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 13-20, 346-386, 614-622).
- **O que READ_WATCH e WRITE_TRACE custam no Celosia.**
  - `readWatch<T>` testa `g_ps2ReadWatchMap != nullptr` em cada leitura de RAM.
  - `trace<T>` testa `g_ps2WatchWriteEnabled` em cada escrita.
  - Com o macro em 0, os dois viram `(void)` — [C:ps2_runtime_macros.h](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/include/ps2_runtime_macros.h) (linhas ~677-718).
- **PS2X_ARM64_MTUNE.** Padrão `cortex-a35`. O CMake raiz adiciona `-march=armv8-a+fp+simd` (+crypto/+crc opcionais)
  e `-mtune=${PS2X_ARM64_MTUNE}` — [C:PS2Recomp/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/CMakeLists.txt) (linhas 12-14, 55-69). O bug que motivou isso: o
  `-march` adicionado depois de `CMAKE_CXX_FLAGS` sobrescrevia `+crypto+crc`, e não havia `-mtune` — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (Phase A, linha 233).
- **Ganho medido no R36S.**
  - Phase 2A: o `hooks` caiu de 1,10 ms para 0,00 ms por frame.
  - Pass 8A (Release, sem PGO): 25,60 FPS de mediana na rota determinística `gp 4458..5298`.
  - Fonte das duas linhas: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 41-47, 64-70, 147).
- **Phase A (2026-11-16), rota casada `f=4500..5900`, governor `performance`.**

  | Configuração | FPS média / mediana | Binário |
  | --- | --- | --- |
  | A0 (watch/trace/logs ligados) | 30,38 / 31,06 | 78,0 MB |
  | A1 (watch e trace em 0, PGO velho) | 30,20 / 30,86 | 64,3 MB |
  | A3 (+ logs OFF, +crypto+crc, -mtune, PGO velho) | 30,24 / 30,96 | — |
  | A3_PGO (A3 + PGO recoletado) | 32,34 / 33,00 | 63,4 MB |

  O A3_PGO deu +1,96 FPS de média (+6,45%), −2,00 ms na game thread e −18,7% de binário
  — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 236-247).
- **Por que o A1/A2 não ganhou nada.** O PGO velho, coletado com READ_WATCH=1, deixou de casar com o novo CFG
  (`-Wcoverage-mismatch`; exemplo: `decomp_ai106::leaderNative`, 141 → 91 arcos). O GCC descartou o perfil nas
  funções alteradas — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 235).
- **Tabela da Phase C, modo sem sincronismo (`SOTC_NO_SYNC=1`).** `RELEASE=ON` levou a game thread de
  116,05 ms para 106,23 ms de média (−8,5%) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 310-313).
- **Nossa base.**
  - Não existe `PS2X_GUEST_READ_WATCH` (grep sem resultado).
  - `ps2TraceGuestWrite()` é chamado em cada WRITE do caminho rápido, mas o corpo é só `(void)` dos argumentos
    ("TODO we dont need this anymore") — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/include/ps2_runtime.h` (linhas 241-256).
- **Logs na nossa base.**
  - O CMake raiz força `PS2X_ENABLE_AGRESSIVE_LOGS OFF`, mas não toca em `PS2X_ENABLE_RUNTIME_LOGS` (padrão ON) nem
    em `PS2X_ENABLE_IOP_RPC_TRACE` (padrão ON) — `/home/user/God-hand-r36S-port-/source/godhand-recomp/CMakeLists.txt` (linha 31); `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/CMakeLists.txt` (linhas 14-16, 446-465).
  - `RUNTIME_LOG(x)` monta um `std::ostringstream`, escreve em `std::cout` e acumula com mutex
    (`append_runtime_log_text`). Há 55 usos em `runtime/ps2xRuntime/src` — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/include/ps2_log.h` (linhas 119-138).
- **Nosso `-mcpu`.** O wrapper de compilação já passa `-target aarch64-linux-gnu.2.30 -mcpu=cortex_a35`, o que fixa
  `-march` e `-mtune` juntos — `/home/user/God-hand-r36S-port-/source/godhand-recomp/platform/zig/zig-c++`.

### Inferences
- **Esforço.** Muito baixo: `-DPS2X_ENABLE_RUNTIME_LOGS=OFF -DPS2X_ENABLE_IOP_RPC_TRACE=OFF` no `build_rk3326.sh`,
  ou `set(... OFF CACHE BOOL "" FORCE)` no CMakeLists raiz, como já é feito com AGRESSIVE_LOGS. Não há código de jogo
  envolvido.
- **Ganho esperado.** Pequeno. Os `RUNTIME_LOG` ficam em caminhos de syscall/IOP, não no acesso à memória. No Celosia
  a parte "logs OFF" isolada ficou no ruído (A1 → A3).
- **Risco.**
  1. Perder mensagens de diagnóstico que o `hang_dump` e os logs de bench usam. Verificar se `[bench-*]` depende de
     `RUNTIME_LOG` (grep antes).
  2. Se um dia usarmos PGO, cada mudança de flag exige recoletar o perfil (lição da Phase A).
- **Validação.** Gravação do GS idêntica byte a byte (não deve mudar nada). Depois `run_ab.sh` A=atual, B=logs OFF,
  e `bench_compare.py`.
- **`-mtune` / `+crypto+crc`.** Nada a portar. O nosso `-mcpu=cortex_a35` já faz o tune. Compiladores raramente geram
  CRC/AES sem intrínsecos, então habilitar extensões não deve mudar desempenho. O r36s_port.md lista só `fp asimd`
  como flags da CPU — [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md) — e isso contrasta com o Celosia rodar `+crypto+crc` no aparelho.
  Conferir `/proc/cpuinfo` (`collect_hw_info.sh`) antes de qualquer `+crypto`.

### Gaps
- Não há medição isolada do `-mtune=cortex-a35` no Celosia: ele foi medido junto com logs OFF no A3.
- Não medi quantas vezes os nossos `RUNTIME_LOG` disparam por segundo numa luta.

## 2. PGO no aparelho (PS2X_LINUX_PGO=gen/<dir>, __gcov_dump, -fprofile-prefix-path, coverage mismatch)

### Takeaway
O PGO foi o maior ganho de build do Celosia: +7% a +8% de FPS de mediana a cada recoleta. Mas ele depende do GCC
16.2 e do libgcov (`.gcda`). Com o nosso zig/clang, o fluxo seria o PGO de IR do LLVM (`.profraw` →
`llvm-profdata merge` → `.profdata`). Não confirmei se o zig 0.17.1 liga o runtime de profile nem se ele traz
`llvm-profdata`. É o item de maior incerteza de toolchain.

### Cited Findings
- **Compilador do Celosia.** O toolchain cruzado é GCC 16.2.0 + binutils 2.47 (pacotes Arch aarch64-linux-gnu),
  sysroot Debian trixie com glibc 2.41, compilando via `aarch64-linux-gnu-g++` — [C:cmake/r36s-toolchain.cmake](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/cmake/r36s-toolchain.cmake).
- **Modo `gen`.**
  - Flags: `-fprofile-generate=/tmp/sotc_pgo/ -fprofile-prefix-path=${CMAKE_BINARY_DIR} -fprofile-update=single
    -fno-profile-values -fno-profile-reorder-functions` em `ps2_runtime` e nos overrides (não no código recompilado
    frio).
  - Link: `-fprofile-generate=/tmp/sotc_pgo/ -Wl,-u,__gcov_dump`.
  - Fonte: [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 632-645).
- **Modo `use`.** Flags: `-fprofile-use=<dir> -fprofile-prefix-path=... -fprofile-partial-training
  -fprofile-correction -fno-profile-values -fno-profile-reorder-functions -fno-vpt -Wno-missing-profile
  -Wno-coverage-mismatch` — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 646-661).
- **Gatilho do dump.** No Linux, `gcovDump(frame)` chama o `__gcov_dump()` fraco uma única vez, quando
  `frame >= SOTC_GCOV_DUMP_FRAME`, e loga `[gcov] frame N: __gcov_dump() complete`. Assim o perfil não inclui o
  desligamento — [C:game/overrides/sotc_native.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/sotc_native.cpp) (linhas ~5446-5505).
- **Por que `-u,__gcov_dump`.** Sem ele, o `__gcov_dump` forte do `libgcov.a` não era puxado no link. Com ele vieram
  213/213 `.gcda` numa sessão real — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 82).
- **Por que `-fprofile-prefix-path`.** Com host Windows, o caminho `D:\...` embutido + `GCOV_PREFIX` falhava no
  aparelho ("Cannot create directory", "free(): invalid pointer"). A solução foi um diretório absoluto Linux
  (`/tmp/sotc_pgo/`) e a remoção do prefixo do build — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 134).
- **Procedimento.** Build `gen` → rodar no R36S com `SOTC_GCOV_DUMP_FRAME=4200` (ou 4900/5050) passando por intro e
  mundo aberto → copiar `/tmp/sotc_pgo/*.gcda` para `build/.../pgo_xxx` → reconfigurar com
  `-DPS2X_LINUX_PGO=<dir>`. Resultado: 0 avisos de missing/mismatch em 213-214 unidades — [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md) ("Reproducible R36S Release + PGO Cross-Build").
- **Ganhos medidos** (rota de 125 s, 15 buckets = 900 frames) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 147-153, 236-247):

  | Passo | FPS de mediana | Game thread (mediana) |
  | --- | --- | --- |
  | Pass 8A → 8B (PGO v1) | 25,60 → 27,00 (+5,5%) | 38,90 → 36,90 ms |
  | Pass 9 → Pass 9 + PGO | 27,30 → 29,40 (+7,7%) | 36,50 → 33,80 ms |
  | Phase A, PGO recoletado | 30,96 → 33,00 | — |

- **Cuidados com o perfil.**
  - Um perfil coletado numa sessão "torta" (`pgo_pass96`) piorou: 27,60 FPS contra 30,10 do `pgo_merged`. Por isso
    eles fundem perfis (`pgo_merged` = 211 TUs do pass95 + 3 do pass96) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 212-217).
  - Perfil velho após mudança de código vira coverage mismatch e cancela o ganho — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 235).
- **Nossa base.** O compilador é zig (`python3 -m ziglang c++`), ou seja clang/LLVM, com libc++ estático e alvo
  glibc 2.30 — `/home/user/God-hand-r36S-port-/source/godhand-recomp/cmake/aarch64-zig-glibc230.cmake`; `/home/user/God-hand-r36S-port-/scripts/build_rk3326.sh` (passo `build`, `ziglang==0.17.1`).
- O clang aceita a sintaxe GCC `-fprofile-generate`, mas grava `default.profraw` (formato LLVM) e lê o perfil com
  `-fprofile-use` — [LLVM test gcc-flag-compatibility.c](https://src.rcs.uwaterloo.ca/xref/llvm-project/clang/test/Profile/gcc-flag-compatibility.c).
- Sem o runtime de profile ligado, o link falha com símbolos `__llvm_profile_*` indefinidos (caso relatado no
  LDC/Debian) — [digitalmars.D - Profiling with LDC](https://digitalmars.com/d/archives/digitalmars/D/Profiling_with_LDC_314269.html). O GCC exige `-fprofile-generate` também no
  link — [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc//Instrumentation-Options.html).
- Não achei relatos específicos de `zig cc -fprofile-generate` funcionando ou falhando. A busca trouxe só um problema
  de pprof/cgo com zig cc, que não é o mesmo assunto — [golang issue 59466](https://golang.org/issue/59466).

### Inferences
- **O que muda no fluxo com zig.** O `.gcda` do Celosia não serve para nós; o equivalente LLVM seria:
  1. Gerar com `-fprofile-generate=/tmp/gh_pgo` (ou `-fprofile-instr-generate`).
  2. No aparelho, disparar `__llvm_profile_write_file()` (o análogo do `__gcov_dump`) num field fixo, via variável
     de ambiente, como o `SOTC_GCOV_DUMP_FRAME`.
  3. No PC, `llvm-profdata merge -o gh.profdata *.profraw` com um `llvm-profdata` da mesma versão do LLVM do zig 0.17.1.
  4. Recompilar com `-fprofile-use=gh.profdata`.
- **Prefixo de caminho.** O PGO do LLVM indexa por nome de função + hash do CFG. Nomes de funções `static` levam o
  caminho do arquivo, então convém compilar sempre no mesmo diretório (`build-rk3326`) para não perder funções
  locais. Os 10.603 arquivos gerados usam nomes globais (`ADXAMP_Destroy_0x33c038` etc.), o que ajuda.
- **Bloqueios a verificar.**
  1. O zig 0.17.1 tem `libclang_rt.profile` para aarch64-linux-gnu? (provável gap; precisa de um teste com hello world).
  2. O pacote pip `ziglang` não traz `llvm-profdata`; seria preciso um LLVM de mesma versão no host.
- **Alternativa.** Compilar só o build PGO com GCC cruzado, como o Celosia faz. Problema: nosso alvo é glibc 2.30 com
  libc++ estático do zig. Com GCC seria `-static-libstdc++` e um sysroot glibc 2.30. É troca de toolchain inteira,
  com risco de diferenças de geração de código que tirariam o valor do A/B contra o binário zig.
- **Escopo.** O Celosia aplica PGO só no runtime + overrides, não no código recompilado frio (−Os). Para nós
  começaria por `ps2_runtime` + `ps2_iop` (+ src/ do port). Instrumentar os 10.603 arquivos gerados deixaria o build
  `gen` lento demais no A35.
- **Validação.**
  - O binário `gen` instrumentado é bem mais lento (o build gen do Celosia tinha 99 MB). A sessão de coleta tem de
    ser a mesma rota do benchmark: para nós, `stage1.state` + PAD_SCRIPT vazio + N fields.
  - Depois: gravação do GS idêntica (PGO não muda semântica, só layout e inlining), `run_ab.sh` A=sem PGO,
    B=com PGO, e recoletar a cada mudança de código.

### Gaps
- Não confirmei o suporte de PGO no zig 0.17.1 (runtime de profile, `-fprofile-generate` no link, versão do LLVM
  embutido). É preciso um teste local.
- Não sei se o kernel 4.4/ArkOS do R36S tem `perf` com amostragem por branch, o que permitiria AutoFDO/propeller
  como alternativa ao PGO instrumentado.

## 3. Ordenação de funções quentes (linux_hot_order.ld) e -ffunction-sections

### Takeaway
O Celosia compila tudo com `-ffunction-sections` e passa ao GNU ld um `--section-ordering-file` gerado por script, a
partir de `nm` + perfil do guest no R36S: cerca de 8.900 símbolos, 35 MB, em zonas A/B/C. Para nós, o lld (usado pelo
zig) tem o equivalente `--symbol-ordering-file`. O que custa é gerar a lista a partir do nosso `PS2X_PROF`. Não há
ganho isolado medido pelo Celosia.

### Cited Findings
- **CMake.** A opção `PS2X_LINUX_FUNCTION_SECTIONS` (ON) aplica `-ffunction-sections` em `ps2_runtime`,
  `ps2EntryRunner` e `ps2_iop`, e linka com `-Wl,--section-ordering-file=<linux_hot_order.ld>` (padrão
  `game/overrides/linux_hot_order.ld`) — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 674-697).
- **Formato.** Um script `.text : { *(.text.<mangled> .text.hot.<mangled> .text.startup.<mangled>) ... }`. Os
  primeiros símbolos são o renderer, `sotc_native::*Draw`, o `SoundMixer` do IOP e `PS2Memory::walkVif1Chain` — [C:game/overrides/linux_hot_order.ld](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/linux_hot_order.ld).
- **Gerador.** `tools/linux_hot_order.py --elf <ps2EntryRunner> --nm ... --vita-order vita_hot_order.ld --lean-hot
  game/lean_hot.txt --guest-prof <arquivo PS2X_PROFILE> --zone-a-leant 400 -o ...`. Monta três zonas:
  - Zona A: símbolos de runtime quente por padrão de nome (`Renderer`, `SoundMixer`, `PS2Memory`, `EEScheduler`,
    `decomp_`, `vu1rec_`...) e as 400 funções lean mais quentes.
  - Zona B: overrides restantes e `sub_*` mornos.
  - Zona C: o lifter `lift_d1`.
  - O resto fica na cauda fria. O script normaliza sufixos de clones do GCC (`.constprop`, `.isra`, `.part`,
    `.cold`, `.lto_priv`).
  - Fonte: [C:tools/linux_hot_order.py](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/tools/linux_hot_order.py) (linhas 1-60, 136-221).
- **Tamanhos.**
  - Pass 4: 8.919 símbolos, 34,72 MB (7,59 MB na zona A, 20,49 MB na B, 6,64 MB na C).
  - Pass 9.5: 8.883 símbolos, 35,04 MB, gerados de `gp_b2_game.prof` (perfil do R36S).
  - Fonte: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 136, 175).
- **Linker.** `--section-ordering-file` é uma opção do GNU ld; o Celosia usa binutils 2.47 — [C:cmake/r36s-toolchain.cmake](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/cmake/r36s-toolchain.cmake).
- **Contexto de cache.** Num teste, inlining extra estourou os 32 KB de L1I do A35 e custou −0,98 FPS mesmo com PGO
  novo — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 289-293). Na mesma fase, menos estado por slot (pf141=0) rendeu
  +1,15/+1,28 FPS por efeito de cache L2 (256 KB compartilhado) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 269, 290).

### Inferences
- **Como portar.**
  1. Somar `-ffunction-sections` (o clang aceita) aos alvos `ps2_runtime`, `ps2_iop`, `godhand` e ao código gerado.
  2. Gerar uma lista de símbolos mangled em ordem: um por linha, que é o formato do `--symbol-ordering-file` do lld,
     mais simples que o script ld.
  3. Passar `-Wl,--symbol-ordering-file=...`. É preciso confirmar que o zig repassa essa opção ao lld.
- **Fonte da lista.** O nosso `PS2X_PROF=1` / `[bench-sections]` do A35 para as seções do runtime. Para as funções
  gamegen, um amostrador por PC (o Celosia usa `PS2X_PROFILE` + `tools/guest_profile.py`). Não há nada específico
  de SotC no método; a lista é do jogo.
- **Ganho esperado.** Desconhecido. O binário stripped tem ~90 MB (`build_rk3326.sh`), contra 32 KB de L1I e 256 KB
  de L2, então a localidade de I-cache e iTLB pode importar. Medir também com `perf stat` de iTLB/I-cache, se houver.
- **Risco.** Baixo: não muda semântica. A gravação do GS tem de ficar idêntica, e só o tempo muda.

### Gaps
- O Celosia não publica A/B isolado do `linux_hot_order.ld`: ele já estava no baseline do Pass 7.
- Não confirmei que o lld do zig 0.17.1 aceita `--symbol-ordering-file` via `zig c++ -Wl,...`. A ausência de
  `--section-ordering-file` no lld é inferência minha, não verificada.

## 4. PS2X_LINUX_SELECTIVE_LTO e a separação lean_part

### Takeaway
O LTO seletivo do Celosia é `-flto=auto -ffat-lto-objects -fno-plt` só no runtime, no IOP e nos overrides nativos,
mantendo o código recompilado (`lean_part*`) fora do LTO. Ganho medido: ~2% na game thread sem sincronismo.
Para nós a ideia se traduz em ThinLTO (`-flto=thin`) em `ps2_runtime` + `ps2_iop` + `src/`, nunca nos 10.603
arquivos gerados. O "lean_part" é específico do pipeline de geração do Celosia e não se aplica diretamente.

### Cited Findings
- **CMake.** Com `PS2X_LINUX_SELECTIVE_LTO`, `-flto=auto -ffat-lto-objects -fno-plt` vão para `ps2_runtime`,
  `ps2_iop` e `/overrides/*.cpp`, e o link de `ps2EntryRunner` recebe `-flto=auto -fno-plt`. Os `lean_part[0-9]+.cpp`
  ficam fora (recebem só `${PS2X_LINUX_GAME_OPT}`) — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 596-612, 663-671).
- **Níveis de otimização por tipo de arquivo** — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 584-593, 663-671):

  | Código | Nível |
  | --- | --- |
  | Código recompilado frio (`ps2EntryRunner`) | `PS2X_LINUX_GAME_OPT`, padrão `-Os` |
  | `ps2_runtime` | `-O3` |
  | `ps2_iop` | `-O3 -fno-math-errno -fno-trapping-math` |
  | Overrides | `-O3` |
  | `vu1rec_*` | `-O2` |
  | Renderer Mali e culler | `-mcpu=cortex-a35` |

- **O que são os `lean_*`.** "Lean copies of hot recompiled functions (game/overrides/lean_*.cpp): they rename the
  functions they hold (lean/renames.inc)". O segundo conjunto, `lean_part_tN.cpp`, define `PS2X_EE_TRUST_FINITE`
  (floats "confiáveis", `SOTC_EE_TRUST`) e não usa o PCH — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 466-487).
  São gerados por `tools/gen_lean.py`, com lista em `game/lean_hot.txt`; o log fala em "1846 lean functions in place
  (13032 table slots)" — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 12).
- **Ganho medido** (Phase C, modo sem sincronismo) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 310-317):

  | Passo | Game thread (média) | Render (HEAVY, média) | Binário |
  | --- | --- | --- | --- |
  | `RELEASE` | 106,23 ms | 13,94 ms | 63,50 MB |
  | `+ SELECTIVE_LTO` | 104,24 ms (−1,9%) | 13,56 ms | 64,51 MB |

- **Nossa base.**
  - `GODHAND_LTO` existe só para MSVC (`/GL` + `/LTCG`) e exclui explicitamente o código gerado. No Linux/zig não há
    LTO — `/home/user/God-hand-r36S-port-/source/godhand-recomp/CMakeLists.txt` (linhas ~50-60).
  - Todo o nosso build usa um único `-O3`, pelo `CMAKE_BUILD_TYPE=Release` + `-mcpu` — `/home/user/God-hand-r36S-port-/scripts/build_rk3326.sh`.

### Inferences
- **Esforço.** Baixo. Um bloco `if(NOT MSVC AND GODHAND_LTO)` com `-flto=thin` em `ps2_runtime`, `ps2_iop` e
  `godhand`, mais `-flto=thin` no link. Com zig é preciso checar que `zig-ar` gera arquivos com bitcode e que o lld
  faz o ThinLTO. Não há equivalente direto de `-ffat-lto-objects` no clang (inferência).
- **Ideia aproveitável do "lean".** Compilar o código gerado frio com `-Os` (menos I-cache) e só as funções quentes
  com `-O3`. Para nós: `set_source_files_properties` nos `generated/*.cpp` com `-Os`, e `-O3` numa lista quente
  derivada do perfil. Precisa de A/B; com o PGO do Celosia isso foi parte do baseline e não há número isolado.
- **Não portável.** O `SOTC_EE_TRUST` / `PS2X_EE_TRUST_FINITE` muda a semântica de float (assume valores finitos) e
  depende de difftests do jogo. Para o God Hand seria um projeto de equivalência à parte.
- **Validação.** Gravação do GS idêntica. Atenção: LTO pode mudar a ordem de avaliação de float só se houver UB, e
  `-fno-math-errno`/`-fno-trapping-math` podem mudar resultados. Se aplicarmos no IOP, conferir o áudio. Depois
  `run_ab.sh` e tamanho de `.text`.

### Gaps
- Não medi o tempo de link com ThinLTO no zig para o nosso binário (~640 MB com debug).
- O Celosia não isola o ganho do `-Os` no código frio.

## 5. PS2X_LINUX_FASTMEM (janela de 4 GiB, espelhos via memfd, handler de SIGSEGV)

### Takeaway
O fastmem do Celosia troca o teste de endereço de cada acesso por um load/store direto em `base + endereço_guest`.
O mecanismo é:
1. Uma janela `PROT_NONE` de 4 GiB.
2. RAM e scratchpad mapeados como espelhos `memfd` em todos os segmentos.
3. Um handler de SIGSEGV que decodifica a instrução ARM64 que falhou e emula o acesso de MMIO.

Ganho medido: −10,7% na game thread sem sincronismo (com RELEASE) e −24% de binário. Para nós seria a extensão do
item 2 da `analise-otimizacoes-cpu.md` (espelho de RAM): mais ganho, e risco bem maior (sinais, decodificação,
MMIO frequente). As interceptações de MMIO frequente precisam de overrides nativos por jogo, como fizeram para o
GS_CSR.

### Cited Findings
- **Janela.** `kFastmemWindowBytes = 4ULL << 30`. Em `PS2Memory::initialize`, `mmap(nullptr, 4 GiB, PROT_NONE,
  MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE)` — [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas ~454-460, ~978-1025).
- **Memória por trás.**
  - Dois `memfd` via `syscall(SYS_memfd_create=279, ..., MFD_CLOEXEC)` (`ps2_rdram` com `ramSize`, `ps2_spr` com
    16 KB) + `ftruncate`.
  - A RAM é mapeada `MAP_SHARED|MAP_FIXED` em `0x00000000`, `0x20000000`, `0x30000000`, `0x80000000` e
    `0xA0000000`. Depois de cada espelho vêm mais 64 KiB do início da RAM (wrap circular, não página de guarda).
  - O scratchpad fica em `0x70000000`, seguido de um segundo espelho de 16 KB.
  - `m_rdram` passa a ser a base da janela.
  - Fonte: [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas ~978-1025).
- O log e o r36s_port.md chamam esses 64 KiB de "circular overhang guard pages", mas o código mapeia o mesmo `memfd`
  no offset 0, ou seja, são espelhos — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 308); [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp).
- **O que fica como `PROT_NONE`.** Todo o resto (IO em `0x10000000`, GS privilegiado em `0x12000000`, VU, BIOS...).
  Um acesso ali gera SIGSEGV.
- **Handler `fastmemSigsegv`.**
  - Se o endereço que falhou cai na janela, lê a instrução em `uc_mcontext.pc` e decodifica:
    1. Classe LDR/STR (`(insn & 0x3A000000) == 0x38000000`): tamanhos 1/2/4/8, sinal (`opc` 2/3), PRFM ignorado,
       SIMD/FP via `fpsimd_context` (até 16 bytes) e writeback pré/pós-indexado.
    2. Classe LDP/STP (`0x28000000`): pares GPR/SIMD, com writeback.
  - O acesso é emulado chamando `PS2Memory::read8..128 / write8..128`. Depois o handler avança `pc += 4` e conta a
    interceptação.
  - Sem tratamento: escreve `[fastmem] unhandled fault` e passa ao handler anterior ou a `SIG_DFL`.
  - `SOTC_FASTMEM_STAT=1` imprime as 16 primeiras interceptações, depois as de índice potência de 2 e uma a cada mil.
  - Fonte: [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas ~520-800).
- **Reinstalação do handler.** `installFastmemSigsegv()` usa `SA_SIGINFO|SA_RESTART` e guarda o handler anterior.
  `PS2Memory::ensureFastmemHandler()` é chamado no início de `PS2Runtime::loadELF`, para reinstalar depois que o
  SDL_Init troca o handler — [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas ~800-835); [C:ps2_runtime.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_runtime.cpp) (linha 1259).
- **Macros.** Com `PS2X_LINUX_FASTMEM`, `PS2_GUEST_READ` vira só `readWatch` + `load<T>(rdram, addr)`, e
  `PS2_GUEST_WRITE` vira `trace` + `store<T>`, sem nenhum teste de faixa. `load/store` são um `memcpy` de
  `rdram + a`.
- Sem fastmem, o Celosia testa `_addr < PS2_RAM_SIZE` e, se falhar, chama `fn##_slow`. Nesse caminho, endereços
  kseg0/kseg1 (`0x80000000+`) vão para o lento.
- Fonte dos dois pontos: [C:ps2_runtime_macros.h](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/include/ps2_runtime_macros.h) (linhas ~665-676, 787-818).
- **Escopo da compilação.** `PS2X_LINUX_FASTMEM=1` vai para `ps2_runtime`, `ps2_iop` e `ps2EntryRunner`, só em
  Linux/aarch64 — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 623-630).
- **Ganho medido** (Phase C, `SOTC_NO_SYNC=1`) — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 310-317):

  | Build | Game thread (média) | Binário stripped |
  | --- | --- | --- |
  | `RELEASE` | 106,23 ms | 63,50 MB |
  | `RELEASE + FASTMEM` (sem LTO) | 94,82 ms (−10,7%) | 48,49 MB (−23,6%) |
  | Combinado (RELEASE+PGO+LTO+FASTMEM+SORT_PROG) | 91,41 ms (11,37 fps, +28,3% sobre o PGO sem RELEASE) | — |

- **Interceptações no SotC.**
  - Antes dos overrides de GS_CSR: >14.000 interceptações ("final_v2").
  - Depois: 16 únicas no boot e 0 em ~7.000 frames de gameplay.
  - O binário de produção Release+PGO+LTO+Fastmem tem 50,34 MB.
  - Fonte: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 308, 317, 343).
- **Kernel do R36S (4.4.189).** `memfd_create` (NR 279) funciona; `statx`, `clone3` e `io_uring` dão ENOSYS
  — [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md).
- **Nossa base, caminho de acesso.**
  1. `READ*/WRITE*` testam `PS2Runtime::isSpecialAddress` (= `Ps2IsSpecialAddress`: falso abaixo de
     `PS2_IO_BASE`, verdadeiro em kseg2/3, senão consulta o endereço físico).
  2. Se não for especial, fazem `addr & PS2_RAM_MASK`, testam `Ps2FastRangeIsContiguous` e, quando o acesso cruza o
     fim da RAM, copiam byte a byte com wrap.
  - Fonte: `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/include/ps2_runtime_macros.h` (linhas ~430-600); `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/include/runtime/ps2_address.h` (linhas 61-69).
- **Nossa base, outros pontos.**
  - Nossa análise já mediu o custo desse caminho: 8 `strb` por store de 64 bits e ~28 instruções ARM por instrução
    MIPS. Ela propõe só o espelho duplo via memfd, sem janela de 4 GiB — `/home/user/God-hand-r36S-port-/docs/analise-otimizacoes-cpu.md` (seção 2).
  - Hoje não existe nenhum handler de SIGSEGV no nosso runtime, nem em `src/` ou `platform/` (grep sem resultado).
  - O código gerado tem 10.603 arquivos em `generated/`.

### Inferences
- **O que mudaria no nosso `ps2_runtime_macros.h`.** Com fastmem, `READ32(addr)` vira só `memcpy(&v, rdram +
  (uint32_t)addr, 4)`: sem `isSpecialAddress`, sem máscara, sem caminho de wrap.
  - Hoje kseg0/kseg1 já caem no caminho rápido pela máscara, então o ganho para nós vem de remover o teste especial e
    o código frio do wrap.
  - Isso difere do Celosia, cujo baseline mandava kseg0 para o caminho lento. O ganho relativo pode ser menor que
    os −10,7% deles.
- **Diferença semântica a mapear.** Na nossa base, endereços kuseg entre 32 MB e 256 MB (`0x02000000..0x0FFFFFFF`)
  não são especiais e dão a volta pela máscara. No fastmem do Celosia eles seriam `PROT_NONE`, e cairiam no
  handler → `PS2Memory::read*`, com o comportamento que o nosso `PS2Memory` tiver para esses endereços. Seria preciso
  mapear também espelhos ali (ou aceitar interceptações) e contar com um equivalente a `SOTC_FASTMEM_STAT`.
- **Esforço: alto.** Arquivos tocados:
  1. `ps2_memory.cpp`: alocação, janela, espelhos, handler e liberação. O destrutor e o `initialize` têm
     `cleanup()`.
  2. `ps2_memory.h`: membros fd e flag.
  3. `ps2_runtime_macros.h`: `READ*/WRITE*` e `FAST_*`.
  4. Reinstalação do handler depois do init do raylib/DRM/GLFW, e possivelmente do SDL3 (se ligado).
  5. Recompilar os 10.603 gerados: sem regenerar, mas longo.
  - Atenção às rotas do nosso runtime que usam `rdram` como buffer de 32 MB com offset físico (savestate, gs_replay,
    DMA). Continuam funcionando, porque o offset 0 é a RAM. Mas qualquer `rdram + endereço_virtual` sem máscara em
    código não-gerado passa a cair em espelho ou em `PROT_NONE`.
- **Risco 1: instruções que o decodificador não trata.** Ele cobre só LDR/STR/LDUR/LDP/STP e PRFM. O clang do zig
  pode gerar LD1/ST1 (estruturas NEON), LDAR/LDXR (atômicos) ou acessos de 128 bits via `memcpy` vetorizado, que
  viram "unhandled fault" → crash. No Celosia o compilador é GCC; com clang a cobertura tem de ser revalidada
  (`objdump` dos acessos que podem tocar MMIO).
- **Risco 2: custo de cada interceptação.** Cada SIGSEGV custa uma troca de contexto do kernel (estimativa: vários
  µs). Um jogo que faz polling de MMIO no laço (GS_CSR, VIF_STAT, D_STAT, timers `0x10000000`) fica muito mais lento
  que hoje. O God Hand pode ter polling diferente do SotC (CRI/ADX). É preciso inventariar as funções que leem MMIO
  e escrever overrides nativos antes do A/B.
- **Risco 3: sinais.** Outros handlers de SIGSEGV do raylib/GLFW/libMali, `hang_dump`, ou um depurador. As threads
  GS/IOP/MTVU também fazem acessos diretos? O handler é por processo e usa `g_fastmemOwner` global.
- **Validação.**
  1. Contador de interceptações por fase (boot, título, luta); a meta é 0 interceptações por frame em gameplay.
  2. Gravação do GS idêntica byte a byte contra o binário sem fastmem (mesma rota stage1.state).
  3. `run_ab.sh`.
  4. Medir `.text` e as seções `ee`/`dispatch` do `[bench-sections]`.
  5. Teste de boot completo: título, menu, abertura, luta, memory card e save state.
- **Caminho incremental sugerido.** Primeiro o espelho duplo de 32 MB já previsto na nossa análise: remove só o
  código de wrap, sem SIGSEGV. Depois, se `ee` ainda pesar, a janela completa.

### Gaps
- Não medi o custo de uma interceptação de SIGSEGV no A35 / kernel 4.4.
- Não sei quantos acessos a MMIO por frame o God Hand faz numa luta. Precisa de contador no `Load32`/`Store32`
  especial do nosso runtime.
- O número "−10,7%" vem do modo `SOTC_NO_SYNC=1` (game ~95-106 ms por frame, até o menu, 1.368 frames). Não há medida
  do fastmem isolado na rota de gameplay padrão de 125 s.

## 6. Evitar interceptação de MMIO do GS_CSR, VIF assíncrono / MTVU, fixação de threads e governor, zram/swap, IOP e áudio

### Takeaway
1. **GS_CSR.** Os overrides só existem por causa do fastmem: são três funções do SotC reescritas para ler o CSR via
   `runtime->memory()`. Não se transferem; o que se transfere é o método (contar e eliminar as interceptações).
2. **Threads e governor.** O layout fixo de threads e o "performance" em CPU, GPU e DMC são baratos de copiar no
   launcher. Nosso `run_bench.sh` já faz o governor durante o teste; falta levar para o launcher de produção, junto
   com zram e a fixação de núcleos.
3. **MTVU.** O Celosia o deixa ligado por padrão. No nosso PC ele piorou; precisa de A/B no A35.

### Cited Findings
- **GS_CSR.**
  - Em gameplay, `[mem:slow]` e `[prof129]` mostraram leituras de `GS_CSR` (`0x120000e0`/`0x12001000`).
  - Com fastmem, overrides nativos de `kernel_sub_001C1538`, `kernel_sub_001C37F0` e `iosSyncV` (`0x001C2858`)
    eliminaram as interceptações de GS_CSR em tempo de execução.
  - Fonte: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 307-308).
  - Implementação: em `decomp_hot44g.cpp`, o comentário diz "route GS_CSR (0x12001000) directly through
    runtime->memory()", e o código chama `runtime->memory().read64(0x12001000u)` e `write64` — [C:game/overrides/decomp_hot44g.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/decomp_hot44g.cpp) (linhas 86, 155, 158, 237).
- **CSR atômico.** No `ps2_memory.cpp` do Celosia o CSR é `std::atomic<uint64_t>`, com `writeCsrHalf` por
  compare_exchange (SIGNAL/FINISH são write-one-to-clear) — [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas ~95-140).
- **MTVU e VIF1 assíncrono no Celosia.**
  - `m_mtvuEnabled = !mtvu || atoi(mtvu) != 0`, ou seja, ligado por padrão (`PS2X_MTVU=0` desliga).
  - A fila tem um limite alto (`kMtvuQueueHigh`).
  - Há uma thread VIF1 assíncrona (`m_vif1A`).
  - Fonte: [C:ps2_memory.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_memory.cpp) (linhas 269-300, destrutor ~840-860).
- **MTVU na nossa base.** `PS2X_MTVU` lê a variável de ambiente, ou usa `s_defaultEnabled` se ela não existir
  — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/src/lib/ps2_async_vif.cpp` (linhas 120-130). No PC, `PS2X_MTVU=1` piorou de 97% para 82% de velocidade
  — `/home/user/God-hand-r36S-port-/docs/analise-otimizacoes-cpu.md` (seção 4).
- **Fixação de threads no Celosia.** `SOTC_THREADS=1` (padrão) faz `threads::place(kind)` fixar com
  `pthread_setaffinity_np`:

  | Thread | Núcleos |
  | --- | --- |
  | Game | 0, exclusivo |
  | Worker (HookWorker) | 1-2 |
  | Lane/offload/guest | 1-3 |
  | Render | 2-3 |
  | Build/log/recorder | 1-3, com nice maior |

  Fonte: [C:native_renderer.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/native/native_renderer.cpp) (linhas 1825-1836, 2090-2130). O launcher exporta `SOTC_THREADS=1` — [C:ports/r36s/sotc.sh](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/ports/r36s/sotc.sh).
- **Paralelismo que não compensou.** `SOTC_CHAR104=1` (personagens em worker) foi rejeitado: a game thread esperava
  3,426 ms por frame pelo worker — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 104).
- **Fixação de threads na nossa base.** O modo bench tem regras de fixação por nome de thread (`sched_setaffinity`)
  e `PS2X_GAME_NICE` (setpriority na GameThread) — `/home/user/God-hand-r36S-port-/source/godhand-recomp/runtime/ps2xRuntime/src/lib/ps2_bench.h` (linhas ~360-385).
- **Launcher do Celosia.**
  1. Para o `batt_led.service`.
  2. Governor `performance` nas 4 CPUs e `scaling_max_freq=1512000`.
  3. Governor `performance` nos devfreq de GPU e DMC, com `min_freq = max_freq`.
  4. `swapon /dev/zram0`, `vm.swappiness=60`, `drop_caches=3`.
  5. Mata o EmulationStation só quando lançado via SSH.
  6. Áudio: `SOTC_AUDIO_MONO=1`, `PS2X_AUDIO_FRAMES=4096`.
  7. Ritmo: `SOTC_FPS_CAP=30` + `SOTC_FRAME_INTERVAL=2` (a versão atual do launcher; os docs de novembro citam 60).
  - Fonte: [C:ports/r36s/sotc.sh](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/ports/r36s/sotc.sh).
- **Governor na nossa base.** O `run_bench.sh` põe CPU e devfreq em "performance" durante a medição e restaura
  depois — `/home/user/God-hand-r36S-port-/scripts/run_bench.sh` (linhas 15, 39-58). Os launchers em
  `scripts/ports/` (`_comum.sh`) não mexem em governor nem em zram (grep sem resultado).
- **IOP.** `ps2_iop` recebe `-O3 -fno-math-errno -fno-trapping-math`. O `SoundMixer` do IOP (reverb, mixVoice,
  decodeBlock) aparece na zona quente da ordem de símbolos — [C:ps2xRuntime/CMakeLists.txt](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/CMakeLists.txt) (linhas 587-589); [C:game/overrides/linux_hot_order.ld](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/linux_hot_order.ld).
- **RAM do R36S.** 895 MiB, ~708-726 MiB disponíveis, 511 MiB de ZRAM — [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md).

### Inferences
- **GS_CSR para nós.** Hoje um acesso ao CSR cai em `isSpecialAddress` → `runtime->Load64`: é uma chamada de função,
  não uma interceptação. Só vira problema com fastmem. O equivalente seria identificar, no perfil do God Hand, as
  funções do kernel/libgraph que fazem polling de CSR (sync de vsync, `sceGsSyncV`) e escrever overrides que chamem
  `runtime->memory()` direto. Os endereços (`0x001C1538` etc.) são do ELF do SotC e não servem.
- **Launcher.** Copiar "performance" em CPU/GPU/DMC, `min_freq=max` e zram é trivial e só afeta o tempo. Risco:
  temperatura e bateria. Nosso `run_ab.sh` já mede a temperatura; validar com a corrida longa de 900 s.
- **Fixação de threads.** Nossa base já tem o mecanismo no modo bench. Testar `GameThread`→0, `GSThread`/`GIFThread`
  →1-2 e `IOPThread`→3 com o `run_ab.sh`. Os nomes de thread são nossos (`ThreadNaming`), então não há conflito com
  o Celosia.
- **MTVU.** Testar no A35 com `PS2X_MTVU=0` contra `1`. O padrão do Celosia ser "ligado" não garante que sirva para
  a nossa implementação, que é outra.

### Gaps
- O Celosia não tem A/B isolado do layout de threads nem do zram.
- Não li a implementação do VIF1 assíncrono (`m_vif1A`) do Celosia a fundo para comparar com o nosso
  `ps2_async_vif.cpp`.
- Não verifiquei se o nosso launcher precisa de `sudo` para escrever em sysfs no ArkOS/dArkOS.

## 7. Método de benchmark: rota determinística por input script vs o nosso (save state + campo fixo + gravação do GS)

### Takeaway
O Celosia mede com o jogo real: rota de New Game por `PS2X_INPUT_SCRIPT` temporizado em segundos, 125-130 s, com o
renderer GLES ativo. Compara medianas de buckets de 60 frames (`SOTC_FRAME_LOG`) numa faixa de frames casada. O
roteiro às vezes deriva. O nosso método (save state + nenhum botão + GS nulo + A/B intercalado + checagem térmica +
gravação do GS byte a byte) é mais determinístico para CPU. O do Celosia acrescenta medir com o renderer real e o
A/B rotativo dentro do mesmo processo.

### Cited Findings
- **Roteiro.** `PS2X_INPUT_SCRIPT="<sec>:<BUTTON>:<duration>;..."` é lido em `ps2_pad.cpp` — [C:ps2_pad.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/PS2Recomp/ps2xRuntime/src/lib/ps2_pad.cpp) (linhas 201, 254).
  O `tools/bench.sh` faz START + CROSS até o New Game, roda `autotest 190`, e tira médias de `[stats]` por trecho
  (título, ponte, santuário) — [C:tools/bench.sh](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/tools/bench.sh).
- **Rota no R36S.**
  - 125 s até o Save-Shrine, com memory card, via `r36s_run_bench.py`.
  - Janela comparada: `gp 4458..5298` = 15 buckets × 60 frames, ou `f=4500..5900` (1.401 frames, com
    `SOTC_FRAME_LOG_ALL=1`).
  - Métricas: FPS min/mediana/média/máx e as medianas de `game`, `hooks`, `render` e `present`, mais p50/p90/p99 do
    frame.
  - Governor `performance` e `SOTC_GL_SHOTS=0`.
  - Fonte: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 68-70, 178, 205, 236).
- **Deriva do roteiro.** Na rodada 1 do A3_PGO o roteiro por tempo desalinhou no frame 2940 e caiu na pausa (12
  draws). Só a rodada 2 casou a rota — [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linha 246).
- **Equivalência das reescritas nativas.**
  - Modos `CHECK=1` / `SOTC_DIFFTEST` comparam a reescrita com o caminho original, com contagem de diferenças.
    Exemplo: `s3chara` com 60.001 poses e 0 diferenças.
  - A/B rotativo dentro do processo: `SOTC_AB_PERIOD=120 SOTC_AB_ROTATE=1`, com média ± desvio por bin.
  - Fonte: [C:SOTC_OPTIMIZATION_LOG.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/SOTC_OPTIMIZATION_LOG.md) (linhas 327-341).
- **Log de frame.** `SOTC_FRAME_LOG` / `SOTC_FRAME_LOG_ALL=1` (cada frame numa linha). Continua ativo no build
  release (`SOTC_RELEASE` mantém `SOTC_FRAME_LOG=1`) — [C:game/overrides/sotc_native.cpp](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/game/overrides/sotc_native.cpp) (linhas 157, 4943); [C:docs/r36s_port.md](https://github.com/amarqs182/celosia/blob/39ab080c85ee6ee35d3a54b142dc79132edd9dfb/docs/r36s_port.md).
- **Nosso `run_ab.sh`.**
  - Oito execuções de 300 s intercaladas (A1 B1 B2 A2 A3 B3 B4 A4), mais A5/B5 de 900 s em `--longo`.
  - Mesma ISO e mesmo `stage1.state` (primeira luta da fase 1).
  - `PS2X_PAD_SCRIPT` vazio, GS nulo, mesmo `godhand.ini`, governors em performance, `PS2X_PROF=1`.
  - Cooldown de 60 s e espera térmica de até 120 s antes de cada execução; SHA-256 dos binários conferido.
  - Fonte: `/home/user/God-hand-r36S-port-/scripts/run_ab.sh` (linhas 1-20). A análise e a comparação ficam em `/home/user/God-hand-r36S-port-/tools/bench_compare.py`.

### Inferences
- **O que copiar.**
  1. Reportar mediana + p90/p99 do frame e janelas de frame casadas (o `bench_compare.py` pode ganhar percentis, se
     não tiver).
  2. A regra "recoletar PGO após qualquer mudança": vale para nós se adotarmos PGO.
  3. Um A/B rotativo no mesmo processo, para flags de tempo de execução (por exemplo `PS2X_IOP_SPIN_US`, MTVU),
     alternando a cada N fields. Cancela a deriva térmica melhor que execuções separadas.
  4. Um contador de interceptações por fase, se fizermos fastmem.
- **O que não copiar.** O roteiro por segundos: o nosso é por field (`1300v:start:10`) e parte de save state, então é
  mais robusto contra a deriva que eles sofreram.
- **Lacuna do nosso método.** O GS nulo não mede o custo do renderer GLES no A35. O Celosia mostra que no R36S o
  `render`/`present` (14-30 ms) é da mesma ordem que a game thread. Uma segunda rodada com GLES ligado (o
  `run_gles.sh` já existe para replay) completa o quadro.

### Gaps
- Não li o `r36s_run_bench.py` (não aparece no clone em `tools/`; a busca por `*bench*.py` não o encontrou). Não sei
  como ele extrai os buckets.
- Os números do Celosia são de SotC com renderer nativo Mali. Nenhum é diretamente comparável ao God Hand. Servem
  como ordem de grandeza: Release+PGO ≈ +15% de FPS de mediana nas fases 2-4; fastmem ≈ −10% na game thread sem
  sincronismo.
