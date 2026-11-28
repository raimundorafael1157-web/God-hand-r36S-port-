# Relatório do primeiro teste no R36H (27/11/2026)

## Resumo curto

- **O teste foi válido.** Os 8 runs terminaram e o save state da luta carregou em todos (em t=15 s). Nada travou e
  não houve throttling: CPU a 1512 MHz o tempo todo, com máximo de 79 °C.
- **O jogo roda a ~2 fps (7% da velocidade do PS2).** A GameThread gasta ~252 ms por field. Para 30 fps (100%)
  precisaria caber em 16,7 ms, ou seja, ficar **~15x mais rápida**.
- **O pacote B funcionou onde devia:** a GSThread caiu **24,5%** (48,8 → 36,9 ms por field). Mas o fps não muda,
  porque quem limita é a GameThread, não a GS.
- **O VU1 é o vilão:** 53% da GameThread (~134 ms por field). Proporcionalmente ele está muito pior no A35 do que
  as outras seções (ver tabela 3).
- **A GPU (Mali-G31) está folgada:** 3,8 ms de GPU por quadro no replay da luta. O problema do replay GLES é o
  lado da CPU (~1,6 s por quadro), não a Mali.
- **As "ANOMALIAS" eram alarme falso:** o `grep -i "ERROR"` do `run_ab.sh` casou com o nome da função
  `sceCdGetError`, que aparece na lista de hooks de todo log. A coluna `erros` do resumo está zerada.

## 1. Aparelho (hw.txt)

| Item | Valor |
|---|---|
| Modelo (device tree) | Game Console R35S/R36S fix by lxc (`rockchip,rk3326-odroidgo3-linux`) |
| Sistema | Ubuntu 19.10 (base do ArkOS), kernel 4.4.189, glibc 2.30 |
| CPU | 4x Cortex-A35, governor `performance`, 1512 MHz |
| RAM | 897 MB, sem swap e sem zram |
| Memória (dmc) | 786 MHz, `performance` |
| GPU | Mali-G31, 520 MHz, blob `r13p0` (OpenGL ES 3.2), `libmali-bifrost-g31-rxp0-wayland-gbm` |
| libgbm | existe no sistema (os `stubs-gles` não são necessários neste firmware) |
| Tela | 640x480, **78 Hz** (`640x480p78`) |
| Hash de páginas (`--hash-bench`) | atual 3,40 ms/quadro, xxh3 3,16 ms/quadro (−7%) |

Obs.: o relógio do aparelho marca 06/11/2021. Isso só afeta os nomes dos arquivos.

## 2. Teste A/B (4 runs de 300 s por pacote, ordem A B B A A B B A)

| Métrica | Pacote | Média | Mediana | Desvio | Melhor | Pior |
|---|---|---|---|---|---|---|
| Game_ms (por field) | A | 252,39 | 252,39 | 0,12 | 252,27 | 252,52 |
| | B | 253,41 | 253,45 | 0,38 | 252,95 | 253,77 |
| GS_ms (por field) | A | 48,81 | 48,82 | 0,02 | 48,78 | 48,83 |
| | B | 36,87 | 36,88 | 0,04 | 36,82 | 36,90 |
| Fields em 300 s | A | 1096,3 | 1096,5 | 0,96 | 1097 | 1095 |
| | B | 1092,5 | 1092,0 | 1,91 | 1095 | 1091 |

Ganho `((A−B)/A)×100`:
- **GSThread: +24,5%** (B mais rápido). No PC tinha dado +29,4%. Confirmado no aparelho.
- **GameThread: −0,4%** (B um pouco mais lento, nos 4 pares). É pequeno, mas o ruído aqui é minúsculo (~0,1%),
  então provavelmente é real. A causa mais provável é o layout do código. Na prática não importa.
- **Velocidade/fps: igual** (7%, 2,0 fps nos dois).

Outros números: IOP ~86 ms por field (em outra thread, ~34% de um núcleo), RSS ~120 MB, pior intervalo entre
quadros ~530 ms. O custo da instrumentação (`PS2X_PROF=1`) medido pelo próprio log foi ~3,4%.

**Ruído no aparelho:** desvio de ~0,1% na GameThread, muito melhor que o ±9% do PC. Testes A/B futuros no
aparelho conseguem enxergar ganhos de 1%.

## 3. Onde a GameThread gasta o tempo (luta, pacote A)

