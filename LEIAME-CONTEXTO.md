# LEIA-ME de contexto: port de God Hand para o R36S

Este arquivo existe para passar o projeto inteiro para outra conta ou outra sessão do Claude.
**Para quem chega agora (outra IA ou pessoa): leia tudo antes de agir.** As regras da seção 2 foram combinadas com
o dono do projeto ao longo de muitas sessões e continuam valendo.

Estado em: **27/11/2026, noite** (sexta-feira, dia do primeiro teste no aparelho).
Branch de trabalho: `claude/artifact-session-5x53uj`. A `main` não foi alterada.
Último commit antes deste arquivo: `90cddb6`.

---

## 1. O que é o projeto

- **Objetivo:** rodar o God Hand de PS2 (versão americana, **SLUS-21503**) **nativamente** no **R36S**, sem
  emulador. O aparelho de teste do usuário é um **R36H**, praticamente igual ao R36S.
  - Chip RK3326: 4 núcleos Cortex-A35 a até ~1,5 GHz.
  - GPU Mali-G31, ~1 GB de RAM, tela 640x480.
  - Firmware provável: ArkOS ou dArkOS. Isso ainda não foi confirmado no aparelho.
- **Como:** recompilação estática com o PS2Recomp. O código MIPS do jogo é traduzido para C++ e compilado para
  ARM64.
  - A base é o **godhand-recomp do LasVola** (fork do PS2Recomp, GPL-3.0), commit `14e956a`.
  - O runtime dessa base descende do fork `sotc-port` do PS2Recomp (TaylorNAlbarnaz/LightVelox), commit `ac9efa0`,
    com as mudanças do port de Urban Reign.
- **Idioma:** o usuário fala **português do Brasil** e quer respostas, documentos e relatórios em pt-BR. Ele prefere
  linguagem simples e direta, com resumos curtos.
- **Licença do repositório:** GPL-3.0, compatível com a base e com o Celosia.

---

## 2. Regras combinadas (obrigatórias)

### Dados e privacidade
- **Nunca commitar dados do jogo:** ISO, ELF, código gerado a partir do jogo (`generated*`), save states
  (`.state`), gravações do GS (`.gsr`), binários compilados do jogo (`godhand-A`, `godhand-B`, `godhand`).
  - Tudo isso vem do disco do usuário e é **só para ele**. Não compartilhar nem publicar.
  - A pasta `builds/` é ignorada pelo git por isso.
- **Não pôr email nem dados pessoais do usuário** em buscas, serviços externos ou commits.
- **Não ajudar a converter ou descriptografar `.pkg`** (DRM). Isso já foi recusado uma vez.
- **Não copiar código gerado de outros projetos:** o `GodHandAndroidPort` publica ~240 MB de C++ gerado do jogo;
  no Celosia, `lean_part*` e `decomp_lift*`. Só ideias, nunca esse código.

### Git
- **Não fazer cherry-pick, merge nem alterar a `main` sem autorização.** Trabalhar só no branch acima.
- Um commit por mudança, com mensagem em português.
- Não criar PR sem pedido explícito.
- Um hook da sessão pede commit e push de todo arquivo novo: fazer sempre, exceto dados do jogo.

### Otimizações
- **"Nenhuma nova otimização de CPU ou GPU deve ser implementada antes da análise dos resultados físicos"** do
  R36H.
  - Exceção dada pelo usuário em 27/11: *"Se você tiver a mais pura certeza de que achou algo bom para ser
    implementado durante o processo, aplique"*. Mesmo assim, valem as regras abaixo.
- **Um commit por otimização,** com teste antes e depois (benchmark no PC com `tools/bench_compare.py`). **Não
  afirmar ganho sem benchmark.**
- **Não fazer mudanças arquiteturais sem autorização:**
  - nada de paralelizar o VU1;
  - nada de mudar o VU1 de thread;
  - nada de reescrever o agendador.
- **Não aplicar o XXH3 como padrão** do hash de páginas. Ele existe só como opção `GLES_PAGE_HASH=xxh3`.
- **Não retomar a profundidade de campo (DOF) na GPU** sem antes resolver a divergência visual. O rascunho está em
  `renderer/experimental/`.
- **Não misturar experimentos com a versão estável.** O pacote A é a referência e não muda.
- **Critério de fidelidade:** a gravação do GS precisa sair idêntica byte a byte. No replay GLES, ignorar a borda
  de 3 px, que não é determinística.

