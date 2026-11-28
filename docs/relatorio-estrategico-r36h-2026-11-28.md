# God Hand no R36H: gargalos e estratégia de otimização

Relatório de 28/11/2026. Base:
- teste A/B de 27/11;
- microbenchmarks do VU1;
- rodada de medição M (B × M intercalados, 8 × 240 s), em `docs/medicoes/rodada-M-r36h/`.

**Nada foi implementado. Nenhuma proposta abaixo será aplicada sem autorização.**

## 1. Resumo para decisão

- **Hoje:**
  - a luta roda a **2,0 fps** (7% da velocidade do PS2);
  - a **GameThread** é o único limite: 100% ocupada, idle 0;
  - ela gasta **256,9 ms por field**, e a meta de velocidade cheia é 16,7 ms (**15,4x**).
- **Onde está o tempo da GameThread (por field):**
  - **VU1:** 137 ms (53%);
  - **código do jogo (EE):** 88 ms (34%), sendo ee 59 + dispatch 22 + sched 7;
  - **GIF:** 22 ms (9%);
  - **VIF:** 7 ms (3%).
  - GPU, memória, sincronização e as outras threads **não limitam hoje**.
- **As otimizações de baixo e médio risco levam a ~2,4–2,5 fps (−15 a −20%).**
  - **Somando o VU1 em outra thread** (opção que já existe no runtime, ainda não testada no aparelho), o teto teórico
    fica em ~3,7–4,5 fps.
  - **30 fps na luta não são alcançáveis com otimizações incrementais nesta base.** Seriam necessários um renderizador
    nativo no lugar do VU1 e do GIF, como fez o Celosia, e reescrever partes da lógica do jogo. É trabalho de meses.
  - Mesmo assim, só o EE já gasta 88 ms por field, 5x a meta.
- **Próximo passo de maior valor, sem escrever código:** medir o B com `PS2X_MTVU=1` no aparelho. É a única alavanca
  que pode passar de 2x, e o binário já tem a opção.

## 2. Condições da medição

| Item | Valor |
|---|---|
| Aparelho | R36H, ArkOS, kernel 4.4.189, 4× Cortex-A35 a 1512 MHz fixo (`performance`), memória a 786 MHz |
| Cena | Primeira luta da fase 1 (`stage1.state`), nenhum botão, GS nulo (sem desenhar), `PS2X_PROF=1` |
| Ordem | B1 M1 M2 B2 B3 M3 M4 B4, 240 s cada (215 s medidos), 60 s de pausa + espera térmica |
| Temperatura | 59–75 °C, **nenhum segundo de throttling** |
| Binários | `godhand-B` 56db0e17… (o B de 27/11, sem mudança) e `godhand-M` c99b7c22… (B + medição) |
| Observação | O atalho foi acionado mais de uma vez. As outras tentativas terminaram em segundos, em "save não encontrado", sem medir nada. Uma delas começou às 10:40:29, já com a B1 rodando. A B1 deu 256,06 ms, igual às outras execuções de B, sem sinal de interferência |

### Estabilidade e custo da instrumentação

| | B (produção) | M (medição) |
|---|---|---|
| Game_ms por field (média de 4) | **256,87** (desvio 0,64 = 0,25%) | 346,60 (desvio 1,76) |
| fps | 2,0 | 1,6 |

**Custo da instrumentação no A35: +34,9%** na GameThread, contra +14% no PC.

| Seção | B (ms/field) | M (ms/field) | Diferença |
|---|---|---|---|
| VU1 | 137,3 | 208,3 | **+71,0** |
| GIF | 22,3 | 31,8 | +9,5 |
| dispatch | 22,0 | 28,2 | +6,1 |
| ee | 59,1 | 60,1 | +1,1 |

- **Causa:** no VU1, ~2,1 milhões de incrementos de contador por field, ~33 ns cada. No GIF, ~28 mil leituras do
  relógio de hardware, que custam ~160 ns cada no A35.
- **Consequência:**
  - **as contagens de M são exatas**: dependem do que o jogo executa, e as 4 execuções de M bateram com variação abaixo
    de 0,3%;
  - **todos os tempos deste relatório vêm de B**;
  - a divisão do tempo do GIF vem de M e está **inflada pelo próprio cronômetro**, então vale como proporção
    aproximada, não como valor absoluto.

## 3. Mapa completo dos gargalos (GameThread, por field, binário B)

