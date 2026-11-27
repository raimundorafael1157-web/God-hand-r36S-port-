#!/bin/bash
# God Hand R36S: a GPU desenhando a luta gravada (fight.gsr), poucos minutos, sem e com o hash XXH3.
. "$(cd "$(dirname "$0")" && pwd)/godhand-teste/_comum.sh"
prepare_run_dir
[ -f "$RUN/fight.gsr" ] || { msg "fight.gsr não está no pacote: teste da GPU pulado."; sleep 10; exit 0; }
msg "God Hand: teste da GPU (replay da luta)..."
(cd "$RUN" && $SUDO sh ./run_gles.sh fight.gsr 3 2>&1 | tee "$TTY" > /dev/null)
(cd "$RUN" && GLES_PAGE_HASH=xxh3 $SUDO sh ./run_gles.sh fight.gsr 3 2>&1 | tee "$TTY" > /dev/null)
bring_back
msg "Pronto: ports/godhand-teste/gles-*.log"
sleep 10