---

## 3. Estrutura do repositório

| Caminho | O que é |
|---|---|
| `scripts/setup_base.sh` | Baixa a base do LasVola em `source/godhand-recomp` e aplica `patches/0001…` até `PATCH_MAX` (padrão: todos; o pacote A usou `PATCH_MAX=0009`) |
| `patches/0001-0010` | Nossas mudanças na base (lista abaixo) |
| `scripts/build_rk3326.sh` | Compila para o aparelho. Saídas em `builds/rk3326/` |
| `scripts/run_bench.sh` | Benchmark sem tela (`GODHAND_BIN`, `BENCH_TAG`) |
| `scripts/run_ab.sh` | Teste A/B no aparelho (detalhes na seção 5) |
| `scripts/run_gles.sh` | Teste da GPU (replay de `fight.gsr`) |
| `scripts/collect_hw_info.sh` | Coleta do hardware: `hw.txt`, `gles_replay --gl-info`, `--hash-bench`, `ldd`, libmali, fb |
| `scripts/ports/` | Atalhos do menu Ports do ArkOS: `_comum.sh` e os 4 scripts "GodHand - N …" |
| `scripts/make_gl_stubs.sh` | Gera as bibliotecas falsas (`stubs`, `stubs-gles`) |
| `scripts/make_sysroot.sh` | Prepara o sysroot de cross-compilação |
| `tools/bench_compare.py` | Compara execuções intercaladas: ms por field, hash do GS, bytes de áudio |
| `tools/gs_stats.py`, `tools/gs_targets.py` | Análise de gravações do GS |
| `renderer/gles/` | Renderizador GLES 3 experimental no nível do GS (`gs_gles_backend.cpp`, `gles_replay.cpp`, `egl_headless.cpp`, testes) |
| `renderer/experimental/` | Experimentos rejeitados ou incompletos: `dof-gpu-wip.patch`, `raw-alpha-attempt.patch` |
| `docs/` | Documentação (tabela abaixo) |
| `reports/`, `research_notes/` | Pesquisas profundas (seção 8) |
| `builds/` | **Fora do git:** binários, pacote de teste e backup. Contém dados do jogo |
| `source/godhand-recomp` | **Fora do git:** a base com os patches aplicados e o código gerado do jogo |

### Patches

| Patch | O que faz |
|---|---|
| 0001 | Build no Linux |
| 0002 | ARM64 e raylib DRM (sem X11) |
| 0003 | Modo benchmark sem tela (`PS2X_BENCH=1`) |
| 0004 | Resolução de 640x480 |
| 0005 | Contador de quadros do jogo |
| 0006 | Ajuste do spin do IOP (`PS2X_IOP_SPIN_US`) |
| 0007 | `gs_replay` no Linux |
| 0008 | Build do renderizador GLES |
| 0009 | Instrumentação: `ps2_prof.h`; seções ee, vu1, vu0, iop, dispatch, sched, gif, vif, idle, gs-wait, vu-wait; sensores; `PS2X_PIN`; `PS2X_GAME_NICE`. Ligada só com `PS2X_PROF=1` e `PS2X_BENCH=1`. Custo: < 0,05% desligada, ~6–13% ligada |
| 0010 | Upload rápido host→VRAM (`GSCpuBackend::UploadImage`). No PC: −29,4% na GSThread, 520 casos de teste equivalentes. É a diferença do pacote B |

### Documentos

| Arquivo | Conteúdo |
|---|---|
| `docs/roteiro-sexta.md` | Roteiro do teste no aparelho, pacotes A e B |
| `docs/bench-mode.md` | Variáveis do modo benchmark |
| `docs/analise-otimizacoes-cpu.md` | Fase 4: 5 técnicas de CPU analisadas e **não** implementadas, com o contador que justifica cada uma |
| `docs/analise-ports-portmaster.md` | O que aproveitar dos ports Celosia, NFSHP2 e GodHand Android |
| `docs/renderizador-gles.md` | Como funciona o renderizador GLES e números do PC |
| `docs/resultados-pc.md`, `docs/benchmarks/` | Resultados no PC (README e JSONs) |
| `docs/hardware.md`, `docs/phase0-report.md`, `docs/renderizador-inventario.md` | Levantamentos iniciais |

---

## 4. Build

