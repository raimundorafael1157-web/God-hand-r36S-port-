# Medição no R36S (caminho B)

Decidido em 25/11/2026: medir a CPU direto no aparelho com um build ARM64 sem tela, em vez de estimar a partir do PC.

## O que existe e o que foi verificado

| Item | Estado | Como foi verificado |
| --- | --- | --- |
| Runtime + código do port compilam no Linux x86_64 | Feito | GCC 13, sem erros; só faltam os símbolos gerados do disco |
| Build ARM64 para o R36S | Feito | zig 0.17.1, alvo `aarch64-linux-gnu.2.30`, `-mcpu=cortex-a35` |
| Compatível com ArkOS (glibc 2.30) | Feito | `objdump -T`: maior símbolo exigido é `GLIBC_2.30` |
| Binário ARM64 inicia | Feito | qemu-aarch64: lê `godhand.ini`, recusa ISO ausente com a mensagem certa |
| Módulo de medição | Feito | teste com jogo simulado no qemu: 60 campos/s, 30 flips/s, 100% CPU, como esperado |
| Rodar o jogo de verdade | **Não testado** | precisa do ISO |
| Medição no aparelho | **Não feito** | precisa do R36S |

## Mudanças no projeto-base

O repositório original fica intacto. `scripts/setup_base.sh` clona LasVola/godhand-recomp no commit
`14e956a`, aplica `patches/` e copia `cmake/` e `platform/`.

| Patch | O quê |
| --- | --- |
| `0001-linux-build.patch` | `#include <cstddef>` faltando; `to_string` ambíguo no libc++; include do runtime para o código do port no Linux |
| `0002-arm64-drm.patch` | raylib em KMS/DRM (sem X11/GLFW): define `PLATFORM_DRM`, controles GLFW desligados nesse modo |
| `0003-bench-mode.patch` | `PS2X_BENCH=1` (sem janela, GPU e áudio; linha `[bench]` por segundo), `PS2X_GS_NULL=1` (GS descarta primitivas), save state em hora marcada |
| `0004-r36s-resolution.patch` | resolução interna padrão 1x (512x448, abaixo da tela 640x480 do R36S) e travada em 1x, mesmo com `godhand.ini` antigo; `GODHAND_ALLOW_HIRES=1` libera |
| `platform/arm64-intrin` | cabeçalhos SSE do x86 redirecionados para sse2neon (MIT, v1.8.0, sha256 conferido) |

## O que a linha `[bench]` mostra

```
[bench] t=42 fields/s=59.9 speed=100% flips/s=30.0 worst=35ms cpu=102% rss=410MB | GameThread=100% VUThread=12%
```

- `fields/s`: campos de vsync emulados por segundo. 59,94 = velocidade do PS2.
- `flips/s`: trocas do buffer exibido. Num jogo com double buffer, é o FPS do próprio jogo.
- `worst`: maior intervalo entre dois flips no segundo.
- `cpu`: 100% = um núcleo inteiro; 400% = os quatro núcleos.
- `rss`: memória residente em MB.

No fim sai uma linha `[bench-summary]` com médias, percentil 5 e pior caso, sem os primeiros 20 s (boot).

## Limites conhecidos

- **Sem controle:** sem janela, ninguém joga. Sem save state, a medição fica no título e na demo. Para medir luta é
  preciso um save state (`PS2X_BENCH_LOAD_STATE`) feito antes, num PC. Isso depende de save states feitos no x86
  carregarem no ARM64, o que ainda não foi testado.
- **VU1 interpretado:** o código VU1 pré-compilado (`generated/vu1`) exige capturar os programas rodando o jogo.
  Sem ele, o VU1 roda interpretado, mais lento. A medição mostra quanto pesa (`VUThread`).
- **GS nulo não é o jogo real:** mede só o que vem antes do renderizador. É o piso de custo: se nem isso chega a
  20 quadros/s, nenhum renderizador resolve.
- O renderizador GPU do projeto-base só existe para Windows (OpenGL desktop via WGL).
