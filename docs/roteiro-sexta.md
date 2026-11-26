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
```

Mande de volta: `hw.txt` e os arquivos `bench-*.log`.
