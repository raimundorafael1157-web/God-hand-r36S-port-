# Código experimental do renderizador

Nada nesta pasta é compilado. Fica separado da versão estável em `renderer/gles/`.

- `dof-gpu-wip.patch`: rascunho da profundidade de campo inteira na GPU (alfa cru nos framebuffers, profundidade
  amostrada como textura, shaders de texture shuffle e de reorganização por largura). **Incompleto, não compila.**
  Falta ligar os caminhos novos em `TextureFor`/`NeedsCpu`/`Submit` e escrever `BuildSwizzleTables`, `Relayout` e
  `SubmitShuffle`. Para retomar: `git apply renderer/experimental/dof-gpu-wip.patch` na raiz do repositório.