- **Compilador:** zig 0.17.1 para cross-compilação, alvo `aarch64-linux-gnu.2.30`, `-mcpu=cortex_a35 -O3`.
- **SIMD:** sse2neon v1.8.0 traduz o SSE para NEON.
- **Pastas de build** em `source/godhand-recomp`:
  - `build-host`: ferramentas e testes no PC;
  - `build-x64`: o jogo no PC;
  - `build-rk3326`: o jogo para o aparelho;
  - `build-rk3326-tools`: `gles_replay` para o aparelho.
- **Opções atuais do build do RK3326:**
  - `PS2X_ENABLE_RUNTIME_LOGS=ON` e `PS2X_ENABLE_IOP_RPC_TRACE=ON`. Foram mantidas de propósito; ver seção 9.
  - `GODHAND_LTO=OFF`.
- **Comando para recompilar só o `gles_replay`:**
  `ninja -C build-rk3326-tools gles_replay`. Depois, strip para `builds/rk3326/gles_replay` e atualizar
  `SHA256SUMS` e `VERSOES.txt`.
- **Gerar o código do jogo:** precisa da ISO do usuário (`ps2/God Hand (USA).iso`). O ELF `SLUS_215.03` tem sha256
  `1742f95bef65bdb2aa57b7a77df4ac7619a092e9646e1ea325bc32ec8a64f3cd`. São ~10.600 funções e os overlays SNR2.

---

## 5. O teste no aparelho (sexta, 27/11 à noite)

### Pacotes

| Binário | Patches | sha256 (início) |
|---|---|---|
| `godhand-A` (referência) | 0001–0009 | `89a78cb447970619…` |
| `godhand-B` (otimizado) | 0001–0010 | `56db0e177bd8e1b6…` |
| `gles_replay` (corrigido em 27/11, 20h) | — | `34062bb5a50fae08…` |
| `stage1.state` | save state da luta | `f68916198fc945a7…` |
| `fight.gsr` | gravação da luta | `97f2feaf837b3ed3…` |

### Arquivos enviados ao usuário (backup em `builds/backup-2026-11-27/`)

| Arquivo | Conteúdo | sha256 (início) |
|---|---|---|
| `GodHand-teste-AB.zip` (58 MB) | Pacote de teste | `05acd09f…` |
| `GodHand-fight-gsr.zip` (151 MB) | Gravação da luta | `b6550533…` |
| `GodHand-R36H-guia.pdf` | Guia passo a passo | `ff4bee10…` |

A fonte do guia é `guia.html` no scratchpad da sessão. O zip de teste anterior (sha `1c3cf978…`) tinha o
`gles_replay` antigo, que podia travar o kernel.

### Instalação (resumo do guia)
1. Extrair os zips no PC.
2. Copiar para a pasta `ports` do cartão (partição EASYROMS) a pasta `godhand-teste` e os 4 atalhos "GodHand - …".
3. O `fight.gsr` vai dentro de `ports/godhand-teste`.
4. Colocar a ISO em `ps2/` do cartão.
5. No menu Ports, rodar nesta ordem:
   - **1 Coleta hardware:** segundos.
   - **3 Teste GPU:** minutos.
   - **2 Teste AB:** ~1 h.
   - **4 Teste AB longo:** opcional, ~1h30.
6. Durante o teste a tela fica parada ou só com texto, e isso é normal. O console deve ficar carregando, fora da
   capa, sem ninguém mexer.

### Como funciona o `run_ab.sh`
- Ordem das execuções: **A1 B1 B2 A2 A3 B3 B4 A4**, 300 s cada. Com `--longo`, mais A5 e B5 de 900 s.
- Antes de começar, confere o `SHA256SUMS` e define `PS2X_PAD_SCRIPT=""` (sem entradas).
- Entre execuções: espera 60 s e depois até a temperatura voltar a no máximo a inicial + 2 °C (limite de 120 s).
- Gera `ab-execucao-*.txt` e `ab-resumo-*.txt`. Colunas:
  - exec, pac, speed%, fps, fields;
  - Game_ms, GS_ms, IOP_ms;
  - vu1%, vuwait%, gswait%, ee%, idle%;
  - MHz e temperatura mínima, média e máxima;
  - thr, rssMB, worstms, erros.
- `--resumo <logs>` refaz o resumo a partir dos logs.

### O que o usuário vai mandar de volta
`hw.txt`, `ab-resumo-*.txt`, `ab-execucao-*.txt`, `bench-cpu-*.log` e `gles-*.log`, sem a ISO e sem o `fight.gsr`.

