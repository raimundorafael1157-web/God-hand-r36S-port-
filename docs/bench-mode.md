# Medição no R36S (caminho B)

Decidido em 25/11/2026: medir a CPU direto no aparelho com um build ARM64 sem tela, em vez de estimar a partir do PC.

## O que existe e o que foi verificado

| Item | Estado | Como foi verificado |
| --- | --- | --- |
| Runtime + código do port compilam no Linux x86_64 | Feito | GCC 13, sem erros; só faltam os símbolos gerados do disco |
| Build ARM64 para o R36S | Feito | zig 0.17.1, alvo `aarch64-linux-gnu.2.30`, `-mcpu=cortex-a35` |
| Compatível com ArkOS (glibc 2.30) | Feito | `objdump -T`: maior símbolo exigido é `GLIBC_2.30` |
| Binário ARM64 inicia | Feito | qemu-aarch64: lê `godhand.ini`, recusa ISO ausente com a mensagem certa |
| Módulo de medição | Feito | teste com jogo simulado no qemu (entrelaçado, 30 FPS): 60 campos/s, 30 flips/s, 60 disp/s, 100% CPU, como esperado |
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
| `0009-bench-profiling.patch` | instrumentação: tempo da GameThread por subsistema, sensores de clock/temperatura/GPU/memória, afinidade opcional, resumo térmico (seção abaixo) |
| `platform/arm64-intrin` | cabeçalhos SSE do x86 redirecionados para sse2neon (MIT, v1.8.0, sha256 conferido) |

## O que a linha `[bench]` mostra

Exemplo do formato (números ilustrativos, não medidos):

```
[bench] t=42 fields/s=59.9 speed=100% flips/s=30.0 disp/s=59.9 worst=35ms cpu=102% rss=410MB mhz=1512 temp=58C | GameThread=100% VUThread=12%
```

- `fields/s`: campos de vsync emulados por segundo. 59,94 = velocidade do PS2.
- `speed`: campos/s divididos por 59,94. **É o número que decide.** Abaixo de 100% o jogo fica em câmera lenta.
- `flips/s`: trocas da página do buffer exibido (FBP). Num jogo com double buffer, é o FPS do próprio jogo.
- `disp/s`: qualquer mudança no DISPFB, inclusive o deslocamento por campo de um jogo entrelaçado.
- `worst`: maior intervalo entre dois flips no segundo.
- `cpu`: 100% = um núcleo inteiro; 400% = os quatro núcleos.
- `rss`: memória residente em MB.

No fim sai uma linha `[bench-summary]` com médias, percentil 5 e pior caso, sem os primeiros 20 s (boot).

## Instrumentação (patch 0009)

Nada aqui muda o que o emulador executa. Tudo fica desligado por padrão: o comando simples mede exatamente o que
media antes.

| Opção | O que faz |
| --- | --- |
| `PS2X_PROF=1` | Divide o tempo da GameThread por subsistema: `ee`, `vu1`, `vu0`, `iop`, `dispatch`, `sched`, `gif`, `vif`, `idle` (folga: nada a fazer ou adiantado em relação ao tempo real), `gs-wait` (esperando a thread do GS) |
| `PS2X_PIN=auto` | Fixa GameThread, IOPThread e GSThread nos núcleos 3, 2 e 1 (ou `PS2X_PIN=GameThread:3,IOPThread:2,...`) |
| `PS2X_GAME_NICE=-5` | Prioridade maior para a GameThread (precisa de root) |
| 900 s no `run_bench.sh` | Rodada longa; `[bench-thermal]` compara o primeiro e o último minuto |

**Como mede.** Cada ponto de entrada de subsistema troca a "seção atual" da GameThread lendo o contador do
processador (`CNTVCT_EL0` no ARM64, 24 MHz no RK3326; TSC no PC). O tempo é exclusivo: um microprograma VU1
chamado de dentro do código do EE conta como `vu1`, não `ee`, e as seções somam o tempo de parede da thread.

