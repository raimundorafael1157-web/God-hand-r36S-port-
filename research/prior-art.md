# Trabalho anterior relevante

Levantado em 25/11/2026. Fato = lido no repositório; inferência = marcado como tal.

## LasVola/godhand-recomp (base)

- https://github.com/LasVola/godhand-recomp, GPL-3.0, último commit 2026-10-20 (`14e956a`), 53 estrelas.
- Windows/MSVC, janela via raylib, SDL3 para controles.
- Renderizador padrão `native`: GS próprio em `runtime/ps2xRuntime/src/lib/gs/`, com backend GPU em OpenGL desktop
  core (`gpu/gs_gl.cpp`, contexto WGL) e um backend CPU (`gs_cpu_backend.cpp`, `gs_cpu_raster.cpp`).
  Alternativa `pcsx2` (GS do PCSX2 como DLL), só com `-DGODHAND_PCSX2_GS=ON`.
- Resolução interna em `godhand.ini`, `[Video] InternalResolution` (1 = 512x448 nativo). Responde a dúvida da Fase 0, passo 1.
- Já tem modo `Headless=1` e `TimeLimitSeconds`, úteis para medir a CPU sem tela.
- 27 arquivos em `runtime/` e `src/` usam intrínsecos x86 (`_mm_*`, `*mmintrin.h`): precisam de NEON ou sse2neon.

## Ptraaah/godhand-recompiled-android (port ARM64 já existente)

- https://github.com/Ptraaah/godhand-recompiled-android, GPL-3.0, **descontinuado** em 2026-11-14 (`d88f02a`).
  O autor libera forks.
- **Correção (26/11):** o repositório publica só a casca Android e a ponte do GS do PCSX2 (`android/`, `path-b/`).
  As mudanças ARM64 do núcleo (runtime e código do port, `libgodhand.so`) **não foram publicadas**. Os relatórios
  em `docs/milestones/` descrevem o que foi feito e servem de mapa, mas o porte do núcleo foi refeito aqui
  (ver `docs/bench-mode.md`).
- Aparelho de referência: POCO F5 (Snapdragon 7+ Gen 2, Adreno 725, 8–12 GB). Números do `docs/PROGRESS.md`:
  - Renderizador CPU do runtime: tela de título a **1 FPS** (pior quadro 1.328 ms).
  - Renderizador GPU próprio (GLES): menus 60 FPS; em jogo o mundo avançava 1–3 quadros novos por segundo.
  - Ponte do GS do PCSX2 sobre Vulkan (padrão de m24 a m45): cenas com multidão a **~42–58 campos/s**
    (60 = velocidade cheia), "laggy em multidões", áudio ~80% bom.
  - m44: em celular com GPU **Mali** (Dimensity 8050) o renderizador Vulkan não abriu; tela preta.
- Instrumentação pronta para reaproveitar: `[frame-split]` (sync, gs-fetch, draw, swap), `[iop-thread]`, `[gs-wait]`.

## Inferência para o RK3326

- O port Android prova que o código recompilado roda em ARM64, mas o código do núcleo não está disponível. O porte
  ARM64 do runtime foi refeito aqui a partir do projeto-base, com o gancho `USE_SSE2NEON` que o próprio runtime já tinha.
- Ordem de grandeza (aproximada, de memória, **a medir**): um núcleo Cortex-A35 a ~1,4 GHz rende cerca de 1/10 a 1/15
  do núcleo principal do Snapdragon 7+ Gen 2; a Mali-G31 MP2 tem cerca de 1/40 do poder de cálculo da Adreno 725.
