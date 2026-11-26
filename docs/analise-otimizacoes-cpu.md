# Otimizações de CPU — análise antes de implementar (Fase 4)

Nada aqui foi implementado. Para cada técnica: onde mudaria, o que pode quebrar, e qual número da
instrumentação (`PS2X_PROF=1`, `[bench-sections]`, `tools/bench_compare.py`) justificaria fazer. A referência é o
PC (luta da fase 1, GameThread ~10,5 ms por field): ee ~25%, vu1 ~23%, dispatch ~12%, sched ~4%, gif ~4%, vif ~2%,
iop ~1%, idle ~29%. A decisão depende dos mesmos números medidos no Cortex-A35.

## 1. Flags do VU1 sob demanda

**O que já existe:** o VU1 pré-compilado já adia o cálculo das flags. Cada operação FMAC grava um registro num anel
(`Vu1FastCtx::flags`, `flagHead`/`flagTail`), e o registro só é consolidado quando o programa lê as flags
(`foldStatus`/`foldStatusSlow`, `runtime/ps2xRuntime/src/lib/vu/ps2_vu1_impl.inl`, ~linha 2939). O que falta da
técnica do PCSX2 (`microVU_Flags.inl`: `mVUsetFlags`, `mVUflagPass`) é **não gravar** o registro quando nenhuma
leitura posterior o usa.

**Onde mudaria:**
- `runtime/ps2xTest/tools/vu1_recomp.cpp`: o gerador dos blocos pré-compilados. Uma análise de vivacidade por bloco
  (quais FMAC têm MAC/status lidos por FMAND/FMEQ/FMOR/FSAND/FSEQ/FSOR/FCAND/FCEQ/FCOR/FCGET ou pelo fim do
  microprograma) e, nas operações sem leitor, omitir o registro no anel.
- `ps2_vu1_impl.inl`: as funções que empurram registros para o anel ganhariam uma variante "sem flags".
- Regenerar `generated/vu1` (5 microprogramas no God Hand) e recompilar.

**Riscos:**
- Uma leitura de flag não vista pela análise: desvio de bloco, flags lidas por outro microprograma, ou o EE lendo
  `VPU_STAT`/flags depois do fim.
- Diferença de 1 ciclo na janela de 4 operações das flags "sticky".
- Mitigação: o PCSX2 conserva sempre as últimas 4 operações antes de um ponto de saída (ver `microVU_Analyze.inl`,
  laço com `calcOPS >= 4`). Validar com a gravação do GS idêntica byte a byte, porque o VU1 escreve os pacotes do GS.

**Contadores que justificariam:**
- `vu1` no `[bench-sections]` do A35 acima de ~20% da GameThread.
- Um contador novo: registros de flag empurrados por segundo contra consolidados. Se poucos são lidos, o ganho
  potencial é proporcional a quanto `foldStatus` e os empurrões pesam no perfil.

## 2. Memória espelhada para o EE

**Hoje:** cada acesso do código gerado testa endereço especial (`PS2Runtime::isSpecialAddress`) e, no caminho
rápido, se o acesso cruza o fim da RAM (`Ps2FastRangeIsContiguous`, em `ps2_runtime_macros.h`). O caso de cruzar
gera código frio grande embutido em cada acesso: vimos 8 `strb` por store de 64 bits, ~28 instruções ARM por
instrução MIPS numa função gerada.

**Onde mudaria:**
- A alocação da RAM em `PS2Memory` (`new uint8_t[ramSize]`, `ps2_memory.cpp` ~linha 610), que passaria a usar
  `memfd_create` + dois `mmap` consecutivos do mesmo arquivo. Um acesso que cruza o fim cai no espelho, com os
  mesmos bytes.
- `Ps2FastRead*`/`Ps2FastWrite*` em `ps2_runtime_macros.h`: remover o caminho de "cruzou o fim".
- Recompilar todo o código gerado: o cabeçalho é incluído pelos 10.602 arquivos, então é uma recompilação longa, mas
  não precisa regenerar.

**Riscos:**
- `memfd_create` exige kernel 3.17+ (o ArkOS usa 4.4: ok) e glibc 2.27+ para o wrapper (ok com 2.30), ou
  `syscall` direto.
- Save states e o depurador que leem `m_rdram` como vetor simples continuam funcionando, porque o primeiro
  mapeamento é contíguo.
- Escrita via espelho precisa aparecer no original: com `MAP_SHARED` do mesmo arquivo, aparece.

**Contadores que justificariam:**
- `ee` alto no A35.
- Tamanho do `.text` do binário: medir antes e depois, porque o ganho esperado é de cache de instruções.
- `bench_compare` com a gravação do GS idêntica.

## 3. Passagens de otimização no código recompilado

**Hoje:** o gerador emite `ctx->pc = ...` antes de cada instrução (`runtime/ps2xRecomp/src/lib/function_emitter.cpp`,
linhas ~193 e ~203); o compilador remove parte, e sobraram 48 de 80 numa função medida. Os registradores ficam em
`ctx->r[]` (memória). O gerador já rastreia registradores constantes (`resolveMemoryAccessHint`,
`updateConstantRegisters`).

