# O que os ports encontrados têm de útil para o God Hand

Análise feita em 27/11/2026 a partir de clones rasos de três repositórios:
- `amarqs182/celosia`: Shadow of the Colossus, PS2Recomp, R36S.
- `manuelpllull/NFSHP2-R36S`: NFS Hot Pursuit 2, decompilação, R36S.
- `owaissseeee/GodHandAndroidPort`: God Hand, PS2Recomp, Android.

Os relatórios de pesquisa estão em `reports/`.

**Nada aqui foi implementado.** Pela regra combinada, cada item espera os números do R36H. Cada item diz qual
número da sexta decide se vale fazer.

## Licenças: o que pode ser reaproveitado

| Repositório | Licença | Uso possível |
|---|---|---|
| Celosia (runtime PS2Recomp modificado e renderer próprio) | GPL-3.0 | Código pode ser trazido, mantendo a GPL-3.0 (a nossa base já é GPL-3.0) e a autoria |
| NFSHP2-R36S (camada GLES, launcher, instalador) | GPL-3.0 | Idem |
| GodHandAndroidPort | Diz GPL-3.0, mas **publica ~240 MB de C++ gerado do jogo** (`app/src/main/cpp/generated*`) | Só como referência de técnicas. **Não copiar nada de `generated*/` nem de patches com endereços do jogo para o nosso repositório.** A regra "nunca commitar dados ou código gerado do jogo" continua valendo |

O runtime do port de Android não vem do LasVola. Vem de outro fork (o do port de Urban Reign, derivado do
`TaylorNAlbarnaz/PS2Recomp` `sotc-port`), então os patches dele não entram direto na nossa base.

## Itens, em ordem de prioridade

### 1. Release de verdade: tirar os logs e o rastreamento do runtime (baixo risco, só build)

**O que vimos:** o nosso `build-rk3326/CMakeCache.txt` está com `PS2X_ENABLE_RUNTIME_LOGS=ON` e
`PS2X_ENABLE_IOP_RPC_TRACE=ON`, que são os padrões do runtime. Cada `RUNTIME_LOG` monta um `std::ostringstream`,
escreve no `std::cout` e guarda o texto numa lista com mutex. O runtime tem 89 chamadas desse tipo.

**Referência:** no Celosia, a fase A desligou READ_WATCH, WRITE_TRACE e RUNTIME_LOGS e fez um PGO novo:
+6,45% de fps e −2 ms na thread do jogo no R36S. Só desligar as opções, sem PGO novo, ficou dentro do ruído
(30,38 → 30,24 fps). O ganho veio da soma. A nossa base não tem READ_WATCH nem WRITE_TRACE: `ps2TraceGuestRangeWrite`
já é vazia.

**O que decide:** quantas linhas de log o binário escreve por minuto de luta. Dá para contar no log da sexta
(`bench-cpu-*.log`). Se for perto de zero, o ganho é desprezível, mas a mudança continua sendo só de build.

### 2. PGO feito no próprio aparelho, mais LTO no runtime (baixo risco, só build)

**Referência no Celosia:**
- PGO com perfil gravado no R36S: +5,5% de fps e −5,1% na thread do jogo.
- LTO seletivo (`-flto -fno-plt` só no runtime, no IOP e nos overrides, não no código gerado): ganho medido junto
  com o fastmem.
- Ordenação das funções quentes pelo linker (`linux_hot_order.ld`).

**Armadilhas que eles registraram:**
- Perfil antigo com código mudado é descartado em silêncio. Por isso compilam com `-Wcoverage-mismatch`.
- O perfil precisa de `-Wl,-u,__gcov_dump` e de um gatilho para gravar no meio do jogo.

**No NFSHP2:** o PGO deu só −1,6% (pcrecomp, medido em VM). O ganho depende do código.

**O que decide:** `ee` mais `dispatch` altos no `[bench-sections]`. O nosso `GODHAND_LTO` existe e está OFF no
build do RK3326.

### 3. Memória espelhada (fastmem): já estava na nossa lista, agora com dados do R36S (risco médio)

**Referência:** o Celosia fez exatamente o item 2 do `docs/analise-otimizacoes-cpu.md`:
- `memfd_create` e espelhos da RDRAM numa janela de 4 GiB;
- trap SIGSEGV para os acessos especiais, com o handler reinstalado depois do `SDL_Init`;
- o binário caiu 23,6% (64 → 48 MB), sinal de que sumiu o código frio embutido em cada acesso;
- 0 traps durante o jogo e 0 diferenças nos testes de equivalência.

**Diferença para nós:** eles também trocaram por versões nativas as funções do jogo que liam o `GS_CSR` (mais de
14.000 traps). No God Hand, `sceGsSyncV` já é HLE, mas falta medir se há outras leituras de `GS_CSR` em laço.

**O que decide:** `ee` alto no A35 e o tamanho do `.text`, como já estava escrito.

### 4. Não dividir a lógica em mais threads no A35 (confirma o que medimos)

**Referência no Celosia:**
- Mandar personagens para uma thread de apoio custou −2,7 fps: as esperas na thread do jogo e a disputa pelo L2
  de 256 KB comeram o ganho.
- Tirar um prefetch especulativo com mutex deu +1,15 fps, também por causa do L2.
- Código maior por inlining perdeu ~1 fps, por estourar o L1I de 32 KB.

