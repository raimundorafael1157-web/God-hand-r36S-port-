# Fase 1: resultados dos microbenchmarks do VU1 (R36H x PC)

Data: 28/11/2026. **Só medição: nada foi otimizado e o runtime, o A e o B não mudaram.** Estes números são custos
por operação isolada, **não** são tempo de emulação. Metodologia: `fase1-vu1-metodologia.md`.

## Condições

| | R36H | PC |
|---|---|---|
| CPU | Cortex-A35 (part 0xd04 r2), 1512 MHz fixo, governor `performance` | Xeon 2,3 GHz (contêiner, 4 vCPU) |
| Sistema | ArkOS, kernel 4.4.189 | Linux 6.18 |
| Compilador | clang 22 (zig), `-mcpu=cortex_a35 -O3`, sse2neon v1.8.0 | gcc 13, `-O3 -msse4.1` |
| Contadores | `perf_event_open` ok (ciclos e instruções reais) | indisponível |
| Núcleos | passada 1 no núcleo 3, passada 2 no núcleo 2 | núcleo 2 |
| Temperatura | 47 °C antes, 55 °C no fim | — |
| Repetição | 9 × ~300 ms por teste, mediana | idem |
| Binário | `vu1_microbench` sha256 `569a2afc…` (o mesmo do pacote) | — |

**Estabilidade:**
- No aparelho, a dispersão entre repetições ficou abaixo de 0,5% em todos os testes menos `budget` (até 3,6%).
- As duas passadas (núcleos 3 e 2) diferem em ~1,5–1,8%, todas no mesmo sentido.
- No PC, a dispersão foi de 2–25%, por causa do contêiner. Por isso a razão A35/PC tem incerteza de alguns por cento;
  os números do aparelho, sozinhos, são precisos.

Dados brutos: `mb-r36h-2026-11-28.log`, `mb-r36h-2026-11-28-cpu3.csv`, `mb-r36h-2026-11-28-cpu2.csv`,
`mb-pc-2026-11-28.log` e `mb-pc-2026-11-28.csv`.

## Resultados (mediana, núcleo 3)

| Teste | PC ns/op | A35 ns/op (núcleo 3) | A35 ns/op (núcleo 2) | A35 ciclos/op | A35 instr/op | A35 IPC | A35/PC |
|---|---|---|---|---|---|---|---|
| loop | 0,033 | 0,39 | 0,39 | 0,5 | 0,5 | 1,00 | 11,8x |
| budget | 0,146 | 3,49 | 3,56 | 4,5 | 3,4 | 0,75 | 24,0x |
| rmax2 | 0,545 | 11,11 | 11,30 | 14,4 | 14,4 | 1,00 | 20,4x |
| rmax_vf | 1,299 | 10,82 | 11,01 | 14,0 | 14,0 | 1,00 | 8,3x |
| instr_vi | 0,875 | 11,04 | 11,27 | 14,3 | 14,2 | 1,00 | 12,6x |
| vnorm_thr | 0,923 | 14,59 | 14,84 | 18,9 | 9,0 | 0,48 | 15,8x |
| vnorm_lat | 1,830 | 9,47 | 9,63 | 12,2 | 6,4 | 0,52 | 5,2x |
| writevf | 0,524 | 7,63 | 7,76 | 9,9 | 5,9 | 0,59 | 14,6x |
| ldst | 0,494 | 11,79 | 11,99 | 15,2 | 8,9 | 0,58 | 23,9x |
| movemask_thr | 0,253 | 10,15 | 10,32 | 13,1 | 7,6 | 0,58 | **40,1x** |
| movemask_lat | 3,994 | 16,43 | 16,70 | 21,2 | 8,4 | 0,39 | 4,1x |
| muladd_thr | 0,231 | 3,29 | 3,34 | 4,2 | 2,4 | 0,56 | 14,2x |
| muladd_lat | 1,603 | 6,18 | 6,29 | 8,0 | 2,4 | 0,30 | 3,9x |
| fmaccore | 1,917 | 36,82 | 37,44 | 47,6 | 24,6 | 0,52 | 19,2x |
| fmac_fast | 4,857 | 133,29 | 135,57 | 172,4 | 114,8 | 0,67 | **27,4x** |
| fmac_exact | 12,178 | 237,38 | 241,45 | 307,0 | 230,4 | 0,75 | 19,5x |
| pd_core | 1,082 | 22,13 | 22,50 | 28,6 | 18,9 | 0,66 | 20,4x |
| instr_fmac | 6,075 | 156,48 | 159,18 | 202,4 | 135,5 | 0,67 | **25,8x** |
| instr_fmac_x10 | 7,016 | 174,05 | 177,09 | 225,1 | 152,9 | 0,68 | 24,8x |
| instr_fmac_x2 | 10,309 | 216,81 | 220,57 | 280,4 | 199,1 | 0,71 | 21,0x |
| csr | 7,832 | 44,07 | 44,80 | 57,0 | 49,4 | 0,87 | 5,6x |

