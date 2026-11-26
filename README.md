# God Hand no R36S

Pesquisa e port de **God Hand** (PS2, Clover Studio, 2006) para portáteis com **Rockchip RK3326**: R36S, R36H e
clones com ArkOS ou dArkOS. O jogo roda como código nativo ARM64, sem emulador de PS2, a partir da recompilação
estática do [God Hand Recompiled](https://github.com/LasVola/godhand-recomp).

> **Fase atual: medir antes de portar.** O RK3326 é muito mais fraco que um PC. Antes de qualquer trabalho de
> renderização, este projeto mede se o processador do R36S aguenta a lógica do jogo em velocidade normal.
> Concluir que não aguenta é um resultado válido.

**Nenhum arquivo do jogo está aqui.** Você precisa do seu próprio disco americano (SLUS-21503).

## Situação

| Etapa | Estado |
| --- | --- |
| Código do projeto base compilando no Linux x86_64 | Feito |
| Build ARM64 para o R36S (glibc 2.30, compatível com ArkOS) | Feito |
| Binário ARM64 iniciando no emulador (qemu) | Feito |
| Modo de medição sem tela (`PS2X_BENCH=1`) e GS nulo (`PS2X_GS_NULL=1`) | Feito, testado com jogo simulado |
| Código do jogo gerado a partir de um disco verificado | Feito (10.602 arquivos + 81 módulos) |
| Jogo completo rodando no Linux x86_64 | Em andamento |
| Medição no R36S | Pendente: precisa do aparelho |
| Renderizador para a Mali-G31 | Só se a medição der go |

O critério de decisão está em [`docs/phase0-report.md`](docs/phase0-report.md): com os gráficos desligados, a
velocidade precisa ficar perto de 100% nas cenas pesadas. Abaixo disso o jogo roda em câmera lenta, e nenhum
renderizador resolve.

## Como funciona

O projeto base fica intacto. Este repositório guarda só o que muda:

```
patches/    mudanças sobre LasVola/godhand-recomp@14e956a, uma por assunto
platform/   cabeçalhos SSE -> NEON (sse2neon) e o compilador zig para ARM64
cmake/      toolchain do RK3326 (aarch64, glibc 2.30, Cortex-A35)
scripts/    montar a base, compilar, coletar dados do aparelho e medir
docs/       plano, critérios, modo de medição, roteiro do teste no aparelho
research/   trabalhos anteriores e como outros ports do R36S foram feitos
tests/      teste do módulo de medição
```

| Patch | O que faz |
| --- | --- |
| `0001-linux-build` | Correções para compilar no Linux com GCC e Clang |
| `0002-arm64-drm` | raylib direto em KMS/DRM, sem X11 |
| `0003-bench-mode` | Medição sem janela: velocidade, FPS do jogo, CPU por thread, memória, clock e temperatura |
| `0004-r36s-resolution` | Resolução interna fixa em 1x (512x448), abaixo da tela 640x480 do R36S |
| `0005-game-frame-counter` | Conta os quadros do jogo pela função que monta a cópia de tela (o God Hand não troca o buffer de exibição) |

## Compilar

Num Linux x86_64 (ou WSL no Windows), com `cmake`, `ninja`, `g++`, `python3` e `git`:

```sh
./scripts/setup_base.sh                                   # baixa a base e aplica os patches
./scripts/build_rk3326.sh generate "/caminho/God Hand (USA).iso"   # gera o código a partir do SEU disco
./scripts/build_rk3326.sh build                           # binário ARM64 em builds/rk3326/
```

O disco precisa bater com o SHA-256 `268b8e28…0a7c` (lista completa em `DISC_HASHES.md` do projeto base).
O código gerado fica em `source/`, que nunca é enviado ao GitHub.

## Medir no R36S

Copie `builds/rk3326/` para o cartão SD e, num terminal do aparelho:

```sh
sh collect_hw_info.sh > hw.txt                        # dados do hardware
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 600   # 10 minutos, só a lógica do jogo: abertura e começo da fase 1
```

O resultado sai numa linha `[bench-summary]` e num arquivo `bench-*.log`. O que cada número quer dizer está em
[`docs/bench-mode.md`](docs/bench-mode.md). Passo a passo completo em [`docs/roteiro-sexta.md`](docs/roteiro-sexta.md).

## Créditos e licença

- [God Hand Recompiled](https://github.com/LasVola/godhand-recomp) (LasVola), base deste port, GPL-3.0
- PS2Recomp (ran-j, LightVelox) e o port de Urban Reign, de onde vem o runtime
- [god-hand-decomp](https://github.com/LucasPicoli/god-hand-decomp) (LucasPicoli), limites de função usados pela recompilação
- [sse2neon](https://github.com/DLTcollab/sse2neon), MIT; [raylib](https://github.com/raysan5/raylib), zlib; [zig](https://ziglang.org), MIT

As mudanças deste repositório são **GPL-3.0**, como o projeto base ([LICENSE](LICENSE)).

God Hand é marca e propriedade da Capcom. Projeto de fã, sem relação com a Capcom ou com a Clover Studio.
