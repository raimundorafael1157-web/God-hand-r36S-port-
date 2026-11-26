# Código experimental do renderizador

Nada nesta pasta é compilado. Fica separado da versão estável em `renderer/gles/`.

- `dof-gpu-wip.patch`: rascunho da profundidade de campo inteira na GPU (alfa cru nos framebuffers, profundidade
  amostrada como textura, shaders de texture shuffle e de reorganização por largura). **Incompleto, não compila.**
  Falta ligar os caminhos novos em `TextureFor`/`NeedsCpu`/`Submit` e escrever `BuildSwizzleTables`, `Relayout` e
  `SubmitShuffle`. Para retomar: `git apply renderer/experimental/dof-gpu-wip.patch` na raiz do repositório.

- `raw-alpha-attempt.patch`: alvos guardando o byte de alfa cru (como a VRAM) em vez de alfa/128, com blend
  separado para o canal alfa (o GS grava o alfa de origem sem mistura). Pré-requisito da profundidade de campo
  inteira na GPU (o efeito põe um byte da profundidade e depois índices de paleta até 0xF8 no alfa).
  **Rejeitado por enquanto (26/11/2026):** medido contra o rasterizador de CPU em 40 quadros da luta, a diferença
  média subiu de 8,36 para 9,10/255 (pixels > 16: de 14,4% para 17,2%); e mesmo com os passes de efeito
  desligados (`GLES_DEBUG=x`) muda ~1.400 pixels por quadro numa faixa x = 0–63, a largura das faixas do passe de
  profundidade de campo. Há uma interação com o caminho híbrido CPU/GPU ainda não identificada. Próximo passo:
  ferramenta de comparação passo a passo (alvos e alfa depois de cada desenho, GPU contra CPU) antes de retomar.
