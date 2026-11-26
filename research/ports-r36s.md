# Como os ports que rodam no R36S foram feitos

Levantado em 26/11/2026 no GitHub. Números são os que cada projeto publica, não medidos aqui.

## Quatro caminhos

| Caminho | Exemplos | Como funciona | Resultado no RK3326 |
| --- | --- | --- | --- |
| Decompilação + renderizador próprio | DBZ Budokai Tenkaichi 3/4 (PS2), Super Mario Strikers (GameCube), THUG (Xbox) | Código do jogo reescrito em C; gráficos redesenhados em OpenGL ES para a Mali, sem imitar a GPU original | Jogáveis. THUG: 25–40 FPS com sombras simplificadas e partículas desligadas. BT3 tem opção de pular quadros |
| Recompilação estática + GPU reimplementada | Pocket Native (3DS), NFSU1/NFSU2 (PC) | Código traduzido para ARM64 antes de rodar; a GPU original refeita em GLES (3DS) ou D3D9 traduzido para GLES 2 (NFSU) | 3DS: jogos leves a 30 FPS, 3D pesado lento. NFSU2: travado em 30 FPS |
| Carregador de bibliotecas Android | NFS Most Wanted 2012, Minecraft Bedrock, Ice Rage | Roda as bibliotecas ARM do APK original direto no Linux, trocando só as chamadas do Android | NFS MW: 48,8 FPS. É o caminho mais fácil: o código já é ARM e já usa GLES |
| Motor reimplementado ou tradução x86 | mkxp-z (RPG Maker), OpenGMK, Box64 + GL4ES | Motor livre que lê os dados do jogo, ou x86 emulado com GL traduzido | Bom para jogos 2D e leves |

O God Hand hoje está no segundo caminho, com a GPU do PS2 (GS + VU1) ainda imitada. Nenhum port pesado em 3D
que roda bem no RK3326 imita a GPU original: todos desenham direto em GLES.

## Práticas comuns que valem para o God Hand

- Governador da CPU, GPU e memória em `performance` durante o jogo (Strikers, NFSU2). `run_bench.sh` agora faz o mesmo.
- Renderizar direto em 640x480, sem escalar depois.
- Cache de shaders em disco (`glProgramBinary`) para não travar na primeira vez que um efeito aparece.
- Apresentação direta em KMS/DRM, sem compositor.
- Thread de renderização separada da lógica do jogo (NFSU2).
- Texturas de 16 bits e efeitos cortados (THUG): sombras simples, partículas desligadas.
- Trava em 30 FPS para ritmo estável e menos calor (NFSU2).
- Contas trocadas por versões NEON nos pontos quentes, já que o Cortex-A35 executa em ordem (NFSU2).

## Fontes

- [impressivestay6355-cmyk/tenkaichi-r36](https://github.com/impressivestay6355-cmyk/tenkaichi-r36)
- [jbcgames/smstrikers-rk3326](https://github.com/jbcgames/smstrikers-rk3326)
- [tuliocareli/thug-r36s](https://github.com/tuliocareli/thug-r36s)
- [whovian64/pocket-native](https://github.com/whovian64/pocket-native)
- [mpjsilva/nfsu1-recompiled-r36s](https://github.com/mpjsilva/nfsu1-recompiled-r36s)
- [Detoy/nfsmw-r36s](https://github.com/Detoy/nfsmw-r36s)
- [LucasPicoli/god-hand-decomp](https://github.com/LucasPicoli/god-hand-decomp): 90% das funções, 58% do código (24/11/2026)
