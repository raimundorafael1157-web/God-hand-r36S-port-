# Inventário do que o God Hand desenha

Primeiro passo do caminho com renderizador próprio. Gravado em 26/11/2026 na primeira luta da fase 1 (save state
`stage1.state`), 60 quadros do jogo (2 segundos), com `PS2X_GS_RECORD` e lido com `gs_replay --text`
(patch `0007` faz ele compilar no Linux). Números por quadro, mediana dos 60.

## O quadro

| Medida | Por quadro | O que significa para um renderizador em GLES |
| --- | --- | --- |
| Triângulos | ~14.100 (95% em strips) | Pouco para qualquer GPU atual, inclusive a Mali-G31 |
| Lotes com o mesmo estado | ~518 | Viram chamadas de desenho. No Cortex-A35 cada uma custa caro no driver; agrupar por estado é obrigatório |
| Estados diferentes | ~250 | Teto de quanto dá para agrupar sem mudar o jogo |
| Alvos de renderização | 7 | Cada troca de alvo numa GPU de tiles (Mali) grava o tile na memória: poucos e pequenos, aceitável |
| Texturas usadas | 120 | |
| Triângulos com transparência | ~99% | Custo de preenchimento; em 512x448 ou 640x480 é administrável |
| Envios de textura | 345 (1,9 MB) | **100% repetem conteúdo de quadros anteriores**: com cache por conteúdo, quase zero |

## Por alvo de renderização (FBP)

| Alvo | Triângulos | Lotes | Provável papel (a confirmar) |
| --- | --- | --- | --- |
| 0x70 | 13.879 | 402 | Buffer de desenho: a cena 3D inteira |
| 0x140 | 92 | 92 | Efeito com um lote por sprite |
| 0x1d0, 0x1d8 | 51 cada | 2 cada | Buffers pequenos de efeito (brilho, desfoque) |
| 0x0 | 11 | 10 | Tela exibida: cópia final (o "flip copy") e interface |
| 0x160, 0x1c8 | 8 e 3 | 8 e 2 | Efeitos pequenos |

## Conclusões

- **Geometria não é o problema.** 14 mil triângulos por quadro a 30 FPS cabem na Mali-G31.
- **Chamadas de desenho e o processador são o risco.** ~400–500 por quadro, vindas de um A35 que já está ocupado
  com a lógica do jogo.
- **Texturas: cache resolve.** O jogo reenvia tudo todo quadro, mas o conteúdo não muda.
- **VU1**: 5 programas cobrem a abertura e a luta (capturados nesta sessão). Ainda não se sabe o que cada um faz.
- **Decompilação**: as funções gráficas mais quentes (`func_002B4BC0`, `func_0030B870`, `func_0030AC40`) ainda
  não têm nome no `god-hand-decomp`.

## Próximos passos possíveis

1. **Reprodução na GPU do R36S**: um renderizador GLES mínimo que reproduz esta gravação (`fight.gsr`), com cache
   de texturas e agrupamento por estado. Mede o custo da GPU e do driver no aparelho, separado da lógica do jogo, e
   é o primeiro pedaço de um renderizador real.
2. Entender os 5 programas de VU1 (transformação, pele dos personagens, iluminação?).
3. Mapear, no `god-hand-decomp`, as funções que montam as listas de desenho.

A gravação contém texturas do jogo: fica só na máquina de quem tem o disco, nunca no repositório.

Ferramentas: `tools/gs_stats.py` (resumo por quadro) e `tools/gs_targets.py` (por alvo e repetição de texturas).
