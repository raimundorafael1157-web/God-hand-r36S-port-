#!/bin/sh
# Teste A/B no R36S: pacote A (godhand-A, patches 0001-0009) x pacote B (godhand-B, 0001-0010: upload rápido para a
# VRAM). A única diferença entre os binários é o patch 0010.
#
#   sh run_ab.sh "/roms/ps2/God Hand (USA).iso"              8 execuções de 300 s: A1 B1 B2 A2 A3 B3 B4 A4
#   sh run_ab.sh "/roms/ps2/God Hand (USA).iso" --longo      depois disso, A5 e B5 de 900 s (térmico, sustentado)
#   sh run_ab.sh --resumo bench-cpu-A1-*.log ...             refaz só a tabela a partir de logs existentes
#
# Mesmas condições em todas as execuções: mesma ISO, mesmo save (stage1.state: a primeira luta da fase 1), nenhum
# botão (PS2X_PAD_SCRIPT vazio: a luta segue sozinha, igual em todas), mesma duração, GS nulo, mesmas configurações
# (o godhand.ini da pasta, igual para os dois binários), governors em "performance" (run_bench.sh) e PS2X_PROF=1
# (a instrumentação custa o mesmo nos dois lados). Antes de cada execução: pausa de estabilização (COOLDOWN, padrão
# 60 s) e espera, até 120 s a mais, de a temperatura voltar para perto da do começo do teste.
# Antes de tudo confere os SHA-256 dos binários (SHA256SUMS) e registra o resultado.
#
# Saída: um log por execução (bench-cpu-A1-<data>.log ...) e ab-resumo-<data>.txt com uma linha por execução e a
# média por pacote. Mande de volta a pasta inteira (ou ao menos ab-resumo-*.txt, ab-execucao-*.txt e bench-cpu-*).
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
SUMMARY="$DIR/ab-resumo-$STAMP.txt"
RUNLOG="$DIR/ab-execucao-$STAMP.txt"

hottest() { m=0; for z in /sys/class/thermal/thermal_zone*/temp; do t=$(cat "$z" 2>/dev/null || echo 0); [ "$t" -gt "$m" ] 2>/dev/null && m=$t; done; echo $((m / 1000)); }
note() { echo "$*"; echo "$(date +%H:%M:%S) $*" >> "$RUNLOG"; }

if [ "${1:-}" = "--resumo" ]; then
    shift
    LOGS="$*"
    SECS="?"; STATE="?"
