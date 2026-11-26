# Roteiro de sexta

Depende de o ISO chegar antes ou não.

## Se o ISO chegar antes (eu preparo tudo)

1. Eu gero o código do jogo, compilo o ARM64 e testo aqui que o jogo inicia (no PC Linux e no emulador ARM64).
2. Tento fazer um save state numa luta da fase 1, para a medição começar direto nela.
3. Na sexta você baixa um pacote com `godhand`, `run_bench.sh` e o save state, copia para o cartão SD e roda.

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
./scripts/build_rk3326.sh build
```

Copie `builds/rk3326/` para o cartão SD do R36S.

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

Mande de volta: `hw.txt`, os arquivos `bench-*.log`, `gles-*.log` e um dos `frame-*.ppm` (para ver se a imagem saiu certa).
