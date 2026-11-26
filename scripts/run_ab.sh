#!/bin/sh
# Compara os pacotes A (godhand-A: patches 0001-0009) e B (godhand-B: 0001-0010, upload rápido para a VRAM) no R36S,
# nas mesmas condições: mesma ISO, mesmo save state, mesma duração, PS2X_PROF=1 nos dois (a instrumentação custa o
# mesmo nos dois lados), governors em "performance" (run_bench.sh), pausa entre execuções para a temperatura voltar.
# A ordem é ABBA, BAAB... alternada por rodada, para que o aquecimento ao longo do teste pese igual nos dois.
#
#   sh run_ab.sh "/roms/ps2/God Hand (USA).iso" [segundos=240] [rodadas=2] [save=stage1.state]
#
# Cada rodada roda A e B duas vezes (ABBA): rodadas=2 -> 8 execuções. COOLDOWN=<s> (padrão 60) é a pausa entre elas.
# Escreve um log por execução (bench-cpu-A1-..., bench-cpu-B1-...) e ab-resumo-<data>.txt com uma linha por
# execução e a média por versão. Mande de volta a pasta inteira de logs ou pelo menos o ab-resumo e os bench-cpu-*.
#   sh run_ab.sh --resumo bench-cpu-A1-*.log bench-cpu-B2-*.log ...   refaz só a tabela a partir de logs existentes.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
SUMMARY="$DIR/ab-resumo-$STAMP.txt"
if [ "${1:-}" = "--resumo" ]; then
    shift
    LOGS="$*"
    SECS="?"; STATE="?"
else
ISO=${1:?uso: sh run_ab.sh <iso> [segundos] [rodadas] [save-state]}
SECS=${2:-240}
ROUNDS=${3:-2}
STATE=${4:-$DIR/stage1.state}
COOLDOWN=${COOLDOWN:-60}
for b in godhand-A godhand-B; do [ -x "$DIR/$b" ] || { echo "falta $DIR/$b"; exit 1; }; done

LOGS=""
n=0
r=1
while [ "$r" -le "$ROUNDS" ]; do
    if [ $((r % 2)) -eq 1 ]; then ORDER="A B B A"; else ORDER="B A A B"; fi
    for v in $ORDER; do
        n=$((n + 1))
        TAG="$v$n"
        echo "== execução $n: pacote $v (rodada $r de $ROUNDS)"
        GODHAND_BIN="godhand-$v" BENCH_TAG="$TAG" PS2X_PROF=1 sh "$DIR/run_bench.sh" "$ISO" cpu "$SECS" "$STATE" > /dev/null 2>&1
        LOG=$(ls -t "$DIR"/bench-cpu-"$TAG"-*.log 2>/dev/null | head -1)
        [ -n "$LOG" ] && LOGS="$LOGS $LOG"
        grep -E "^\[bench-summary\]" "$LOG" 2>/dev/null | cut -c1-90
        echo "   pausa de $COOLDOWN s"
        sleep "$COOLDOWN"
    done
    r=$((r + 1))
done
fi

# Uma linha por execução, a partir dos logs. Os tempos por thread são ms de CPU por field emulado (participação da
# thread no segundo x 10 ms / fields por segundo), média dos segundos medidos (depois do aquecimento, sem o segundo
# do carregamento do save). As seções são a divisão do tempo de parede da GameThread (PS2X_PROF=1).
{
    echo "Comparação A (0001-0009) x B (0001-0010) — $STAMP — $SECS s por execução, save $(basename "$STATE")"
    echo "$(uname -m) $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')"
    echo
    printf '%-4s %-3s %7s %10s %10s %10s %6s %8s %8s %6s %6s %8s %7s %7s\n' exec pac speed% Game_ms GS_ms IOP_ms vu1% vu-wait% gs-wait% ee% idle% cpuMHz tempC tmaxC
    for LOG in $LOGS; do
        awk -v logname="$(basename "$LOG")" '
            function num(s, key,   i, rest) { i = index(s, key); if (!i) return ""; rest = substr(s, i + length(key)); sub(/[^0-9.].*/, "", rest); return rest }
            /warmup [0-9]+ s excluded/ { w = num($0, "warmup "); if (w != "") warm = w + 0; next }
            /^\[bench\] t=/ && !/state load/ {
                t = num($0, "t="); f = num($0, "fields/s=");
                if (t + 0 < warm || f + 0 <= 1) next
                g = num($0, "GameThread="); s = num($0, "GSThread="); o = num($0, "IOPThread=");
                if (g != "") { gs += g * 10 / f; gn++ } if (s != "") { ss += s * 10 / f; sn++ } if (o != "") { os += o * 10 / f; on++ }
                m = num($0, " mhz="); if (m + 0 > 0) { ms += m; mn++ }
                tc = num($0, " temp="); if (tc + 0 > 0) { ts += tc; tn++; if (tc + 0 > tmax) tmax = tc + 0 }
            }
            /^\[bench-summary\]/ { speed = num($0, "speed avg=") }
            /^\[bench-sections\]/ { vu1 = num($0, " vu1="); vw = num($0, " vu-wait="); gw = num($0, " gs-wait="); ee = num($0, " ee="); idle = num($0, " idle=") }
            END {
                v = substr(logname, index(logname, "cpu-") + 4, 1); e = substr(logname, index(logname, "cpu-") + 5); sub(/-.*/, "", e)
                printf "%-4s %-3s %7s %10.2f %10.2f %10.2f %6s %8s %8s %6s %6s %8.0f %7.1f %7.0f\n", e, v, speed,
                       gn ? gs / gn : 0, sn ? ss / sn : 0, on ? os / on : 0, vu1, vw, gw, ee, idle, mn ? ms / mn : 0, tn ? ts / tn : 0, tmax
            }' warm=25 "$LOG"
    done
} > "$SUMMARY.tmp"

# Média por versão (colunas numéricas), embaixo da tabela.
{
    cat "$SUMMARY.tmp"
    echo
    echo "Média por pacote:"
    awk 'NR > 4 && ($2 == "A" || $2 == "B") {
            for (c = 3; c <= 14; c++) { s[$2, c] += $c } n[$2]++
         }
         END {
            for (p = 1; p <= 2; p++) {
                v = (p == 1) ? "A" : "B"; if (!n[v]) continue
                printf "%-4s %-3s", "méd", v
                for (c = 3; c <= 14; c++) printf " %9.2f", s[v, c] / n[v]
                printf "   (%d execuções)\n", n[v]
            }
         }' "$SUMMARY.tmp"
    echo
    echo "Colunas: speed% = velocidade de emulação (100% = PS2); Game/GS/IOP_ms = ms de CPU por field emulado de cada"
    echo "thread; vu1% / vu-wait% / gs-wait% / ee% / idle% = parte do tempo de parede da GameThread (vu-wait só existe"
    echo "com PS2X_MTVU=1); cpuMHz = clock médio do cpu0; tempC / tmaxC = zona térmica mais quente, média e máxima."
} > "$SUMMARY"
rm -f "$SUMMARY.tmp"
cat "$SUMMARY"
echo "resumo: $SUMMARY"
