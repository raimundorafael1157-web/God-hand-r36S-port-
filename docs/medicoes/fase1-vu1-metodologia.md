# Fase 1: investigação do VU1, microbenchmarks isolados

Estado em 28/11/2026. **Fase de medição: nenhuma otimização foi aplicada.**
- Os binários A e B não mudaram: `godhand-A` 89a78cb4…, `godhand-B` 56db0e17….
- O runtime de produção não mudou.
- Os resultados destes microbenchmarks **não** são tempo de emulação. São custos por operação, medidos isoladamente,
  e servem para explicar o tempo de emulação, não para substituí-lo.

## 1. Ponto de partida: o que já se sabe do jogo

| Medida | PC | R36H | Fonte |
|---|---|---|---|
| VU1 na GameThread (luta) | ~3,8 ms/field | ~134 ms/field | `[bench-sections]` (A35: relatório de 27/11) |
| Execuções do VU1 por field | 572 | (a medir) | `[bench-vu1]`, patch de medição M01, só no PC |
| Ciclos do VU executados por field | ~929 mil, 100% no código pré-compilado | (a medir) | idem |
| Custo por ciclo do VU | 4,1 ns | ~144 ns (estimado = 134 ms / 929 mil) | idem |

A estimativa de 144 ns supõe que o A35 executa o mesmo número de ciclos do VU que o PC. Os dois rodam o mesmo save state
sem entradas, então isso deve valer. A confirmação vem do binário de medição M01 no aparelho, numa rodada futura.

## 2. As operações mais usadas pelo VU1 (item 1)

### Chamadas no código gerado
Contagem estática nos 5 microprogramas do God Hand, 123.614 linhas de C++ gerado:

| Chamada no código gerado | Vezes | O que é |
|---|---|---|
| `rmax` | 10.967 | Contabilidade de ciclos: espera pelos registradores (escalar, `uint64_t`) |
| `_mm_loadu_ps` / `_mm_load_ps` | 3.265 / 2.515 | Leituras de registradores VF e da memória do VU |
| `qaddr` | 3.125 | Endereço na memória do VU |
| `_mm_storeu_ps` / `_mm_store_ps` | 1.885 / 1.537 | Escritas |
| `vnorm` | 1.865 | Normalização de denormais e infinitos do PS2 (6 operações inteiras SIMD) |
| `ldq` | 1.770 | Leitura de 128 bits da memória do VU |
| `writeVf` | 1.653 | Escrita de VF: cópia crua + cópia normalizada, com máscara de componentes |
| `_mm_setzero_ps` | 1.476 | |
| `blendDest` | 1.281 | Máscara de componentes xyzw |
| `nv` / `nbc` / `nacc` | 1.122 / 495 / 320 | Leituras de registradores já normalizados |
| `stq` | 763 | Escrita de 128 bits na memória do VU |

Toda instrução também passa por uma checagem de orçamento de ciclos (`if (cyc >= budget)`) e por `++cyc`.

### Dentro das contas (FMAC)
- `fmacCore`: `mul`/`add`, 6 a 10 comparações e máscaras, e 1 `movemask` para decidir se o caminho rápido é seguro.
- Depois, no caminho rápido: mais 3 a 4 `movemask` com tabela `kRev4` para as flags, e o empilhamento das flags num
  anel em memória.
- Se o resultado sai da faixa segura, `fmacExact`: conversões para `double` (`_mm_cvtps_pd`), `mul_pd`/`add_pd`,
  comparações `_pd` e `movemask_pd`.
- Cada entrada e saída do VU1 lê e grava o modo de arredondamento (`_mm_getcsr`/`_mm_setcsr`).

## 3. Como o sse2neon traduz essas operações (item 2)

Tabela completa em `fase1-traducao-sse2neon.txt`. Cada intrínseca foi compilada sozinha:
- x86: gcc 13, `-O3 -msse4.1`;
- ARM: o zig do build, clang 22, `-mcpu=cortex_a35 -O3`, com o mesmo `sse2neon.h` do runtime.

| Intrínseca | x86 | ARM (sse2neon) | Observação |
|---|---|---|---|
| add/mul/and/or/andnot/cmp `_ps`, `_pd`, `cvtps_pd`, `set1`, load/store | 1 | 1 | Tradução direta |
| `_mm_shuffle_ps` (broadcast, mistura) | 1 | 1 (`dup`, `uzp2`) | Direta |
| `_mm_shuffle_ps(3,0,2,1)` (produto vetorial) | 1 | 3 (`ext`, `zip1`, `zip2`) | Pequena expansão |
| **`_mm_movemask_ps`** | 1 | **8**: 2 leituras de tabela, `ushl`, `and`, **`addv`**, `fmov` | Expansão grande, com soma entre pistas e passagem NEON→escalar |
| `_mm_movemask_pd` | 1 | 4 (`ushr`, `mov`, `fmov`, `orr`) | Expansão, com passagem NEON→escalar |
| **`_mm_getcsr`** | 1 (`stmxcsr`) | **chamada a `fegetround()` da libc** + tabela | Função externa |
| **`_mm_setcsr`** | 1 (`ldmxcsr`) | **chamada a `fesetround()` da libc** + tabela | Função externa, que escreve no FPCR |
| `rmax` | 2–3 | 2 (`cmp`, `csel`) | Direta |

### Testes completos (instruções da função inteira, 8 operações por volta)

| Teste | x86 | ARM |
|---|---|---|
| `instr_fmac` (FMAC completo) | 1263 | 1113 |
| `fmac_fast` | 996 | 934 |
| `fmac_exact` | 416 | 378 |
| `vnorm_thr` | 161 | 94 |
| `rmax_vf` | 315 | 253 |
| `movemask_thr` | 53 | 78 |