## Gargalos identificados e quantificados

### 1. Uma instrução FMAC custa ~202 ciclos no A35, e a maior parte não é a conta

A decomposição sai da diferença entre os testes, em ciclos medidos pelo perf:

| Parte da instrução FMAC (`instr_fmac`) | Ciclos | % |
|---|---|---|
| A conta e as checagens de faixa (`fmaccore`) | 47,6 | 24% |
| Flags, empilhamento no anel, leituras `nv`/`nbc`/`nacc` e `writeVf` (`fmac_fast − fmaccore`) | 124,8 | **62%** |
| Contabilidade de ciclos: orçamento, 5 `rmax`, `setReady4` (`instr_fmac − fmac_fast`) | 30,0 | 15% |
| **Total** | **202,4** | |

**A parte das flags e do anel é a maior.** Dela, pelos testes isolados:
- 3 a 4 `movemask` de ~13 ciclos cada: ~40–52 ciclos;
- `writeVf`: ~10 ciclos;
- o resto, ~60–75 ciclos, são o empilhamento no anel, a tabela `kRev4`, os desvios e as leituras dos registradores.

Essa última fatia é inferida por subtração, não foi medida sozinha. Para separá-la seria preciso mais um teste: só o
empilhamento no anel.

### 2. `movemask` é a principal ineficiência de tradução

- **No PC:** 1 instrução, 0,25 ns.
- **No A35:** 7,6 instruções, 13 ciclos (sse2neon: 2 leituras de tabela, `ushl`, `and`, `addv`, `fmov`). É 40 vezes o
  PC, o pior caso de toda a lista.
- **Origem:** em parte arquitetura (passagem NEON → registrador comum e `addv` lentos), em parte tradução (8
  instruções onde o x86 tem 1).

### 3. O caminho exato (`double`) custa +105 ciclos por conta

- `fmac_exact` gasta 307 ciclos contra 172 do `fmac_fast`.
- Cada 10% de contas pelo caminho exato acrescentam ~23 ciclos à instrução média (`instr_fmac_x10`: 225 contra 202),
  ou +11%.
- **Ainda não se sabe que fração das contas do jogo cai nesse caminho.** É uma contagem que falta (ver o fim).

### 4. Contabilidade de ciclos: barata por instrução, mas está em todas

- Uma instrução inteira (`instr_vi`) é **só** contabilidade e escrita: 14,3 ciclos com IPC 1,0, ou seja, 14
  instruções executadas uma por ciclo.
- Numa FMAC, a contabilidade é 15% do custo.
- No código gerado são 10.967 chamadas a `rmax`, e toda instrução passa por uma checagem de orçamento.

### 5. O que não é gargalo

- **Modo de arredondamento** (`csr`): 57 ciclos por entrada e saída. Mesmo que as 572 execuções do VU1 por field
  trocassem o modo, dariam ~25 µs por field: **~0,02% dos 134 ms**. Desprezível.
- **A conta em si** (`muladd`):
  - a vazão é 14x o PC: clock 1,5x menor, NEON de 64 bits (2 ciclos por operação de 128 bits: 4,25 ciclos para mul +
    add) e execução em ordem;
  - a latência é só 3,9x o PC (8 ciclos para mul + add, 4 + 4).
  - É arquitetura pura, e não há o que traduzir melhor.

## Arquitetura x tradução

| Categoria | A35/PC | Causa principal |
|---|---|---|
| Conta SIMD (`muladd_thr`) | 14x | **Arquitetura:** clock, NEON de 64 bits, em ordem |
| Latência de conta (`muladd_lat`, `vnorm_lat`) | 4–5x | **Arquitetura:** latências parecidas, clock menor |
| Contabilidade escalar (`rmax2`, `instr_vi`, `budget`) | 13–24x | **Arquitetura e quantidade de instruções:** IPC 1,0, nenhuma trava, só instruções demais por instrução do VU |
| Leitura e escrita na memória do VU (`ldst`) | 24x | **Arquitetura:** latência de load em núcleo em ordem (IPC 0,58) |
| **`movemask`** | **40x** | **Tradução** (8 instruções contra 1) **e arquitetura** (NEON → escalar) |
| **FMAC completa** | **26x** | **Soma das anteriores: ~62% do custo em flags, anel e escrita, não na conta** |

