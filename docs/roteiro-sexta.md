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
sudo apt install -y cmake ninja-build g++ python3-pip git curl
./scripts/setup_base.sh
./scripts/build_rk3326.sh generate "/mnt/c/caminho/God Hand (USA).iso"
./scripts/build_rk3326.sh build
```

Copie `builds/rk3326/` para o cartão SD do R36S.

## No R36S

```sh
sh collect_hw_info.sh > hw.txt
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" cpu 300
sh run_bench.sh "/roms/ps2/God Hand (USA).iso" swgs 300
```

Mande de volta: `hw.txt` e os arquivos `bench-*.log`.
