# Rodada M: binário de medição do VU1 e do GIF

Estado em 28/11/2026. **Só instrumentação: nenhuma otimização.**
- A e B não mudam (`godhand-A` 89a78cb4…, `godhand-B` 56db0e17…).
- O runtime de produção (`source/godhand-recomp`) não muda.

## O binário `godhand-M`

**Composição:** é o pacote B (0001–0010) com três patches de medição. Eles ficam em `patches/medicao/`, que o
`setup_base.sh` não aplica:

| Patch | O que mede |
|---|---|
| `M01-vu1-contadores.patch` | Execuções do VU1, ciclos do VU, código pré-compilado x interpretador, tempo por ciclo do VU (`[bench-vu1]`) |
| `M02-M03-vu1-caminhos-e-gif.patch` | **VU1:** chamadas de cada variante de FMAC, caminho exato (`double`), registros de flags empilhados, leituras de flags pelo programa, registros consumidos nas leituras, por estouro do anel e pendentes no fim, e chamadas de `writeVf`/`ldq`/`stq`/`div`/`clip`/`xgkick` (`[bench-vu1-paths]`). **GIF:** pacotes, bytes, tags por formato, registradores, vértices, e o tempo do `processGIFPacket` dividido em processamento, trava, envio ao GS, `Flush`, `Sync`, upload de imagem e outros (`[bench-gif]`) |

- **Quando os contadores funcionam:** só com `PS2X_BENCH=1 PS2X_PROF=1`. Fora disso, cada ponto de medição custa uma
  checagem de variável.
- **Exatidão:** as contagens são exatas, porque dependem do que o jogo executa e não do tempo.
- **Custo:** a instrumentação custa tempo, principalmente no VU1, com ~1,2 milhão de incrementos por field. Por isso a
  rodada intercala M com B:
  - **contagens e tempo do GIF:** de M;
  - **tempo real das seções:** de B;
  - **custo da instrumentação:** M menos B.

### Variantes de FMAC (já existentes no gerador do VU1)

| Variante | Flags |
|---|---|
| `fmac<>` | MAC/status calculados e empilhados no anel a cada conta |
| `fmacV<>` | Só os bits "sticky", acumulados em vetor |
| `fmacF<>` | Calculados e empilhados uma vez (última conta do segmento) |
| `fmacFastN<>` / `fmacFastV<>` | Sem flags / só sticky, pela análise de flags mortas do `vu1_recomp` |

O `fmac<>` chama o `fmacF<>` por dentro. O relatório já desconta isso ("fmacF direct").

### Como reconstruir

O build é isolado, numa cópia da base fora da árvore de produção.
1. Cópia da base em 14e956a (`git worktree add`), com `patches/0001…0010` e depois `M01` e `M02-M03`.
2. Links para `generated/`, `generated_modules/`, `platform/`, `renderer/` e `game_data/` da produção. São só leitura.
3. `cmake/` copiado, não como link: com link, `cmake/..` apontaria para a produção. Também `sse2neon.h`, que o
   `setup_base.sh` baixa.
4. `cmake` com as mesmas opções do `build_rk3326.sh`, compilando só `ps2_runtime`, `ps2_iop` e `godhand_vu1_code`.
   O código do jogo não inclui nenhum arquivo alterado.
5. Ligação com o mesmo comando do executável de produção (`ninja -t commands bin/godhand`), trocando só essas três
   bibliotecas pelas de medição. As do código do jogo são as mesmas do B.
6. Resultado: `godhand-M` strip com sha256 `c99b7c222babc6dbead78f359fcaa5623fff1a074276a0edda2e42ed04d525fc`.

### Validação feita
- **PC:** build x64 equivalente, luta de 60 s (`M-pc-60s.txt`) e o roteiro inteiro em versão curta, B1 e M1 de 40 s
  (`M-pc-validacao-*.txt`).
- **ARM:** o `godhand-M` foi conferido no qemu. Inicia e imprime as linhas novas (sem medir tempo).

## A rodada no aparelho (`scripts/run_meas.sh`, atalho "GodHand - 6 Medicao VU1 e GIF")
- **Ordem:** `B1 M1 M2 B2 B3 M3 M4 B4`, 240 s cada, intercalada para o custo da instrumentação não se confundir com
  aquecimento.
- **Condições:** as mesmas do A/B (mesmo save, nenhum botão, GS nulo, `PS2X_PROF=1`, governors em `performance`,
  60 s de pausa e espera térmica).
- **Conferência:** o hash de `godhand-B`, `stage1.state` e `godhand-M` é checado antes de começar.
- **Saída:** `meas-resumo-*.txt` (tabela por execução), `meas-detalhe-*.txt` (as linhas de medição de cada execução),
  `meas-execucao-*.txt` e os `bench-cpu-*.log`.

## Primeiros números no PC (só validação, não valem para o A35)
Luta da fase 1, `godhand-M`, 35 s medidos:
- **Volume:** **547 mil contas FMAC por field**, para 951 mil ciclos do VU.
- **Variantes:**
  - `fmacV`: 68%;
  - `fmac` com anel: 27%;
  - `fmacF` direta: 5%;
  - `fmacFastN`: 0. Os 143 usos no código gerado não rodam nesta luta.
- **Caminho em `double`:** **0,65%** das contas.
- **Flags:**
  - **174 mil registros** empilhados por field;
  - **14,6 mil leituras** de flags pelo programa (`commitStatus`), que consomem 164 mil registros;
  - 9,3 mil saem por estouro do anel e 3,3 mil ficam pendentes no fim.
- **Memória do VU:** `ldq` 262 mil, `stq` 107 mil, `div` 17 mil, `clip` 32 mil, `xgkick` 734 por field.
- **GIF:** 1.290 pacotes e 1,44 MB por field, 16 mil vértices, 1,13 ms. Divisão: processamento 55%, envio ao GS 37%,
  trava 3%, `Flush` 2%, `Sync` 0%.
- **Custo da instrumentação no PC:** GameThread de 15,3 (B) para 17,4 ms por field (M), +14%. Quase todo no VU1.