## Conferência com o jogo

- **No jogo:** ~134 ms por field / ~929 mil ciclos do VU (contagem do PC, mesmo save state) ≈ **144 ns por ciclo do
  VU no A35**.
- **No microbenchmark:** uma FMAC mais uma instrução inteira dá 156 + 11 = **167 ns**. A ordem de grandeza bate: o custo
  do VU1 no jogo é explicado pelo custo por instrução medido aqui, sem precisar de outra causa escondida.
- **A diferença de razão:**
  - no jogo, A35/PC dá ~35x (134 / 3,8 ms, neste PC);
  - nos microbenchmarks, ~26x;
  - **causas prováveis, não verificadas:** o núcleo fora de ordem do PC sobrepõe mais as instruções do código real, e o
    código gerado do VU1 é grande (123 mil linhas, 5 microprogramas), então pode sofrer falhas de cache de
    instruções no A35 (L1I de 32 KB). Os microbenchmarks cabem no L1.

## Mistura de instruções no código gerado

Contagem estática, não dinâmica:
- 5.872 pares de instruções, com 2.025 contas FMAC.
- Das FMAC:
  - 1.242 usam `fmac<>`, com flags empilhadas no anel a cada conta;
  - 692 usam `fmacV<>`, com flags acumuladas em vetor, sem anel a cada conta;
  - 91 usam `fmacF<>`.
- Tipos: 700 MADD, 688 MUL, 217 ADD, 197 SUB, 70 OPMULA, 62 OPMSUB.

## Oportunidades com impacto mensurável no FPS (não aplicadas)

As estimativas valem por instrução FMAC; o impacto no fps depende da mistura dinâmica, que ainda não foi medida.

| # | Oportunidade | Ganho estimado por FMAC | Risco | Toca em |
|---|---|---|---|---|
| 1 | **Flags sob demanda:** não calcular nem empilhar as flags de contas cujas flags nenhuma instrução lê (item 1 da `analise-otimizacoes-cpu.md`) | até ~60% (os ~125 ciclos da parte de flags e anel, menos o `writeVf`) nas FMAC sem leitor | Médio: análise de vivacidade no gerador do VU1 | Gerador do VU1 (**precisa da sua autorização**) |
| 2 | **Extrair as flags com NEON nativo**, sem `_mm_movemask_ps` (as 4 máscaras de uma vez, com uma só passagem para o registrador comum) | ~25–40 ciclos (~12–20%) | Baixo: resultado bit a bit igual, verificável | Funções auxiliares do VU1 |
| 3 | **Contabilidade de ciclos por bloco** em vez de por instrução, nos trechos sem desvio nem espera possível | até ~15% na FMAC e grande parte da instrução inteira | Médio: o tempo do VU muda se feito errado | Gerador do VU1 |
| 4 | **Caminho exato:** só vale mexer se a contagem mostrar que ele é frequente | +11% por cada 10% de uso | — | Medir antes |
| — | Modo de arredondamento | ~0,02% | — | **Não vale a pena** |

**Teto de referência:** mesmo que os itens 1 a 3 cortassem pela metade o custo do VU1 (134 → ~67 ms por field), a
GameThread cairia de ~252 para ~185 ms por field: de ~2 para ~2,7 fps. O VU1 é o maior gargalo e vale atacar, mas
sozinho não leva a 30 fps. Isso confirma a conclusão do relatório de 27/11.

## O que falta medir (próxima rodada, no binário de medição, fora do A e do B)

1. **Contagem dinâmica por tipo de instrução do VU1 no jogo:**
   - quantas FMAC por field, com `fmac` (anel) ou `fmacV`;
   - quantas pelo caminho exato;
   - quantas flags empilhadas são lidas de fato.
   Isso fecha o modelo e dá o ganho real dos itens 1, 2 e 4.
2. **O custo do empilhamento no anel, isolado:** um teste a mais no microbenchmark.
3. **Falhas de cache de instruções** no VU1 real (`perf` com evento de L1I, se o kernel expuser), para explicar a
   diferença entre 26x e 35x.