| Seção | % | ms | Observação |
|---|---|---|---|
| **vu1** | 53,5 | **137,3** | 948 execuções do VU1, **1,49 milhão de ciclos do VU**, 100% pelo código pré-compilado → **92 ns (139 ciclos do A35) por ciclo do VU** |
| **ee** | 23,0 | 59,1 | Código do jogo recompilado |
| **dispatch** | 8,6 | 22,0 | Chamadas e saltos entre funções recompiladas |
| **gif** | 8,7 | 22,3 | 2.050 pacotes, 2,26 MB, 26,1 mil vértices |
| vif | 2,8 | 7,2 | Descompactação para a memória do VU1 |
| sched | 2,7 | 7,0 | Agendador do EE |
| iop | 0,7 | 1,9 | Parte do IOP na GameThread |
| idle / gs-wait / vu-wait | 0 / 0 / 0 | 0 | **A GameThread nunca espera: é processamento puro** |
| **Total** | 100 | **256,9** | |

### Fora da GameThread (não limita hoje)

| Recurso | Medida | Leitura |
|---|---|---|
| GSThread (rasterizador de CPU) | 37,5 ms de CPU por field, em outro núcleo | Não limita hoje (gs-wait 0). **Vai limitar acima de ~13 fps**, e a 30 fps precisaria de ≤16,7 ms. A GPU entra aí |
| IOPThread | 87 ms de CPU por field (~34% de um núcleo) | Em núcleo próprio, não bloqueia a GameThread |
| CPU total | 153% de 400% | **~2,5 núcleos ociosos.** Espaço para paralelismo, só que a GameThread é sequencial |
| Memória | RSS 120 MB de 897 MB; controlador de memória 4% ocupado | **Não é gargalo**: nem capacidade nem banda |
| GPU (Mali-G31) | 3,8 ms por quadro no replay da luta (27/11) | **Folgada.** Hoje o jogo nem usa a GPU (GS nulo no teste, CPU no jogo real) |
| Sincronização | gs-wait 0, vu-wait 0, trava do GIF ~0,6 ms (~3% do GIF), `Sync` 0 | **Não é gargalo** |
| Temperatura | até 75 °C, sem throttling | Não limita |

### 3.1 VU1: o que roda por field (contagens exatas de M)

| Medida | Por field | Leitura |
|---|---|---|
| Contas FMAC | **850 mil** (0,57 por ciclo do VU) | É a operação dominante |
| — `fmacV` (flags só "sticky", em vetor) | 574 mil (67,6%) | Já é o caminho barato |
| — `fmac` (flags empilhadas no anel a cada conta) | **233 mil (27,4%)** | Caminho caro: ~172 ciclos no microbenchmark |
| — `fmacF` direta (última do segmento) | 42,6 mil (5,0%) | Precisa ficar exata |
| — sem flags (`fmacFastN`/`FastV`) | 0 | Existe no gerador, mas não roda nesta luta |
| Caminho exato (`double`) | **5,6 mil (0,66%)** | **Desprezível** |
| Registros de flags empilhados | 275,7 mil | |
| Leituras de flags pelo programa | **23,5 mil** | Só ~8,5% dos registros são o último antes de uma leitura; os demais só contribuem com bits "sticky" |
| Registros consumidos nas leituras / por estouro do anel / pendentes no fim | 264,6 mil / 11,1 mil / 5,4 mil | |
| LQ (`ldq`) / SQ (`stq`) | 397 mil / 174 mil | |
| DIV / CLIP / XGKICK | 27,6 mil / 54,6 mil / 1,2 mil | |
| Troca do modo de arredondamento | 0 | **Desprezível** |

**Modelo do custo do VU1**, com os ciclos dos microbenchmarks multiplicados pelas contagens:

| Parte | Ciclos | % dos 137 ms |
|---|---|---|
| FMAC com anel | ~47 milhões | |
| `fmacV` | ~34 milhões | |
| LQ/SQ | ~8 milhões | |
| Contabilidade de ciclos | ~12 milhões | |
| **Explicado** | **~100 milhões (~68 ms)** | **~50%** |
| **Sem atribuição** | | **~50%** |

- **Na parte sem atribuição** devem estar `vnorm`, operações inteiras, desvios, CLIP e DIV, e possivelmente falhas de
  cache de instruções. O código do VU1 é grande, e o L1I do A35 tem 32 KB.
- **Para atribuir essa metade** é preciso um perfil por amostragem no aparelho. Fica como medição recomendada.

