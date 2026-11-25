#!/bin/sh
# Coleta de hardware para o port God Hand RK3326 (seção 4 do prompt).
# Rode no aparelho (via SSH ou terminal do ArkOS/dArkOS): sh collect_hw_info.sh > hw.txt
# Só lê informações; não altera nada no sistema.

sec() { printf '\n===== %s =====\n' "$1"; }
try() { printf '$ %s\n' "$*"; sh -c "$*" 2>&1 || printf '(falhou ou não existe)\n'; }

sec "Arquitetura e kernel"
try uname -a
try cat /etc/os-release
try 'ls /etc/*release* /etc/*version* 2>/dev/null; cat /usr/share/plymouth/themes/text.plymouth 2>/dev/null | head -5'

sec "CPU"
try cat /proc/cpuinfo
try 'for c in /sys/devices/system/cpu/cpu[0-9]*; do echo "$c: gov=$(cat $c/cpufreq/scaling_governor) cur=$(cat $c/cpufreq/scaling_cur_freq) max=$(cat $c/cpufreq/cpuinfo_max_freq) avail=$(cat $c/cpufreq/scaling_available_frequencies 2>/dev/null)"; done'

sec "Memória"
try cat /proc/meminfo
try 'cat /proc/swaps; free -m'
try 'cat /sys/block/zram0/disksize 2>/dev/null'

sec "SoC / device tree"
try 'cat /proc/device-tree/model; echo; cat /proc/device-tree/compatible | tr "\0" " "; echo'

sec "GPU e driver"
try 'ls /sys/class/devfreq/; for d in /sys/class/devfreq/*; do echo "$d: gov=$(cat $d/governor) cur=$(cat $d/cur_freq) max=$(cat $d/max_freq) avail=$(cat $d/available_frequencies 2>/dev/null)"; done'
try 'lsmod | grep -iE "panfrost|mali|bifrost"'
try 'ls -l /dev/dri /dev/mali* 2>/dev/null'
try 'dmesg 2>/dev/null | grep -iE "panfrost|mali|gpu" | head -40'
try 'ls /usr/lib/aarch64-linux-gnu/ | grep -iE "mali|libEGL|libGLES|libvulkan|libgbm"'
try 'eglinfo 2>/dev/null | head -80'
try 'es2_info 2>/dev/null | head -30'
try 'glxinfo -B 2>/dev/null | head -30'
try 'vulkaninfo --summary 2>/dev/null | head -60'
try 'ls /usr/share/vulkan/icd.d/ 2>/dev/null'

sec "Tela"
try 'cat /sys/class/graphics/fb0/virtual_size /sys/class/graphics/fb0/modes 2>/dev/null'
try 'cat /sys/class/drm/*/modes 2>/dev/null'

sec "Áudio"
try 'cat /proc/asound/cards; aplay -l'
try 'ls /usr/lib/aarch64-linux-gnu/ | grep -iE "libSDL2|libasound|libpulse|pipewire"'

sec "Bibliotecas de base (para a cross-compilation)"
try 'ldd --version | head -1'
try 'ls -l /usr/lib/aarch64-linux-gnu/libSDL2*'
try 'strings /usr/lib/aarch64-linux-gnu/libstdc++.so.6 2>/dev/null | grep -E "^GLIBCXX_[0-9.]+$" | tail -3'

sec "Armazenamento e temperatura"
try 'df -h'
try 'for z in /sys/class/thermal/thermal_zone*; do echo "$z: $(cat $z/type) $(cat $z/temp)"; done'

sec "Benchmark de CPU simples (mesmo comando no PC; Fase 0, passo 5)"
try 'command -v sysbench && sysbench cpu --cpu-max-prime=10000 --threads=1 run | grep -E "events per second|total time"'
try 'command -v sysbench && sysbench cpu --cpu-max-prime=10000 --threads=4 run | grep -E "events per second|total time"'
try 'time sh -c "i=0; while [ \$i -lt 300000 ]; do i=\$((i+1)); done"'