| Seção | % da GameThread | ms por field no R36H | ms por field no PC | R36H / PC |
|---|---|---|---|---|
| vu1 | 53,2% | ~134 | ~2,4 | **~55x** |
| ee | 23,0% | ~58 | ~2,6 | ~22x |
| dispatch | 8,8% | ~22 | ~1,3 | ~18x |
| gif | 8,6% | ~22 | ~0,4 | **~52x** |
| sched | 2,8% | ~7 | ~0,4 | ~17x |
| vif | 2,8% | ~7 | ~0,2 | ~33x |
| iop | 0,8% | ~2 | ~0,1 | — |
| idle | 0% | 0 | ~3,0 | — |

O código escalar (ee, dispatch, sched) está ~17–22x mais lento que no PC. Isso é o esperado para um A35 em ordem a
1,5 GHz contra um desktop. Já **o VU1 e o GIF estão ~50x mais lentos**, o dobro dessa proporção. Os dois são
código SIMD (SSE traduzido para NEON pelo sse2neon). Hipóteses, ainda **não verificadas**:
- o NEON do A35 processa 128 bits em 2 ciclos (metade do throughput);
- alguma intrínseca do SSE vira uma sequência longa no sse2neon (por exemplo, mudança de arredondamento via
  FPCR, `movemask`, `dp_ps`, clamps);
- falta de cache (L1 de 32 KB, L2 compartilhado).

## 4. Teste da GPU (replay de `fight.gsr`, 2 execuções, 174 quadros)

| Métrica (mediana por quadro) | Execução 1 | Execução 2 |
|---|---|---|
| Tempo de GPU (`glFinish`) | 3,82 ms | 3,81 ms |
| Tempo de CPU | 1630 ms | 1627 ms |
| Chamadas de desenho | 433 | 433 |
| Triângulos | 14.208 | 14.208 |
| Desenhos na CPU (DOF) | 12 | 12 |
| Readbacks | 14 | 14 |
| Uploads | ~625–685 | ~625–685 |
| Chamadas de estado GL | 1150 | 1150 |

- **A Mali dá conta:** 3,8 ms de GPU com 433 desenhos. O medo de 30–42 µs por desenho (do NFSHP2) não aparece aqui
  no tempo de GPU.
- **O lado da CPU do replay está patológico:** 1,6 s por quadro, enquanto o rasterizador de CPU inteiro do jogo
  gasta 49 ms por field. O log não separa esse tempo por categoria. Os suspeitos são os 14 `glReadPixels`
  (sincronizam com a GPU e podem converter formato), os 12 desenhos de DOF na CPU e os ~650 uploads de textura por
  quadro. **Precisa de cronômetro por categoria antes de qualquer conclusão.**
- Os dois resultados bateram entre si (diferença < 0,2%), o que mostra que a medição é estável.

## 5. O que dá para concluir sobre 30 fps

- Hoje: 252 ms por field. Meta: 16,7 ms. Falta cortar **~94%** da GameThread.
- Mesmo que o VU1 caísse para a proporção "normal" do A35 (~20x o PC, ~50 ms), a GameThread ficaria em ~170 ms:
  ~10% de velocidade, ~3 fps.
- Ou seja: **ajustes de build (LTO, PGO, logs) e de tempo não chegam nem perto.** Cada um vale poucos por cento.
  Os 30 fps só seriam possíveis com mudanças de outra escala, por exemplo:
  - tirar o VU1 do caminho, com um renderizador nativo que lê as estruturas do jogo (o caminho do Celosia, de meses
    de trabalho);
  - ou uma reescrita do VU1/GIF focada em NEON.
  
  As duas são decisões arquiteturais e precisam da sua autorização.

## 6. Próximos passos sugeridos (só medição, conforme a regra)

1. **Corrigir o falso alarme do `run_ab.sh`:** o padrão `ERROR` casa com `sceCdGetError`. Trocar por
   `\bERROR\b` e usar `grep -a`, porque o NUL do `/proc/device-tree/model` faz o grep tratar o log como binário.
2. **Descobrir por que o VU1 está ~55x mais lento:**
   - perfil de amostragem no aparelho (`perf`, se existir no ArkOS, ou um amostrador simples por sinal);
   - microbenchmark das intrínsecas do sse2neon mais usadas pelo VU1, no A35.
3. **Cronômetros por categoria no `gles_replay`** (readback, desenho na CPU, upload, hash, chamadas GL), para
   explicar o 1,6 s.
4. Medir o `sceGsExecStoreImage` nas cutscenes, que continua pendente.

Itens já respondidos por este teste: taxa do painel (78 Hz), presença de libgbm, throttling (nenhum) e custo de GPU
por desenho (baixo).
