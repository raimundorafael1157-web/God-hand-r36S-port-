#!/bin/sh
# Rodada de MEDIÇÃO no R36S/R36H: godhand-B (o pacote B, sem mudança) x godhand-M (o mesmo B + patches de medição
# M01-M03: contadores de caminhos do VU1, flags e GIF). Nenhuma otimização: M só conta e cronometra.
#
#   sh run_meas.sh "/roms/ps2/God Hand (USA).iso"         8 execuções de 240 s: B1 M1 M2 B2 B3 M3 M4 B4
#
# Mesmas condições do run_ab.sh (mesmo save, nenhum botão, GS nulo, PS2X_PROF=1, governors em performance, pausa e
# espera térmica entre execuções). Para que serve cada binário:
#   - M: as contagens (exatas, não dependem do tempo) e o tempo por parte do GIF;
#   - B: o tempo real das seções (os contadores de M custam tempo no VU1);
#   - M x B: o custo da própria instrumentação (Game_ms de M menos o de B).
# Saída: bench-cpu-B1-*.log ... bench-cpu-M4-*.log, meas-resumo-<data>.txt (tabela por execução, via
# run_ab.sh --resumo) e meas-detalhe-<data>.txt (as linhas [bench-vu1], [bench-vu1-paths] e [bench-gif] de cada M).
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
STAMP=$(date +%Y%m%d-%H%M%S)
RUNLOG="$DIR/meas-execucao-$STAMP.txt"
DETAIL="$DIR/meas-detalhe-$STAMP.txt"
hottest() { m=0; for z in /sys/class/thermal/thermal_zone*/temp; do t=$(cat "$z" 2>/dev/null || echo 0); [ "$t" -gt "$m" ] 2>/dev/null && m=$t; done; echo $((m / 1000)); }
note() { echo "$*"; echo "$(date +%H:%M:%S) $*" >> "$RUNLOG"; }

ISO=${1:?uso: sh run_meas.sh <iso>}
SECS=${SECS:-240}
STATE=${STATE:-$DIR/stage1.state}
COOLDOWN=${COOLDOWN:-60}
PLAN=${PLAN:-"B1 M1 M2 B2 B3 M3 M4 B4"}
: > "$RUNLOG"
note "== medição $STAMP: ISO '$ISO', $SECS s por execução, ordem $PLAN, pausa $COOLDOWN s"
note "aparelho: $(cat /proc/device-tree/model 2>/dev/null | tr -d '\0') | $(uname -r)"
[ -f "$ISO" ] || { note "ERRO: ISO não encontrada"; exit 1; }
[ -f "$STATE" ] || { note "ERRO: save não encontrado: $STATE"; exit 1; }
for b in godhand-B godhand-M run_bench.sh run_ab.sh; do
    [ -f "$DIR/$b" ] || { note "ERRO: falta $DIR/$b"; exit 1; }
done
chmod +x "$DIR/godhand-B" "$DIR/godhand-M" 2>/dev/null
for sums in SHA256SUMS SHA256SUMS-M; do
    if [ -f "$DIR/$sums" ] && command -v sha256sum >/dev/null 2>&1; then
        if (cd "$DIR" && sha256sum -c "$sums" 2>/dev/null | grep -E "godhand-(B|M)|stage1") >> "$RUNLOG" 2>&1 &&
           ! (cd "$DIR" && sha256sum -c "$sums" 2>/dev/null | grep -E "godhand-(B|M)|stage1" | grep -qv ": OK$"); then
            note "SHA-256 ($sums): confere"
        else note "ERRO: SHA-256 ($sums) não confere - veja $RUNLOG"; exit 1; fi
    else
        note "ERRO: falta $sums"; exit 1
    fi
done
export PS2X_PAD_SCRIPT=""
START_TEMP=$(hottest)
note "temperatura no início: ${START_TEMP} C"
LOGS=""
n=0
for TAG in $PLAN; do
    n=$((n + 1))
    v=$(echo "$TAG" | cut -c1)
    sleep "$COOLDOWN"
    waited=0
    while [ "$(hottest)" -gt $((START_TEMP + 2)) ] && [ "$waited" -lt 120 ]; do sleep 10; waited=$((waited + 10)); done
    note "== execução $n ($TAG): godhand-$v, $SECS s, temperatura antes $(hottest) C"
    GODHAND_BIN="godhand-$v" BENCH_TAG="$TAG" PS2X_PROF=1 sh "$DIR/run_bench.sh" "$ISO" cpu "$SECS" "$STATE" > /dev/null 2>&1
    LOG=$(ls -t "$DIR"/bench-cpu-"$TAG"-*.log 2>/dev/null | head -1)
    if [ -z "$LOG" ]; then note "   ANOMALIA: sem log para $TAG"; continue; fi
    LOGS="$LOGS $LOG"
    note "   $(grep -aE "^\[bench-summary\]" "$LOG" | cut -c1-110) ($(grep -aE "^exit code" "$LOG" | tail -1))"
    grep -qaE "\[crash\]|[Ff]atal|[Ss]egmentation fault|(^|[^A-Za-z_])ERROR([^A-Za-z_]|$)" "$LOG" && note "   ANOMALIA: $TAG tem linhas de erro no log (ver $(basename "$LOG"))"
done
note "== fim; temperatura $(hottest) C"

{
    echo "Linhas de medição de cada execução (M: contagens e GIF; B: só [bench-vu1] não existe, é o binário do pacote)"
    for LOG in $LOGS; do
        echo "== $(basename "$LOG")"
        grep -aE "^\[bench-(summary|sections|vu1|vu1-paths|gif)\]" "$LOG"
    done
} > "$DETAIL"
sh "$DIR/run_ab.sh" --resumo $LOGS > /dev/null 2>&1
LAST=$(ls -t "$DIR"/ab-resumo-*.txt 2>/dev/null | head -1)
[ -n "$LAST" ] && mv "$LAST" "$DIR/meas-resumo-$STAMP.txt"
note "resumo: meas-resumo-$STAMP.txt | detalhe: meas-detalhe-$STAMP.txt"
