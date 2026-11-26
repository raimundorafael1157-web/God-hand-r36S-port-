# Roteiro de sexta

Depende de o ISO chegar antes ou não.

## Se o ISO chegar antes (eu preparo tudo)

1. Eu gero o código do jogo, compilo o ARM64 e testo aqui que o jogo inicia (no PC Linux e no emulador ARM64).
2. Tento fazer um save state numa luta da fase 1, para a medição começar direto nela.
3. Na sexta você baixa um pacote com `godhand-A`, `godhand-B`, os scripts e o save state, copia para o cartão SD e roda.

## Se o ISO não chegar (você compila no PC)

No PC, dentro do WSL (Ubuntu):

```sh
git clone <este repositório> && cd God-hand-r36S-port-
sudo apt install -y cmake ninja-build g++ python3-pip git curl binutils-aarch64-linux-gnu \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev libasound2-dev
./scripts/setup_base.sh
./scripts/build_rk3326.sh generate "/mnt/c/caminho/God Hand (USA).iso"
./scripts/build_rk3326.sh vu1 "/mnt/c/caminho/God Hand (USA).iso"   # VU1 pré-compilado: essencial para a velocidade
./scripts/build_rk3326.sh state "/mnt/c/caminho/God Hand (USA).iso" # save state na primeira luta da fase 1
./scripts/build_rk3326.sh build                    # pacote B (todos os patches): BUILD_OUT=godhand-B
# Pacote A (sem o 0010): PATCH_MAX=0009 ./scripts/setup_base.sh && BUILD_OUT=godhand-A ./scripts/build_rk3326.sh build
```

Copie `builds/rk3326/` para o cartão SD do R36S.

## Pacotes A e B

O pacote traz dois binários do jogo (`VERSOES.txt` tem os hashes):

| Binário | Pacote | Patches | O que muda |
| --- | --- | --- | --- |
| `godhand-A` | A — Referência | 0001–0009 | a base planejada, com a instrumentação |
| `godhand-B` | B — Otimizado | 0001–0010 | + upload rápido para a VRAM (thread do GS) |

A única diferença entre eles é o patch 0010; nenhum tem as otimizações do renderizador (Fase 2). Os comandos de
`run_bench.sh` sem nada a mais usam o `godhand-A`.

### Comparação A x B (a última coisa do dia, com o aparelho frio)

```sh
sh run_ab.sh "/roms/ps2/God Hand (USA).iso"          # 2 rodadas ABBA de 240 s = 8 execuções, ~45 min
```

Mesma ISO, mesmo save, mesma duração, governors em "performance" e `PS2X_PROF=1` nas duas versões; a ordem
alterna (A B B A, depois B A A B) para o aquecimento pesar igual, com 60 s de pausa entre execuções
(`COOLDOWN=90 sh run_ab.sh ...` para mais). No fim sai `ab-resumo-<data>.txt`:

- uma linha por execução: velocidade, ms de CPU por field da GameThread, da GSThread e da IOPThread, VU1, vu-wait
  (espera do EE pelo VU1, só existe com `PS2X_MTVU=1`), gs-wait (espera pelo GS), ee, idle, clock da CPU,
  temperatura média e máxima;
- a média por pacote.

`sh run_ab.sh --resumo bench-cpu-A1-*.log ...` refaz a tabela a partir dos logs.

## No R36S

```sh
sh collect_hw_info.sh > hw.txt
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300 stage1.state   # a luta: o teste que decide
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 600                # abertura + começo da fase, sem save
PS2X_IOP_SPIN_US=200 sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300 stage1.state   # IOP girando menos: menos calor?
PS2X_MTVU=1 sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300 stage1.state   # VU1 em outra thread
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" swgs 600
sh run_gles.sh fight.gsr 3                                            # a GPU desenhando a luta gravada (renderer/gles)
GLES_PAGE_HASH=xxh3 sh run_gles.sh fight.gsr 3                        # o mesmo com o hash XXH3 (compare o cpu= das duas)
```

Depois, as rodadas de diagnóstico (instrumentação; nenhuma muda o jogo, todas desligadas no comando simples acima):

```sh
PS2X_PROF=1 sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300 stage1.state   # tempo por subsistema (EE, VU1, IOP...)
PS2X_PROF=1 PS2X_PIN=auto sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300 stage1.state   # o mesmo, threads fixas em núcleos
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 900 stage1.state   # 15 min: aquecimento e throttling ([bench-thermal])
```

O `hw.txt` agora traz também o driver GLES de verdade (`[gl-info]`: blob ou Panfrost, extensões como
`EXT_buffer_storage` e timer query) e o `[hash-bench]` (hash atual contra XXH3 na CPU do aparelho). Se a
`libgbm` não existir (driver fbdev), o `gles_replay` usa sozinho o stub de `stubs-gles/`.

A primeira linha do log (`[bench-sensors]`) diz quais sensores o kernel do ArkOS expõe; o que faltar aparece como
`n/a` e o teste continua. A rodada de 900 s é a mais longa: deixe o R36S na mesa, fora da capa, carregado.

Mande de volta: `hw.txt`, os arquivos `bench-*.log`, `gles-*.log`, o `ab-resumo-*.txt` e um dos `frame-*.ppm` (para ver se a imagem saiu certa).