**Leitura preliminar:** fora o `movemask` e o modo de arredondamento, o ARM gera **tantas instruções quanto o x86, ou
menos**. Uma ineficiência de tradução no total de instruções não explica, sozinha, um VU1 ~50x mais lento contra ~20x do
código escalar. A hipótese que os testes no aparelho vão confirmar ou derrubar é microarquitetural:
- o A35 executa em ordem e trava em cada dependência;
- o NEON do A35 processa 128 bits em duas metades de 64;
- `fmov`/`umov` (NEON→escalar) e `addv` têm latência alta, e cada `movemask` cria uma dependência NEON → escalar →
  tabela em memória → desvio.

## 4. Os microbenchmarks (item 3)

Programa `tools/vu1_microbench/vu1_microbench.cpp`, independente do jogo e do runtime.
- **Origem do código:** as funções auxiliares são cópias de `ps2_vu1_impl.inl`.
- **Formato dos passos:** cada teste repete uma operação 8 vezes por volta, com números de registrador constantes, como
  no código gerado, e com o estado em memória, como nos objetos do VU1.
- **Latência e vazão:** os testes `_lat` encadeiam cada resultado no próximo; os `_thr` usam passos independentes.
- **Orçamento:** o orçamento de ciclos é lido em tempo de execução, para o compilador não eliminar as checagens.
- **Exatidão:** `instr_fmac_x10` e `instr_fmac_x2` forçam 1 em 10 e 1 em 2 operações pelo caminho exato (`double`).
  A proporção real no jogo ainda não é conhecida (medição futura).

Testes: `loop`, `budget`, `rmax2`, `rmax_vf`, `instr_vi`, `vnorm_thr`, `vnorm_lat`, `writevf`, `ldst`,
`movemask_thr`, `movemask_lat`, `muladd_thr`, `muladd_lat`, `fmaccore`, `fmac_fast`, `fmac_exact`, `pd_core`,
`instr_fmac`, `instr_fmac_x10`, `instr_fmac_x2`, `csr`.

### Método de medição
- Calibra o número de passos para ~300 ms por repetição e faz 9 repetições.
- Registra a mediana, o mínimo, o máximo e a dispersão em ns por operação.
- **Ciclos e instruções** vêm de `perf_event_open`, quando o kernel deixa. Senão, os ciclos são estimados pela
  frequência do sysfs e marcados como "est".
- O teste roda fixo num núcleo (`--cpu`). No aparelho são duas passadas, nos núcleos 3 e 2, com o governor em
  `performance`.

### Condições no PC (`mb-pc-2026-11-28.log` e `.csv`)
- Intel Xeon a 2,3 GHz, 4 vCPUs, carga ~0,1, núcleo 2, gcc 13.
- Sem contadores de hardware (contêiner), então os ciclos do PC não estão disponíveis.
- **Ruído:** a dispersão entre repetições foi de 2% a 25%, porque o contêiner tem vizinhos. Para comparar, use a
  mediana, nunca o mínimo.

## 5. Resultados brutos no PC (ns por operação, mediana de 9)

| Teste | PC | R36H |
|---|---|---|
| loop | 0,033 | — |
| budget | 0,146 | — |
| rmax2 | 0,545 | — |
| rmax_vf | 1,299 | — |
| instr_vi | 0,875 | — |
| vnorm_thr | 0,923 | — |
| vnorm_lat | 1,830 | — |
| writevf | 0,524 | — |
| ldst | 0,494 | — |
| movemask_thr | 0,253 | — |
| movemask_lat | 3,994 | — |
| muladd_thr | 0,231 | — |
| muladd_lat | 1,602 | — |
| fmaccore | 1,917 | — |
| fmac_fast | 4,857 | — |
| fmac_exact | 12,178 | — |
| pd_core | 1,082 | — |
| instr_fmac | 6,075 | — |
| instr_fmac_x10 | 7,015 | — |
| instr_fmac_x2 | 10,309 | — |
| csr | 7,832 | — |

**Checagem de coerência no PC:** no jogo, o VU1 custa 4,1 ns por ciclo do VU. Um par de instruções (FMAC +
inteira) no microbenchmark custa ~6,1 + 0,9 = 7,0 ns. A diferença é esperada, porque os ciclos do jogo incluem esperas
(ciclos sem instrução nova) e nem toda instrução superior é uma conta com flags. A ordem de grandeza bate.

O binário de ARM foi conferido no qemu: roda, mas o tempo do qemu não vale como medida.

## 6. O que a rodada no aparelho vai responder (itens 4 e 5)

- **Razão A35/PC por teste.** Se `muladd_thr` e `ldst` ficarem na faixa de ~20x, como o código escalar, e
  `movemask_lat`, `fmaccore` e `instr_fmac` muito acima disso, o problema está nas dependências NEON ↔ escalar, não na
  conta em si.
- **IPC.** Se o `perf` funcionar, IPC baixo em `instr_fmac` aponta travas de dependência e latência; IPC normal com
  muitas instruções aponta excesso de instruções.
- **Custo do caminho exato** (`fmac_exact` contra `fmac_fast`) e do modo de arredondamento (`csr`). Para o modo de
  arredondamento: 572 execuções por field × 2 trocas.
- **Contabilidade de ciclos** (`budget`, `rmax2`, `rmax_vf`, `instr_vi`): quanto do custo de uma instrução é só
  contagem de tempo.
- **Modelo final:** com os ns por operação do A35 e a mistura de instruções do jogo (contagem futura por tipo, no
  binário de medição), dá para estimar quantos dos ~134 ms vêm de cada categoria.