**Onde mudaria:**
- `function_emitter.cpp`: emitir `ctx->pc` só antes de chamadas, syscalls, acessos possivelmente especiais e saídas.
- O tradutor de instruções (`cg.translateInstruction`): usar a base constante conhecida para emitir o acesso
  rápido sem o teste de endereço especial, quando a base é RAM comum.
- Regenerar todo o código do jogo e recompilar.

**Riscos:**
- Algum tratador (exceção, syscall, `Load32` especial, `hang_dump`) ler `ctx->pc` esperando o valor da instrução
  atual.
- Rastreamento de constante errado: um endereço especial tratado como RAM é corrupção silenciosa.
- Validação: gravação do GS idêntica, mais um teste de boot completo (título, menu, abertura e luta), já que a
  mudança toca todo o código.

**Contadores que justificariam:**
- `ee` + `dispatch` no A35, somados, acima de ~40%.
- Contagem de instruções ARM por instrução MIPS, um número estático e exato a partir do `objdump` de uma amostra de
  funções, antes e depois.

## 4. VU1 em thread separada com fila SPSC

**Hoje:** o `PS2X_MTVU=1` (`runtime/ps2xRuntime/src/lib/ps2_async_vif.cpp`: `enqueue`, `pipelineBusy`,
`waitPipelineIdle`, `sync`) já executa o VIF1/VU1 em outra thread, mas no PC piorou (97% → 82%): a GameThread
espera a fila com frequência.

**O modelo do PCSX2** (`pcsx2/MTVU.cpp`): anel de palavras de 32 bits com posições atômicas, o EE nunca espera o
fim do microprograma (agenda o fim pela média dos últimos 4, `Get_vuCycles`), e só espera em `WaitOnSize` (anel
cheio) ou `WaitVU` (leitura de estado do VU).

**Onde mudaria:**
- `ps2_async_vif.cpp`: substituir a espera por lote por espera só nos pontos em que o jogo lê estado do VU1
  (`VPU_STAT`, memória do VU1, `FLUSH`/`FLUSHA` do VIF), e estimar o tempo do microprograma para os eventos do EE.
- `EeScheduler`: um evento de "VU1 terminou" com o tempo estimado, em vez de executar em linha.

**Riscos (os mais altos da lista):**
- Mudança de ordem observável pelo jogo. Se o God Hand lê algo que o VU1 escreveu antes do fim estimado, o
  resultado muda e pode afetar jogabilidade.
- O próprio PCSX2 combina isso com um ajuste opcional de ciclos (`EECycleSkip`), sinal de que a estimativa não é
  exata.
- Não iniciar sem autorização (regra combinada).

**Contadores que justificariam:**
- `vu1` alto no A35.
- Um contador novo para a espera do EE pela fila do VIF assíncrono: hoje essa espera aparece como `ee` ou `vif`,
  sem separação. Adicionar `vu-wait` às seções antes de qualquer mudança.
- `bench_compare` com `PS2X_MTVU=1` contra `0` no aparelho.

## 5. Sincronização EE, VU1 e GS

**Hoje:**
- A fila do GS tem limite por blocos (`GSThreadedBackend`, `m_queueChunkLimit`; esperas medidas como `gs-wait`,
  que dá 0% no PC).
- O agendador adia checkpoints (`PS2X_EE_DEFER_CYCLES`, padrão 256) e dorme até o horário real nos waits
  (`EeScheduler`, `hostDeadline`).
- O IOP gira até 2 ms antes de dormir (`PS2X_IOP_SPIN_US`).

**O modelo do Dolphin** (`VideoCommon/Fifo.cpp`: `SyncGPU`, `m_sync_ticks`): a CPU pode ficar adiantada até uma
distância máxima, sem sincronizar a cada comando.

**Onde mudaria:**
- `gs_threaded_backend.cpp`: o limite da fila, se `gs-wait` aparecer no A35.
- `iop_subsystem.cpp`: o `PS2X_IOP_SPIN_US` padrão, se a rodada com 200 µs no A35 não perder velocidade.
- `EeScheduler.cpp`: nada sem evidência; a ordem dos eventos não pode mudar.

**Riscos:**
- Mudar a ordem em que o EE vê interrupções do GS e do IOP muda jogabilidade.
- Mudanças de limite ou de espera só afetam o tempo, não a ordem. São seguras e mensuráveis, uma de cada vez.

**Contadores que justificariam:**
- `gs-wait` maior que 0 no A35.
- `idle` igual a 0 junto com `speed` abaixo de 100% (EE sem folga).
- CPU da IOPThread e temperatura no `[bench-thermal]` com `PS2X_IOP_SPIN_US=200` contra o padrão.

## Ordem sugerida, depois dos números do A35

1. Ajustes só de tempo: `PS2X_IOP_SPIN_US` e o limite da fila do GS, se os contadores indicarem.
2. Memória espelhada (2): não muda a lógica, valida-se byte a byte, e mede-se pelo `.text` e pelo `ee`.
3. Flags sob demanda (1), se `vu1` pesar.
4. Passagens no gerador (3), se `ee`/`dispatch` pesarem: é a mudança de maior alcance.
5. VU1 em thread com SPSC (4): por último e só com autorização, depois de medir `vu-wait`.
