# Hardware-alvo (seção 4)

Status: **não medido.** Tudo abaixo é hipótese até a saída de `scripts/collect_hw_info.sh` ser colada aqui.

## Como coletar

No aparelho (terminal do ArkOS/dArkOS ou SSH):

```sh
sh collect_hw_info.sh > hw.txt
```

Cole o `hw.txt` inteiro em `docs/hardware-raw.txt` (ou no chat). O script só lê informações e roda um benchmark curto.

## Hipótese (não usar em decisões)

| Item | Hipótese | Medido |
| --- | --- | --- |
| SoC | Rockchip RK3326 | ? |
| CPU | 4x Cortex-A35, até ~1,3–1,5 GHz | ? |
| GPU | Mali-G31 MP2 | ? |
| Driver gráfico | libmali (blob) ou Panfrost | ? |
| GLES / Vulkan | GLES 3.2 (blob) ou 3.1 (Panfrost); Vulkan incerto | ? |
| RAM | 1 GB, sem swap ou com zram | ? |
| Tela | 640x480, 4:3 | ? |
| Sistema | ArkOS / dArkOS (Ubuntu ARM64) | ? |
| Áudio | ALSA, SDL2 | ? |
| glibc / SDL2 | ? | ? |
