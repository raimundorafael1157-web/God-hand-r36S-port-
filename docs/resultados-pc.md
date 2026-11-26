# Resultados no PC (26/11/2026)

Todos os números abaixo foram medidos neste servidor: Intel Xeon a 2,1 GHz, 4 núcleos virtuais, Linux x86_64,
gráficos desligados (GS nulo) e o disco verificado (SHA-256 `268b8e28…0a7c`). Não são números do R36S.

## O que foi verificado

| Verificação | Resultado |
| --- | --- |
| Geração do código a partir do disco | 10.602 arquivos + 81 módulos, executável confere (`1742f95b…`) |
| Build PC Linux | Compila sem erros, 111 MB |
| Build ARM64 (R36S) | Compila sem erros, glibc ≤ 2.30, 88 MB sem símbolos de depuração |
| Jogo no PC | Logos, título, menu, NEW GAME, abertura e fase 1 com luta, sem erros |
| Jogo no ARM64 (emulador qemu) | Mesma sequência até a luta, sem erros nem funções faltando |
| Save state PC → ARM e ARM → PC | Carrega nos dois sentidos; a luta continua coerente nas imagens |
| Imagens (renderizador de CPU) | Título, menu, abertura e luta corretos |
| FPS próprio do God Hand | 30 quadros por segundo (2 campos por quadro) |
| Memória | ~110 MB no título, ~136 MB na luta (PC). No ARM medido só no emulador, que soma o próprio consumo |

## Velocidade (GS nulo, PC)

| Trecho | Velocidade média | 5% piores | Thread do jogo |
| --- | --- | --- | --- |
| Título | 100% | 100% | ~30% |
| Abertura, VU1 interpretado | 10–23% | — | 100% |
| Abertura, VU1 pré-compilado | 93–100% | — | 60–95% |
| Primeira luta da fase 1 (save state) | 97% | 90% | 99% |

Na luta, mesmo este Xeon fica no limite com a thread do jogo a 99%.

## O que mudou por causa destes testes

- **VU1 pré-compilado é obrigatório.** Interpretado, ele sozinho derruba a abertura para 10–23% até no PC.
  Passo `build_rk3326.sh vu1`.
- **GS nulo numa thread própria.** A primeira versão fazia as cópias de textura na thread do jogo e mostrava a luta
  a 82%; na thread própria, como no runtime normal, 97%. Era erro de medição, corrigido.
- **Quadros do jogo pela cópia de tela.** O God Hand nunca troca o DISPFB; ele copia o quadro pronto. O contador
  agora observa a função que monta essa cópia (`func_0030B870`).
- **Espera do IOP.** No ARM a espera chamava o sistema a cada volta; agora usa a instrução `yield`. Com 0,2 ms de
  espera em vez de 2 ms, a velocidade ficou igual e o consumo caiu 8 pontos (122% → 114%).
- **VU1 em outra thread (`PS2X_MTVU=1`)** piorou no PC (97% → 82%): as threads esperam umas pelas outras. Fica como
  teste no R36S, não como padrão.

## Inferência para o R36S (não medida)

A luta precisa de ~1 núcleo inteiro deste Xeon para ficar perto de 100%. Um Cortex-A35 a ~1,5 GHz executa em
ordem e é, por estimativa de memória e sem fonte, de 5 a 8 vezes mais lento por núcleo que este Xeon. Se for isso,
a luta rodaria a algo como 12–20% da velocidade no R36S, só com a lógica, antes de qualquer renderizador. Isso
apontaria para no-go no caminho atual. Só a medição no aparelho confirma ou derruba.

## Revisão crítica do código (26/11, manhã)

| Problema | Gravidade | Correção |
| --- | --- | --- |
| `setup_base.sh` rodado de novo apagaria `build-host`, `build-x64` e `build-rk3326` (horas de compilação) | Alta | A limpeza preserva `build*` |
| `pip install --user` é recusado no Ubuntu 23.04+ (PEP 668): o build no WSL pararia | Alta | Tenta de novo com `--break-system-packages` (só afeta a pasta do usuário) |
| Biblioteca gráfica faltando no aparelho impediria o programa de abrir, mesmo sem desenhar | Média | Bibliotecas vazias em `stubs/`, usadas só quando falta alguma. Testado: a luta roda só com elas |
| O segundo do carregamento do save state aparecia como ~7800% de velocidade | Média | Marcado "(state load, left out)" e fora do resumo |
| Um segundo inteiro sem quadro novo mostrava "worst=0ms" | Média | Conta o tempo desde o último quadro |
| `make_sysroot.sh` quebraria o `apt update` no Ubuntu 22.04 | Média | Restringe também o `sources.list` antigo a amd64 |
| Teste do módulo de medição não compilava desde o contador de quadros | Média | Atualizado e conferindo quadros e pior intervalo |
| Duas threads com o nome "GameThread" no log (a do IOP herdava o nome) | Baixa | A do IOP se chama `IOPThread` |
| Contador sem uso no GS nulo | Baixa | Removido |