---

## 6. Números de referência no PC

**Luta da fase 1, GameThread com ~10,5 ms por field:**
- ee ~25%, vu1 ~23%, dispatch ~12%, sched ~4%, gif ~4%, vif ~2%, iop ~1%, idle ~29%.

**Outros números:**
- Ruído A/A da GameThread: ~±9%. Diferenças menores que isso não são ganho.
- `PS2X_MTVU=1` (VU1 em outra thread): **pior** no PC, 97% → 82% de velocidade, com `vu-wait` de 16,6%.
- O VU1 já adia o cálculo das flags (`foldStatus`).
- O áudio não é reproduzível bit a bit.
- **Renderizador GLES no PC** (llvmpipe, não diz nada sobre a Mali):
  - ~14.000 primitivas e ~435 chamadas de desenho por quadro;
  - 9 desenhos na CPU por causa da DOF;
  - diferença média ~8 em 255 contra o rasterizador de CPU.
- **Mali e drivers:**
  - Evitar `glMapBufferRange`.
  - O ArkOS pode usar o blob da Mali para fbdev, sem libgbm. Por isso existem os `stubs-gles`.

---

## 7. Achados de segurança e correções recentes

- **Commit `7d434d4` (27/11):** o `gles_replay` não abre mais `/dev/dri/renderD128`.
  - No RK3326, `eglInitialize` com `EGL_PLATFORM_GBM_KHR` nesse nó causa **kernel oops que trava o aparelho até
    desligar na força**. Isso está documentado pelo port Celosia em `docs/r36s_port.md`.
  - Agora ele tenta só `card0` e `card1`. `GLES_EGL_NODE=<nó>` força um nó específico.
  - O jogo em si (raylib DRM) já abria só `card1`/`card0`.
  - **Nunca reintroduzir `renderD128`.**

---

## 8. Pesquisas feitas

Ficam em `reports/` e `research_notes/`.

### Shadow of the Colossus no R36S
- Existe o repositório **`amarqs182/celosia`**, fork do `HenryKun55/celosia`, GPL-3.0. Também usa PS2Recomp, mas o
  runtime vem de outro ramo.
- ~32–34 fps num percurso fixo, segundo o próprio autor. Não tem release.

### Ports de PS2 no PortMaster
- O único port do catálogo que parte da versão de PS2 é o **NFS Hot Pursuit 2** (`nfshp2`, 24/11).
  - É uma decompilação em armhf, de ~27 fps.
  - O usuário põe a ISO em `gamedata/`, e um instalador Python roda na primeira execução.
- Nenhum port feito com PS2Recomp no PortMaster.
- O guia do PortMaster manda ports da Capcom e decomps para o repositório Multiverse.
- `reqs: ["power"]` esconde o port do RK3326.

### Celosia aplicado ao God Hand
- Só as anotações estão em `research_notes/Celosia aplicado ao God Hand/` (3 arquivos).
- **O relatório final NÃO foi escrito:** o usuário interrompeu. Se for pedido, o redator deve ler as 3 notas e
  salvar em `reports/Celosia aplicado ao God Hand.md`.
- **Conclusões das notas:**
  - O renderizador do Celosia é **nativo e específico do SotC**: lê as estruturas do jogo, desliga GS, VIF e VU1, e
    tem ~42 mil linhas feitas em ~6 semanas. **Não serve direto para o God Hand.**
  - Servem as técnicas de GLES na Mali (seção 9) e o controle de ritmo por relógio.
  - Os ganhos "mecânicos" deles (de 25,6 para 34,7 fps):
    - PGO gravado no aparelho: +5,5% a +7,7% por coleta;
    - memória espelhada (fastmem): −10,7% na thread do jogo, num modo sem trava de fps;
    - LTO seletivo: −1,9%;
    - remover um prefetch que disputava o L2: +1,15 fps.
  - Desligar só os logs não deu ganho: 30,38 → 30,24 fps, dentro do ruído.
  - **No A35, mais threads e código maior pioram:** dividir personagens numa thread custou −2,7 fps; inlining que
    estourou o L1I de 32 KB custou −1 fps. Isso combina com o nosso resultado do MTVU.
  - As 351 funções reescritas à mão dependeram de símbolos e de muito trabalho. Para o God Hand, isso é para meses.
  - O que falta para seguir esse caminho: amostrador de PC do guest, difftest (comparar a função nativa com a
    recompilada) e chaves liga/desliga.
  - O PGO com zig/clang usaria perfis LLVM (`.profraw`). Ainda não foi testado se o zig 0.17.1 suporta isso em
    aarch64.