else
    ISO=${1:?uso: sh run_ab.sh <iso> [--longo]}
    LONG=0; [ "${2:-}" = "--longo" ] && LONG=1
    SECS=${SECS:-300}
    STATE=${STATE:-$DIR/stage1.state}
    COOLDOWN=${COOLDOWN:-60}
    : > "$RUNLOG"
    note "== teste A/B $STAMP: ISO '$ISO', $SECS s por execução, save $(basename "$STATE"), pausa $COOLDOWN s"
    note "aparelho: $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0') | $(uname -r) | glibc $(ldd --version 2>/dev/null | head -1 | awk '{print $NF}')"
    [ -f "$ISO" ] || { note "ERRO: ISO não encontrada"; exit 1; }
    [ -f "$STATE" ] || { note "ERRO: save não encontrado: $STATE"; exit 1; }
    for b in godhand-A godhand-B; do
        [ -f "$DIR/$b" ] || { note "ERRO: falta $DIR/$b"; exit 1; }
        [ -x "$DIR/$b" ] || chmod +x "$DIR/$b" 2>/dev/null
    done
    if [ -f "$DIR/SHA256SUMS" ] && command -v sha256sum >/dev/null 2>&1; then
        if (cd "$DIR" && sha256sum -c SHA256SUMS) >> "$RUNLOG" 2>&1; then note "SHA-256: binários conferem com SHA256SUMS"
        else note "ERRO: SHA-256 não confere (arquivo corrompido na cópia?) - veja $RUNLOG"; exit 1; fi
    else
        note "AVISO: SHA-256 não conferido (falta SHA256SUMS ou sha256sum)"
    fi
    export PS2X_PAD_SCRIPT=""
    START_TEMP=$(hottest)
    note "temperatura no início: ${START_TEMP} C"

    # A1 B1 B2 A2 A3 B3 B4 A4 (+ A5 B5 de 900 s com --longo)
    PLAN="A1 B1 B2 A2 A3 B3 B4 A4"
    [ "$LONG" = 1 ] && PLAN="$PLAN A5 B5"
    LOGS=""
    n=0
    for TAG in $PLAN; do
        n=$((n + 1))
        v=$(echo "$TAG" | cut -c1)
        RUN_SECS=$SECS
        case "$TAG" in A5|B5) RUN_SECS=900 ;; esac
        sleep "$COOLDOWN"
        waited=0
        while [ "$(hottest)" -gt $((START_TEMP + 2)) ] && [ "$waited" -lt 120 ]; do sleep 10; waited=$((waited + 10)); done
        note "== execução $n ($TAG): pacote $v, $RUN_SECS s, temperatura antes $(hottest) C"
        GODHAND_BIN="godhand-$v" BENCH_TAG="$TAG" PS2X_PROF=1 sh "$DIR/run_bench.sh" "$ISO" cpu "$RUN_SECS" "$STATE" > /dev/null 2>&1
        LOG=$(ls -t "$DIR"/bench-cpu-"$TAG"-*.log 2>/dev/null | head -1)
        if [ -z "$LOG" ]; then note "   ANOMALIA: sem log para $TAG"; continue; fi
        LOGS="$LOGS $LOG"
        CODE=$(grep -E "^exit code" "$LOG" | tail -1)
        SUMLINE=$(grep -E "^\[bench-summary\]" "$LOG" | cut -c1-110)
        note "   $SUMLINE ($CODE)"
        [ -z "$SUMLINE" ] && note "   ANOMALIA: execução $TAG sem [bench-summary] (travou ou terminou cedo?)"
        grep -qiE "\[crash\]|fatal|segmentation|ERROR" "$LOG" && note "   ANOMALIA: $TAG tem linhas de erro no log (ver $(basename "$LOG"))"
    done
    note "== fim; temperatura $(hottest) C"
fi