**Para nós:** reforça o resultado do `PS2X_MTVU=1` (pior no PC, `vu-wait` de 16,6%) e o cuidado com tabelas novas.
No A35, menos memória tocada vale mais que paralelismo. Qualquer otimização que aumente tabelas ou código quente
precisa de A/B no aparelho, porque no PC isso não aparece.

### 5. Ritmo de quadros e painel de 77 Hz (baixo risco, afeta só o tempo)

**Referências:**
- No NFSHP2, os menus travam em 12,9 ms por quadro, porque o painel do R36S medido roda a 77,4 Hz e não a 60.
  As cadências estáveis são 38,7 e 25,8 fps.
- No Celosia, `SDL_GL_SetSwapInterval(0)` é ignorado (o swap continua 1). Eles controlam o ritmo com
  `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)`.

**Para nós:** o God Hand roda a 30 lógicos por 60 fields. Num painel de 77 Hz, o vsync não fecha com 60 Hz e
aparece trepidação mesmo com a CPU sobrando.

**O que medir:** a taxa do painel do R36H. O `collect_hw_info.sh` atual não grava isso. Dá para tirar do
`/sys/class/drm/*/modes` ou do tempo do swap num menu. Sugestão para o próximo pacote: só medição, nenhuma
otimização.

### 6. Renderizador GLES: o custo por chamada de desenho é o número que manda

**Referências:**
- O blob Mali r13p0 custa 30–42 µs por chamada de desenho na thread que chama o GL (NFSHP2, regressão com
  r² 0,93; Celosia parecido).
- Nossa luta da fase 1 tem ~436 chamadas por quadro no `gles_replay` (`docs/renderizador-gles.md`). São de 13 a
  18 ms por quadro só de driver, dentro do orçamento de 33 ms, mas é a maior parcela.

**O que já temos e o que falta:**
- Já temos o GL numa thread separada da thread do jogo (a GSThread). Foi o maior ganho do NFSHP2 (16,8 → 27 fps,
  junto com o agrupamento).
- Já temos um só programa principal, então um cache de binário de shader (73–135 ms por programa) é prioridade
  baixa.
- **O que eles mostraram que funciona no Mali:**
  - `glInvalidateFramebuffer` de profundidade e stencil antes do swap e em alvos sobrescritos (Celosia: render
    −13,9%);
  - evitar UBO (+15 µs por chamada);
  - VBO estático ajuda pouco (−3 µs por chamada no NFSHP2).
- **O que não funcionou:** juntar chamadas vizinhas em tempo de execução (menos de 1% das chamadas no Burnout 3),
  e ordenar por textura (muda a ordem do blending).

**O que decide:** `gles-*.log` da sexta. Medir ms por quadro contra chamadas por quadro e tirar a inclinação. Se
for perto de 40 µs, o caminho é reduzir chamadas e trocas de estado no nosso agrupamento, antes de mexer em
shader.

### 7. O atalho do `sceGsExecStoreImage` no port de Android (risco alto, só investigar)

**O que eles fazem:** substituem `sceGsExecStoreImage` (0x3958D8) por um retorno imediato. Segundo eles, isso tira
15 esperas de 50–90 ms nas cutscenes. Só funciona porque o renderer Vulkan deles não devolve a VRAM, então
pular a leitura não muda nada para eles.

**Na nossa base:** a função roda como código recompilado do jogo e lê a VRAM de verdade, pelo FIFO GS→host
emulado (`ps2_memory.cpp`). Pular a leitura muda os dados que o jogo recebe.

**O que decide:** se a sexta mostrar travadas nas cutscenes, contar quanto tempo se passa dentro dessa função.
Hoje o `ps2_prof` não mede isso; seria uma seção nova. Só faz sentido trocar o comportamento se o jogo não usar
o resultado, e isso exige prova (gravação do GS idêntica).

### 8. Empacotamento para o PortMaster: modelo pronto no NFSHP2

**O modelo:**
- launcher de ~60 linhas (`r36s/portmaster/NFSHP2.sh`), `port.json` sem `power` (aparece no RK3326);
- ISO do usuário em `gamedata/`, instalada na primeira execução pelo `patcher.txt` com um script Python
  (verifica a região, pode ser retomada, ~10 min);
- caminho alternativo pelo PC;
- sem swap nem travamento de clocks no launcher, por regra do PortMaster.

**Para nós:**
- Os nossos scripts `scripts/ports/` são de teste e travam clocks e afinidade. Para uma versão pública, o launcher
  precisa seguir o modelo do NFSHP2.
- O passo de "gerar o código do jogo a partir do ISO" é do mesmo tipo que o modelo do DaytonaArm, que o PortMaster
  ainda não respondeu.
- O guia manda ports da Capcom para o Multiverse.
- Isso fica para depois que o jogo rodar bem. Antes de qualquer PR, perguntar no `#testing-n-dev` do Discord.

## O que não aproveitar

- O **60 fps** e os cheats do port de Android: mudam a jogabilidade e estão fora do escopo.
- **Vulkan e backend GS do PCSX2:** o Vulkan da Mali não mostra imagem no R36S.
- **Reescrever funções do jogo à mão,** como as 351 do Celosia, que levaram dois terços do tempo da thread do jogo
  para código nativo: é trabalho de meses. Só depois que o perfil no A35 apontar funções específicas.
- **EGL direto no GBM:** no Celosia travou o kernel. O nosso `gles_replay` usa EGL surfaceless ou GBM, então o
  primeiro teste no R36H deve ser o `--gl-info`, que já está no roteiro, e não uma luta longa.
