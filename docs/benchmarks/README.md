# Benchmarks no PC

Comparações feitas com `tools/bench_compare.py` (luta da fase 1, save state, GS nulo, 85 s por execução, 25 s de
aquecimento fora da conta, execuções de A e B intercaladas). Os resultados de cada comparação ficam aqui, em JSON.

## O que cada número é

- **speed:** fields por segundo / 59,94. O jogo segue o tempo real: no PC fica perto de 100% e só cai quando a CPU
  não dá conta.
- **ms/field por thread:** tempo de CPU da thread por field emulado. É o número que muda quando uma otimização
  funciona, mesmo com a velocidade presa em 100%.
- **gs:** hash da gravação do GS dos fields 5100–5219, que inclui hashes da VRAM. O EE é determinístico nesse
  trecho, então o hash tem de ser idêntico em todas as execuções; diferente = mudança de comportamento.
- **audio_per_field:** bytes de saída do SPU2 por field (48 kHz estéreo = ~3.203; acima disso entra o boot). Cair
  significa o IOP atrasando. O áudio não é idêntico byte a byte entre execuções (o IOP roda em tempo real), por isso
  é conferido por números. O RMS também é registrado, mas varia muito com o trecho (música ou silêncio) e não serve
  como critério.

## Referência (26/11/2026)

`2026-11-26-pc-referencia-aa.json`: o mesmo binário (patches 0001–0009, instrumentação desligada) dos dois lados,
4 execuções cada. Mede o ruído da máquina:

| Medida | A | B | B−A (intervalo ~95%) |
| --- | --- | --- | --- |
| Velocidade | 99,49% ± 0,32 | 99,35% ± 0,37 | −0,13 ± 0,47 |
| GameThread | 10,44 ms/field ± 0,71 | 10,63 ms/field ± 0,61 | +0,19 ± 0,91 |
| IOPThread | 8,54 ms/field ± 0,31 | 8,62 ms/field ± 0,17 | +0,09 ± 0,34 |
| GSThread | 2,12 ms/field ± 0,16 | 2,10 ms/field ± 0,17 | −0,02 ± 0,23 |
| Áudio | 3.271,7 B/field | 3.272,2 B/field | +0,5 ± 2,6 |
| GS | idêntico nas 8 execuções | | |

Conclusão: nesta VM, pelo tempo, só diferenças acima de ~9% na GameThread são detectáveis com 4 execuções. Ganhos
menores precisam de contadores exatos (como os do renderizador) ou do aparelho.

## Patch 0010 — upload rápido para a VRAM (26/11/2026)

`2026-11-26-pc-0010-gs-upload.json`: A = referência (0001–0009), B = com 0010, 4 execuções cada.

| Medida | A | B | B−A (intervalo ~95%) |
| --- | --- | --- | --- |
| GSThread | 2,20 ms/field ± 0,08 | 1,55 ms/field ± 0,13 | **−0,65 ± 0,15 (−29,4%)** |
| GameThread | 10,89 ms/field ± 0,26 | 11,41 ms/field ± 1,23 | +0,52 ± 1,23 (ruído) |
| Áudio | 3.273,5 B/field | 3.271,4 B/field | −2,1 ± 1,1 |
| GS | idêntico nas 8 execuções | | |

A média de B na GameThread sobe por uma única execução lenta da VM (jogo, IOP e GS subiram juntos e a velocidade
caiu para 96%); as outras três ficaram em 10,7–10,9 ms, como A. A mudança não toca a GameThread.
Equivalência: `renderer/gles/tests/gs_upload_test.cpp`, 520 casos (13 formatos, larguras ímpares, uploads cortados
em pedaços) com VRAM idêntica byte a byte à escrita pixel a pixel; no microbenchmark, de 7,2–8,5 para ~4,9–6,0
ns/pixel no x86. Ganho no Cortex-A35: a medir.