### 3.2 GIF: onde vão os 22 ms (proporções de M, infladas pelo cronômetro)

| Parte | % (M) | Leitura |
|---|---|---|
| Processamento (tags, registradores, montagem de primitivas) | ~53% | ~0,44 µs (~660 ciclos) **por vértice**, 26,1 mil vértices |
| Envio de lotes à thread do GS (`Submit`) | ~41% | **7.085 envios por field, ~3,7 vértices por envio**: lotes muito pequenos |
| Trava / `Flush` / outros | ~6% | Pequeno |
| `Sync` (esperar o GS) / upload de imagem | 0 / 0 | Não acontecem nesta cena |

O GIF nunca espera o GS (gs-wait 0): o custo é **processamento e passagem de lotes**, não sincronização.

## 4. Propostas avaliadas pelo critério combinado

As estimativas usam a frequência medida no jogo e os ciclos dos microbenchmarks. Toda estimativa precisa de A/B no
aparelho antes de virar fato. Impacto no fps: hoje 2,0 fps = 256,9 ms.

### Maior potencial

| # | Proposta | Frequência no jogo | Custo atual | Economia potencial | Impacto estimado no fps | Riscos de precisão e compatibilidade | Custo de implementação |
|---|---|---|---|---|---|---|---|
| P1 | **VU1 em outra thread** (`PS2X_MTVU=1`, já existe) | Todo o VU1: 137 ms | 53% da GameThread | Teto: a GameThread ficaria em ~120 ms, com o VU1 em paralelo (137 ms) | **Teto ~3,7 fps.** Real desconhecido: no PC piorou (vu-wait 16,6%) | Médio: muda a ordem temporal entre EE e VU1, mas a opção já existe e é usada no Celosia | **Zero para medir** (variável de ambiente). Ajustar a sincronização é mudança arquitetural |
| P2 | **Flags mais baratas nas FMAC com anel** (mais análise de flags mortas no gerador do VU1) | 233 mil por field; só 23,5 mil leituras | ~172 contra ~60 ciclos | ~13–14 ms (os ~210 mil que não precedem leitura) | **−5,4% → ~2,11 fps** | Médio: os bits "sticky" e as leituras entre segmentos precisam ficar exatos. Verificável com `PS2X_VU1_VERIFY` e o hash do GS | Médio-alto: análise no `vu1_recomp`, regenerar os 5 programas |
| P3 | **Build: PGO no aparelho + LTO do runtime** | Todo o código | — | Celosia: +5,5% a +7,7% (PGO), +1,9% (LTO) | **−4 a −8% → ~2,1–2,2 fps** | Baixo: não muda a semântica; verificar com hash do GS | Médio: falta provar que o zig 0.17 gera e usa perfil em aarch64 |
| P4 | **EE: passagens no gerador** (`ctx->pc` só onde precisa, acesso direto à RAM com base constante) | ee + dispatch = 81 ms | 32% | ~15–25% do ee: −9 a −15 ms | **−4 a −6%** | Médio-alto: toca todo o código gerado | Alto: regenerar e revalidar o jogo inteiro |
| P5 | **Memória espelhada** (fastmem) | Todo acesso à memória no ee | Parte dos 59 ms | Incerto: −6 a −12 ms (Celosia: −10,7% num jogo sem VU1 pesado) | **−2 a −5%** | Médio-alto: tratador de sinal, decodificação de instruções | Médio |

### Baixo risco

| # | Proposta | Frequência no jogo | Custo atual | Economia potencial | Impacto estimado no fps | Riscos de precisão e compatibilidade | Custo de implementação |
|---|---|---|---|---|---|---|---|
| P6 | **`movemask` em NEON nativo** (no lugar do sse2neon) | ~1,7 milhão por field (1 por FMAC + 3 nas com anel ou F) | ~13 ciclos | ~8 ciclos cada (meta ~5, a validar): ~9 ms | **−3,5% → ~2,07 fps** | **Baixo:** resultado bit a bit igual, testável isoladamente | **Baixo:** uma função auxiliar, recompilar o VU1 |
| P7 | **GIF: lotes maiores para a thread do GS** (juntar os vértices do pacote antes do `Submit`) | 7.085 envios por field | ~9–10 ms | ~4–6 ms | **−2% → ~2,04 fps** | Baixo-médio: a ordem das primitivas fica igual; hash do GS idêntico | Baixo-médio |
| P8 | **Contabilidade de ciclos por bloco** | 1,49 milhão de ciclos do VU por field | ~12 milhões de ciclos (~8 ms) | ~4–6 ms | **−2%** | Médio: o tempo do VU muda se feito errado | Médio-alto (gerador) |