# Uma linha por execução. Tempos por thread = tempo OCUPADO (CPU) em ms por field emulado: participação da thread no
# segundo (utime+stime de /proc) x 10 ms / fields por segundo, média dos segundos medidos (sem aquecimento nem o
# segundo do carregamento). As seções (vu1, vu-wait, gs-wait, ee, idle) são % do tempo de PAREDE da GameThread.
{
    echo "Teste A/B: A = godhand-A (0001-0009), B = godhand-B (0001-0010) | $STAMP | $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0')"
    echo
    printf '%-4s %-3s %6s %5s %8s %8s %8s %8s %6s %7s %7s %6s %6s %13s %11s %5s %7s %6s %5s\n' \
        exec pac speed% fps fields Game_ms GS_ms IOP_ms vu1% vuwait% gswait% ee% idle% MHz_min/med/max tC_min/med/max thr rssMB worstms erros
    for LOG in $LOGS; do
        awk -v logname="$(basename "$LOG")" '
            function num(s, key,   i, rest) { i = index(s, key); if (!i) return ""; rest = substr(s, i + length(key)); sub(/[^0-9.].*/, "", rest); return rest }
            BEGIN { warm = 25; mmin = 1e9; tmin = 1e9 }
            /warmup [0-9]+ s excluded/ { w = num($0, "warmup "); if (w != "") warm = w + 0; next }
            /^\[bench\] t=/ && !/state load/ {
                t = num($0, "t="); f = num($0, "fields/s=");
                if (t + 0 < warm || f + 0 <= 1) next
                fields += f; fr = num($0, "frames/s="); frames += fr; secs++
                g = num($0, "GameThread="); s = num($0, "GSThread="); o = num($0, "IOPThread=");
                if (g != "") { gs += g * 10 / f; gn++ } if (s != "") { ss += s * 10 / f; sn++ } if (o != "") { os += o * 10 / f; on++ }
                m = num($0, " mhz="); if (m + 0 > 0) { ms += m; mn++; if (m + 0 < mmin) mmin = m + 0; if (m + 0 > mmax) mmax = m + 0 }
                cap = num($0, " max="); if (cap + 0 > 0) { if (!cap0) cap0 = cap + 0; if (cap + 0 < cap0 || (m + 0 > 0 && m + 0 < cap + 0)) thr++ }
                tc = num($0, " temp="); if (tc + 0 > 0) { ts += tc; tn++; if (tc + 0 > tmax) tmax = tc + 0; if (tc + 0 < tmin) tmin = tc + 0 }
                r = num($0, " rss="); if (r + 0 > rss) rss = r + 0
                wv = num($0, " worst="); if (wv + 0 > worst) worst = wv + 0
            }
            /\[crash\]|[Ff]atal|[Ss]egmentation|ERROR/ { errs++ }
            /^\[bench-summary\]/ { speed = num($0, "speed avg=") }
            /^\[bench-sections\]/ { vu1 = num($0, " vu1="); vw = num($0, " vu-wait="); gw = num($0, " gs-wait="); ee = num($0, " ee="); idle = num($0, " idle=") }
            END {
                i = index(logname, "cpu-"); tag = substr(logname, i + 4); sub(/-.*/, "", tag); v = substr(tag, 1, 1)
                if (!mn) { mmin = 0 } if (!tn) { tmin = 0 }
                printf "%-4s %-3s %6s %5.1f %8.0f %8.2f %8.2f %8.2f %6s %7s %7s %6s %6s %4.0f/%4.0f/%4.0f %3.0f/%3.0f/%3.0f %5d %7.0f %6.0f %5d\n",
                    tag, v, speed, secs ? frames / secs : 0, fields, gn ? gs / gn : 0, sn ? ss / sn : 0, on ? os / on : 0,
                    vu1 == "" ? "-" : vu1, vw == "" ? "-" : vw, gw == "" ? "-" : gw, ee == "" ? "-" : ee, idle == "" ? "-" : idle,
                    mmin, mn ? ms / mn : 0, mmax, tmin, tn ? ts / tn : 0, tmax, thr, rss, worst, errs
            }' "$LOG"
    done
} > "$SUMMARY.tmp"

{
    cat "$SUMMARY.tmp"
    echo
    echo "Média por pacote (as estatísticas completas - mediana, desvio, melhor/pior, diferença % - saem do"
    echo "tools/ab_report.py no PC, a partir dos mesmos logs):"
    awk 'NR > 3 && ($2 == "A" || $2 == "B") && $1 !~ /5$/ {
            for (c = 3; c <= 13; c++) { s[$2, c] += $c } n[$2]++
         }
         END {
            printf "%-8s %6s %5s %8s %8s %8s %8s %6s %7s %7s %6s %6s\n", "", "speed%", "fps", "fields", "Game_ms", "GS_ms", "IOP_ms", "vu1%", "vuwait%", "gswait%", "ee%", "idle%"
            for (p = 1; p <= 2; p++) {
                v = (p == 1) ? "A" : "B"; if (!n[v]) continue
                printf "%-8s", "média " v
                for (c = 3; c <= 13; c++) printf " %7.2f", s[v, c] / n[v]
                printf "   (%d execuções de 300 s)\n", n[v]
            }
         }' "$SUMMARY.tmp"
    echo
    echo "Colunas: speed% velocidade de emulação (100% = PS2) | fps quadros do jogo por segundo | fields emulados no"
    echo "período medido | Game/GS/IOP_ms tempo OCUPADO de CPU por field emulado | vu1% vuwait% gswait% ee% idle%: % do"
    echo "tempo de PAREDE da GameThread (vuwait só com PS2X_MTVU=1) | MHz cpu0 mín/méd/máx | tC zona mais quente"
    echo "mín/méd/máx | thr segundos com throttling (clock abaixo do teto ou teto caindo) | rssMB memória máx |"
    echo "worstms pior intervalo entre quadros | erros linhas de erro no log. A5/B5 (900 s) ficam fora da média."
} > "$SUMMARY"
rm -f "$SUMMARY.tmp"
cat "$SUMMARY"
echo "resumo: $SUMMARY"