O despacho e o agendador são chamados ~10 milhões de vezes por segundo. Cronometrar todas as chamadas custava ~45% da
thread no PC, então elas são **amostradas**:
- todas as chamadas são contadas;
- 1 em 128, sorteada pseudoaleatoriamente, é cronometrada, e o total é extrapolado;
- o custo de uma leitura do contador (medido no início) é descontado de cada trecho cronometrado;
- seções exatas que ocorrem dentro delas (por exemplo, a espera de ritmo dentro do checkpoint) pausam o cronômetro.

O caminho rápido dos checkpoints (o laço apertado) não é marcado e fica em `ee`.

**Linhas novas no log** (os campos antigos da linha `[bench]` não mudaram de lugar; o novo vai no fim):

```
[bench-sensors] cpu clocks: 4 cores | cpu max: ok | thermal: soc-thermal gpu-thermal | devfreq: ff400000.gpu (load) dmc (no load)
[bench] ... | GameThread=99% IOPThread=60% | clocks: cpu=1296 max=1296 temps=soc-thermal:52,gpu-thermal:50 ff400000.gpu=400/37% dmc=786 | game: ee=31% vu1=26% ...
[bench-sections] GameThread wall time: ee=... | switches/s=... x ... ns + sampled markers/s=... x ... ns -> overhead ~...% | counter read bias ... ticks
[bench-thermal] first 60 s: speed=..% temp=..C | last 60 s: speed=..% temp=..C | temp max=..C | cpu cap min=.. MHz | gpu clock min=.. MHz
```

(exemplo de formato; os nomes dos sensores dependem do kernel). O que o kernel não expõe sai como `unavailable` no
`[bench-sensors]` e `n/a` por segundo, e o teste continua.

**Verificação no PC (Xeon 2,1 GHz, luta da fase 1, 3 rodadas de 60 s por variante):**

| Variante | Velocidade média | CPU da GameThread |
| --- | --- | --- |
| Base, sem os marcadores | 98,7% | 61,9% (56–68) |
| Instrumentada, desligada | 98,7% | 65,6% (64–67) |
| Instrumentada, `PS2X_PROF=1` | 97,3% | 70,2% (68–72) |

- **Desligada:** um marcador custa +0,02 ns (despacho) e +0,29 ns (seção exata), medido isoladamente. Com ~10
  milhões por segundo, isso é <0,05%. A diferença na tabela é ruído da VM.
- **Ligada:** custa ~6–8% da GameThread pela estimativa do próprio bench, e até ~13% na CPU medida acima. O
  `[bench-sections]` de cada rodada informa a estimativa medida no próprio aparelho. A medida de velocidade que
  decide é a rodada sem `PS2X_PROF`.
- **Fidelidade:** o fluxo de comandos do GS e os hashes da VRAM de 60 fields da luta saíram idênticos byte a byte
  (323.613.961 bytes, mesmo SHA-256) em duas rodadas da base, uma desligada e uma ligada.
- **Divisão medida no PC com `PS2X_PROF=1`:** ee 23–27%, vu1 23–24%, dispatch 11–13%, sched ~4%, gif ~4%, vif ~2%,
  iop ~1%, idle 26–31% (folga, porque no PC o jogo sobra).
- O teste `tests/bench_loop_test.cpp` confere a contabilidade: 25% simulados em vu1 saem 25,0%, e ~13% simulados em
  despacho amostrado saem 13–14%. Passa em x86-64 e em ARM64 (qemu, `CNTVCT_EL0`).

## Limites conhecidos

- **Sem controle:** sem janela, ninguém joga. Sem save state, a medição fica no título e na demo. Para medir luta é
  preciso um save state (`PS2X_BENCH_LOAD_STATE`) feito antes, num PC. Isso depende de save states feitos no x86
  carregarem no ARM64, o que ainda não foi testado.
- **VU1 interpretado:** o código VU1 pré-compilado (`generated/vu1`) exige capturar os programas rodando o jogo.
  Sem ele, o VU1 roda interpretado, mais lento. A medição mostra quanto pesa (`VUThread`).
- **GS nulo não é o jogo real:** mede só o que vem antes do renderizador. É o piso de custo. O critério é `speed`
  perto de 100%: abaixo disso o jogo fica em câmera lenta, e nenhum renderizador resolve.
- O renderizador GPU do projeto-base só existe para Windows (OpenGL desktop via WGL).