### Outros ports
- **`owaissseeee/GodHandAndroidPort`:** God Hand com PS2Recomp no Android, com Vulkan e backend GS do PCSX2.
  - Não serve para o R36S: o Vulkan da Mali não mostra imagem.
  - Tem um atalho no `sceGsExecStoreImage` (0x3958D8) que pula a leitura da VRAM. **Na nossa base isso muda os
    dados que o jogo recebe:** só investigar se as cutscenes travarem.
- **NFSHP2:** a Mali gasta **30–42 µs por chamada de desenho**. Com as nossas ~435 chamadas por quadro, isso daria
  13–18 ms por quadro só de driver.
  - Alguns R36S rodam o painel a **77,4 Hz**, o que dá cadências estáveis de 38,7 e 25,8 fps.

---

## 9. Próximos passos, em ordem

1. **Receber e analisar os resultados do R36H.** Montar um relatório com:
   - média, mediana, desvio, melhor e pior de cada pacote;
   - ganho = `((A−B)/A)×100`.
2. **Pendências antigas da especificação A/B:**
   - `tools/ab_report.py`, para as estatísticas e gráficos;
   - um modelo de relatório para preencher.
3. **Só depois dos números**, decidir as otimizações. Ordem sugerida (ver `docs/analise-otimizacoes-cpu.md` e
   `docs/analise-ports-portmaster.md`):
   1. Ajustes que só mexem no tempo: `PS2X_IOP_SPIN_US`, limite da fila do GS.
   2. Build: desligar `RUNTIME_LOGS`/`IOP_RPC_TRACE`, depois PGO no aparelho e LTO no runtime. Testar primeiro se
      o zig gera e usa perfis em aarch64.
   3. Memória espelhada: primeiro o espelho duplo de 32 MB, sem trap; depois medir os traps.
   4. Flags do VU1 sob demanda, se o `vu1` pesar.
   5. Passagens no gerador de código, se `ee` + `dispatch` pesarem.
   6. VU1 em thread com fila SPSC: **só com autorização**.
4. **Medições que faltam no pacote** (só medição, sem otimizar):
   - taxa do painel (`/sys/class/drm/*/modes`);
   - tempo gasto dentro do `sceGsExecStoreImage` nas cutscenes.
5. **Renderizador GLES na Mali**, se o `gles-*.log` confirmar o custo por chamada de desenho:
   - `glInvalidateFramebuffer` antes do swap;
   - reduzir trocas de estado;
   - evitar UBO;
   - controle de fps por `clock_nanosleep` (o swap interval 0 é ignorado pelo driver).
   - A integração do GLES no jogo ainda não existe: hoje o jogo usa o rasterizador de CPU.
6. **Futuro (PortMaster):**
   - launcher no modelo do NFSHP2, sem travar clocks nem criar swap;
   - pacote sem `power`;
   - perguntar no `#testing-n-dev` do Discord sobre Multiverse e sobre gerar o código a partir da ISO do usuário.

---

## 10. Outras ideias do usuário

- Ele perguntou se a base serviria para um port de **Carros (Cars) de PS2**. A resposta dada: a ferramenta e o
  método servem (PS2Recomp, medição e pacote), mas cada jogo exige gerar o código a partir do disco dele, montar
  hooks próprios e validar tudo de novo.
- Ele quer saber se dá para chegar a **30 fps**. Os testes de sexta foram montados para calcular isso: quanto da
  GameThread falta cortar no A35, por seção.

---

## 11. Como começar numa sessão nova

1. Clonar este repositório e fazer checkout do branch `claude/artifact-session-5x53uj`.
2. Ler este arquivo, `docs/roteiro-sexta.md` e os dois `docs/analise-*.md`.
3. Para recompilar: `scripts/setup_base.sh` (precisa de rede para baixar a base), depois gerar o código a partir
   da ISO do usuário, depois `scripts/build_rk3326.sh`. **A ISO e o código gerado nunca vão para o git.**
4. Se o usuário mandar os logs do aparelho: analisar com `run_ab.sh --resumo` e `tools/bench_compare.py`, e
   responder em pt-BR com um resumo curto antes dos detalhes.
