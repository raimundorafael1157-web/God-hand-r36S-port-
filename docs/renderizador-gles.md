# Renderizador GLES 3 (primeira versão, para medir)

Código em `renderer/gles/`. Um backend do GS (`GSGlesBackend`) que desenha com OpenGL ES 3 em vez do rasterizador
de CPU do runtime, e um programa (`gles_replay`) que reproduz uma gravação de comandos do GS (`.gsr`) o mais rápido
possível, sem tela (EGL surfaceless/GBM). Mede só o custo de desenhar: nenhum código do jogo roda.

## Como funciona

- Cada primitiva do GS vira triângulos num vetor; primitivas seguidas com o mesmo estado GL saem numa chamada só.
- Um framebuffer GL por FBP do GS, uma textura de profundidade por ZBP.
- Texturas decodificadas da imagem da VRAM para RGBA8 e guardadas; quando as páginas da VRAM são reescritas, os
  bytes são comparados por hash (o jogo regrava as mesmas texturas todo quadro).
- Textura em cima de um frame buffer desenhado na GPU: amostra o framebuffer direto (inclusive T8H: alfa pela CLUT).
- **Caminho híbrido**: o que a GPU não reproduz (frame buffer em CT16, máscara de escrita parcial, frame buffer ou
  profundidade lidos como textura em outro formato ou largura, DATE) vai para o rasterizador de CPU do runtime, que é
  exato. Antes, as imagens que só a GPU tem e que o desenho toca são copiadas para a VRAM (cor por `glReadPixels`,
  profundidade por um passe que a converte em bytes); depois, a GPU recebe de volta o que a CPU desenhou.
- Convenção de cor: RGB em bytes (x/255); alfa como no GS (0x80 = 1.0), para o blending com alfa de origem e de
  destino bater com o GS.

## Resultado no PC (luta da fase 1, 120 quadros gravados)

| | por quadro |
|---|---|
| primitivas / triângulos | ~14.000 / ~14.200 |
| chamadas de desenho | ~436 |
| trocas de framebuffer | ~23 |
| texturas decodificadas | ~23 (~3,5 MB), o resto reaproveitado por hash |
| desenhos na CPU (híbrido) | 9 |
| cópias GPU→VRAM / VRAM→GPU | 7 / 4 |

A imagem final bate com a do rasterizador de CPU (diferença média ~8 em 255; o desfoque de profundidade sai um pouco
mais fraco). O tempo medido no PC usa o llvmpipe (GPU em software) e não diz nada sobre o Mali-G31: a medida que vale
é a do aparelho (`run_gles.sh`).

## O efeito que forçou o caminho híbrido

No fim de cada quadro o jogo faz profundidade de campo à moda do PS2: lê o buffer de profundidade (Z16S) como textura
e escreve num frame buffer de cor, relê esse buffer como CT16 em 512x1024 escrevendo com máscara de bits, e usa o
byte de alfa resultante como índice de paleta (T8H) em passes com teste de profundidade. São 9 primitivas, mas cada
uma cobre a tela. No PC, as cópias e o rasterizador de CPU desses passes custam ~10 ms por quadro; no Cortex-A35
seriam várias vezes isso. **Próximo passo:** fazer esse efeito direto em shader (ler a textura de profundidade e
produzir o alfa sem passar pela VRAM).

## No aparelho

    sh run_gles.sh fight.gsr 3

Escreve `gles-<data>.log` e um PPM da imagem a cada 60 quadros. A gravação vem do disco do usuário (gerada com
`PS2X_GS_RECORD=5000:120:fight.gsr` no build x64): nunca vai para o git.