### Becos sem saída (não recomendados)

| Proposta | Por quê |
|---|---|
| Caminho exato (`double`) do VU1 | 5,6 mil por field × ~105 ciclos = **0,4 ms (0,15%)** |
| Modo de arredondamento (`csr`) | **0 trocas** no jogo |
| Desligar só os logs do runtime | Celosia: dentro do ruído. Só vale junto do PGO (P3) |
| `PS2X_IOP_SPIN_US` e ajustes do IOP | O IOP roda em outro núcleo e a GameThread não espera por ele: **não muda o fps** (pode mudar o consumo) |
| Memória / banda / zram | RSS 120 MB, controlador a 4%: **não é gargalo** |
| GPU, XXH3, DOF na GPU, cache de shader | **Não limitam a luta hoje**. Só passam a importar quando o jogo desenhar pela GPU e chegar perto de ~13 fps |
| Paralelizar a lógica do jogo em mais threads | Celosia: −2,7 fps no A35. E aqui o EE é sequencial por natureza |

## 5. Até onde dá para chegar

Estimativas cumulativas a partir de 256,9 ms (2,0 fps), sem garantia de que se somem:

| Cenário | GameThread | fps da luta |
|---|---|---|
| Hoje | 256,9 ms | 2,0 |
| Baixo risco (P3 + P6 + P7) | ~225–235 ms | ~2,2–2,3 |
| + médio risco (P2 + P8 + P5) | ~200–210 ms | ~2,4–2,6 |
| + P4 (gerador do EE) | ~185–195 ms | ~2,6–2,8 |
| + P1 com sobreposição real do VU1 | ~110–130 ms (limitado pela thread do VU1) | **~4–4,7 (teto otimista)** |
| **Meta 30 fps** | **16,7 ms** | **inalcançável por esse caminho** |

- **Para chegar perto de 30 fps:** o VU1 e o GIF teriam que sair do caminho, com um renderizador nativo que lê as
  estruturas do jogo, como o Celosia, e o código do EE precisaria ficar ~5x mais rápido com funções reescritas à mão.
  O Celosia fez isso em ~6 semanas para o SotC partindo de 25 fps, com símbolos e um jogo mais leve no VU1.
  Aqui partimos de 2 fps.
- **E depois, o GS:** o rasterizador de CPU (37,5 ms por field) também viraria limite; precisaria integrar o
  renderizador GLES no jogo.

## 6. Recomendação (nada disso aplicado)

1. **Medir primeiro, sem código novo:** B com `PS2X_MTVU=1` no aparelho, na mesma rodada de 8 execuções. Decide se a
   maior alavanca (P1) existe no A35.
2. **Medir a metade não atribuída do VU1:** perfil por amostragem no aparelho, por função e endereço. Decide se P2 e
   P6 são mesmo os maiores itens do VU1.
3. **Se você autorizar otimizações**, a ordem de menor risco por ganho é:
   1. P6 (`movemask`), um commit, A/B no aparelho;
   2. P7 (lotes do GIF);
   3. P3 (PGO/LTO, depois de provar o PGO com o zig);
   4. P2 e P8 (gerador do VU1);
   5. P5 e P4, por último.

   P1, se a medição for boa, é decisão sua por ser arquitetural.
4. **Decisão estratégica:** se o objetivo for o jogo **jogável** (25–30 fps), as otimizações incrementais não bastam.
   Vale decidir agora se o projeto segue como estudo de desempenho (melhor fps possível, com precisão) ou se parte para
   um caminho de renderizador nativo, de escala muito maior.

## 7. Dados brutos

- **Rodada M:** `docs/medicoes/rodada-M-r36h/`. Os 8 `bench-cpu-*.log`, `meas-resumo`, `meas-detalhe` e os 6
  `meas-execucao`, incluindo as tentativas que pararam em "save não encontrado".
- **Microbenchmarks:** `docs/medicoes/mb-r36h-2026-11-28*`.
- **Metodologia:** `docs/medicoes/rodada-M-metodologia.md`, `fase1-vu1-metodologia.md` e `fase1-vu1-resultados.md`.
- **Patches de medição:** `patches/medicao/`.
- **Scripts:** `scripts/run_meas.sh`, `tools/vu1_microbench/`.
