# Regras, empacotamento e critérios de aceitação do PortMaster para ports de jogos comerciais de PS2 (decomp/recomp), em 2026-11-27

Método: clones rasos e esparsos de `PortsMaster/PortMaster-New` (commit de 2026-11-27), `PortsMaster/PortMaster-GUI` (2026-11-25), `PortsMaster/PortMaster-Website` (2026-10-05, fonte de portmaster.games) e `PortsMaster-MV/PortMaster-MV-New` (2026-11-18), mais a busca da API REST do GitHub e as páginas de PR via WebFetch. O domínio portmaster.games **não resolve nesta sessão** (fora da lista de rede permitida). Por isso as páginas do site foram lidas no repositório-fonte `PortMaster-Website/website/*.html`, que pode estar até cerca de 7 semanas atrás do site publicado. Não consegui ler o Discord.

## 1. Política legal e de conteúdo: arquivos do usuário, rtr, código protegido, binários decomp/recomp

### Takeaway
O PortMaster só distribui o que pode ser distribuído legalmente. Jogos comerciais entram com `rtr: false`, e o usuário fornece os dados (ISO/ROM) com instruções de obtenção legal. Não há regra escrita proibindo binários compilados a partir de decomp ou recomp. Na prática houve um caso de cada lado:
- **nfshp2**: decomp de PS2 da EA com binário pronto. Foi aceito no repositório principal em 2026-11-24.
- **DaytonaArm**: recomp estático. Envia só o runtime "ROM-free", e o usuário gera o código traduzido no PC. Está em rascunho e a dúvida "isso é aceitável?" segue sem resposta.

A documentação oficial manda ports "Nintendo, Capcom, etc." e "decomp" para o repositório **Multiverse (PortMaster-MV-New)**. Isso pesa diretamente sobre God Hand (Capcom/Clover).

### Cited Findings
- **Redirecionamento para o Multiverse.** O guia de empacotamento diz, na seção de PR: "Fork the main repo: https://github.com/PortsMaster/PortMaster-New. If your port is Nintendo, Capcom, etc. related, or a decomp for example, fork https://github.com/PortsMaster-MV/PortMaster-MV-New instead." — [PortMaster-Website/website/packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **O que é o Multiverse.** README do MV: "This is a repo for hosting ports unable to be included in the main repositories." Ele tem 45 ports, entre eles soh, soh2, sm64coopdx, zelda3, gta3, gtavc, megamanw, megamanxregenesis, dusklight (decomp de Twilight Princess que pede o ISO de GC/Wii do usuário) e melee. — [PortMaster-MV-New README](https://github.com/PortsMaster-MV/PortMaster-MV-New)
- **O MV é fonte padrão na GUI.** O cliente traz `021_portmaster.multiverse.source.json` ("PortMaster Multiverse") como fonte padrão, ao lado da principal. Ports do MV aparecem para o usuário final, com selo "Multiverse" no site. — [PortMaster-GUI harbourmaster/config.py](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/pylibs/harbourmaster/config.py); [PortMaster-Website js/games_old_search.js](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/js/games_old_search.js)
- **FAQ do site** (trecho de busca, não verificado na página porque o domínio está bloqueado): "PortMaster installs the port, you provide the game. We don't distribute copyrighted game files and we don't support piracy." — [portmaster.games/faq](https://portmaster.games/faq)
- **AGENTS.md, "Hard nos":** "DRM-bypass/Steam-emulator tooling, and dropping the original porter's credit when building on their work." — [PortMaster-New AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Ativos protegidos: PR #518** (Legend of Sword and Fairy, 2024). Fechado sem merge. kloptops: "the sdlpal repo mentions that the assets for the game are copyrighted so that means without any form of LICENSE / permission for us to distribute them", e "All copyrighted assets must be removed". O histórico git também precisava ser limpo. — [PR #518](https://github.com/PortsMaster/PortMaster-New/pull/518)
- **Obtenção legal: PR #2689** (GTA SA, 2026-09/10). Fechado sem merge.
  - Cebion: "make sure that you put instructions on how to obtain the game files legally… If we can't provide that we unfortunetaly can not put the Port on Portmaster."
  - Slayer366 sugeriu "place it in MV instead".
  - Cebion ofereceu hospedar no "MultiVerse Repo" se houvesse modo legal e fácil de obter os dados.
  - O port já excluía `libGame.so` e os ativos da Rockstar. — [PR #2689](https://github.com/PortsMaster/PortMaster-New/pull/2689)
- **Precedente PS2 decomp: nfshp2 (PR #2890).**
  - Datas e revisão: aberto em 2026-11-23, merge de Cebion em 2026-11-24, aprovações de Cebion e Ganimoth. Nenhum comentário de discussão e nenhuma objeção legal registrada.
  - Escopo: "native port of the PS2 version… adapted from the New 3DS port", feito a partir da decomp de Octoblimp.
  - O que vai no pacote: o binário pronto `nfshp2.armhf` (ELF ARM 32 bits, ~1,6 MB) compilado do código decompilado, a licença GPL-3.0 e scripts Python de instalação. O README diz "The package holds no game data".
  - O que o usuário fornece: o `.iso` do próprio disco. — [PR #2890](https://github.com/PortsMaster/PortMaster-New/pull/2890); [ports/nfshp2](https://github.com/PortsMaster/PortMaster-New/tree/main/ports/nfshp2)
- **Conflito.** O nfshp2 é decomp de jogo comercial e entrou no repositório principal, ao contrário do texto que manda "a decomp for example" ao MV. O perfectdark (decomp de N64 da Nintendo/Rare, com ROM do usuário verificada por md5) também está no principal. A regra do MV, portanto, não é aplicada de forma uniforme e parece pesar mais para IPs de editoras litigiosas (Nintendo, Capcom). — [ports/perfectdark/port.json](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/perfectdark/port.json); [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Precedente recomp estático: DaytonaArm (PR #2886, aberto, rascunho).**
  - O pacote leva só o runtime "ROM-free" `daytona.aarch64`. O usuário fornece os sets MAME e compila no PC os módulos `cache/daytona-aarch64.so` com `tools/build-arm64-cache.sh`. Marcado `rtr: false`, `exp: true`.
  - O autor perguntou se o modelo "install-then-compile" é aceitável. Até a coleta, a única resposta foi Cebion exigindo um tópico de testes no Discord antes da revisão.
  - O PR #2885 foi fechado só por defeito do branch. — [PR #2886](https://github.com/PortsMaster/PortMaster-New/pull/2886)
- **Bully (PR #2564, merge em 2026-07-15).** Port de Android, não de PS2, mas aceito com dados do usuário (APK próprio). A revisão de bmdhacks focou em verbosidade: "a very complicated and verbose AI-generated port". O script caiu de ~300 para ~100 linhas antes da aprovação. — [PR #2564](https://github.com/PortsMaster/PortMaster-New/pull/2564)
- **Recomp com PS2Recomp em outro projeto.** O BT3-Recomp (DBZ BT3, SLUS-21678), em GPL-3.0, diz que o repositório "contains no game code" e que o código MIPS é traduzido "at build time, from your own disc image". Não é port PortMaster. O README é ambíguo sobre se os binários de release contêm código traduzido. — [BT3-Recomp README](https://raw.githubusercontent.com/z3xox/BT3-Recomp/HEAD/README.md)
- **Estado do PS2Recomp.** O projeto (ran-j) é descrito pela imprensa em 2026 como experimental, recompilando MIPS R5900 para C++. — [Time Extension, 2026-01](https://www.timeextension.com/news/2026/01/native-pc-ports-of-ps2-games-could-be-on-the-way-thanks-to-new-recompilation-experiment); [Android Authority](https://www.androidauthority.com/ps2recomp-3635482/)

### Inferences
- **Diferença jurídica.** O C++ gerado pelo PS2Recomp a partir do ELF de God Hand é uma tradução mecânica do código da Capcom, e o binário compilado dele é obra derivada desse código. Isso é diferente de um decomp "limpo" (nfshp2) só em grau, não em natureza: ambos vêm do código do jogo. O nfshp2 abre o precedente mais favorável para um binário pronto. Já o DaytonaArm mostra a alternativa mais conservadora, que o próprio autor tratou como questão em aberto: o pacote leva só o runtime GPL, e a tradução é feita a partir do ISO do usuário.
- **Repositório.** God Hand é IP da Capcom, citada nominalmente na regra do MV. O destino mais provável é o **PortMaster-MV-New**, ou pelo menos os revisores vão perguntar isso. Submeter ao principal pode funcionar (como o nfshp2), mas o risco de "place it in MV instead" é alto.
- **Opção mais defensável:** distribuir só o runtime e as ferramentas (GPL-3.0) e gerar o código no PC ou no dispositivo a partir do ISO, ou um binário que carrega um módulo `.so` gerado pelo usuário. Para isso seria preciso o PS2Recomp, um compilador C++ e muito tempo de CPU no dispositivo, o que é inviável num RK3326 de 1 GB. Na prática fica "gerar no PC", como no DaytonaArm.
- **Opção mais simples:** enviar o binário pronto, como o nfshp2. Tem precedente, mas não há declaração oficial de que código traduzido de jogo seja aceitável.
- **GPL-3.0 e direitos da Capcom.** A GPL-3.0 do PS2Recomp obriga a disponibilizar o fonte do runtime e da cola. Publicar o C++ gerado (que é código da Capcom) seria problemático legalmente, e a GPL não resolve isso. O nfshp2 contornou a questão publicando a decomp em repositório próprio (manuelpllull/NFSHP2-R36S).

### Gaps
- Não há política escrita da equipe PortMaster específica sobre "binários recompilados que embutem código traduzido". A resposta ao PR do DaytonaArm não existia na coleta.
- A FAQ (portmaster.games) não pôde ser lida diretamente, porque o domínio está bloqueado na sessão. A citação vem de trecho de busca.
- As discussões do Discord (#testing-n-dev, tópico do nfshp2) não puderam ser lidas.
- Não achei nenhum PR, issue ou post de Reddit sobre port PortMaster baseado em PS2Recomp. A busca nos orgs PortsMaster e PortsMaster-MV por "PS2Recomp", "God Hand" e "recomp" só retornou o DaytonaArm.

## 2. Formato de empacotamento: port.json, gameinfo.xml, screenshot, script, runtimes, tamanho, testes e revisão

### Takeaway
A estrutura obrigatória é `portname/` com `port.json`, `README.md`, `screenshot.{jpg,png}` (4:3, 640x480), `gameinfo.xml`, `Port Name.sh` e a subpasta `portname/` (com `licenses/`), mais `cover` opcional.

O script segue um molde fixo:
- localização do `controlfolder` e `source control.txt`;
- `mod_${CFW_NAME}.txt` e `get_controls`;
- `$GAMEDIR`, log e `$GPTOKEYB`/`$GPTOKEYB2`;
- `pm_platform_helper`, execução do binário e `pm_finish`.

Arquivos com mais de 90 MB são divididos em pedaços. Os testes precisam estar documentados num tópico no Discord (#testing-n-dev), cobrindo AmberELEC, dArkOS, muOS, ROCKNIX (Libmali e Panfrost) e Knulli. O merge exige revisão humana, com pelo menos 2 aprovações. Ports verbosos gerados por IA são rejeitados.

### Cited Findings
- **Nome e estrutura.** O nome do port começa com [a-z0-9] e aceita só [a-z0-9._]. O script tem maiúsculas e termina em `.sh`, e a pasta do port tem o mesmo nome da pasta-mãe. Scripts e pastas devem ser únicos no projeto. — [PortMaster-New README](https://github.com/PortsMaster/PortMaster-New/blob/main/README.md); [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Exemplo oficial de port.json (Defblade)**, `version: 4`, com os campos `attr`:
  - `title`, `porter` (lista), `desc`, `desc_md`, `inst`, `inst_md`, `genres` (lista), `image`;
  - `rtr` (bool), `exp` (bool), `runtime` (lista), `store` (lista);
  - `availability` (ex.: "full"), `reqs` (lista), `arch` (lista), `min_glibc`.
  - O guia diz: "If the game is using a runtime e.g. Godot/Mono/Java no arch needs to be entered." — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Valores reais nos 1.522 port.json do repositório principal** (contagem própria):
  - `rtr`: false em 787, true em 734.
  - `availability`: None em 814, full em 291, paid em 290, free em 107, demo em 17.
  - `arch`: aarch64 em 935, vazio (runtime) em 259, armhf em 204, aarch64+x86_64 em 84.
  - `reqs` mais usados: `!lowres` (59), `power` (58), `analog_2` (38), `2gb` (4), `ultra` (1).
  - Runtimes mais usados: `weston_pkg_0.2.squashfs` (78), `frt_3.5.2` (73), `gmtoolkit` (51), `dotnet-8.0.12` (47), `mono-6.12.0.122-aarch64` (33). — [PortMaster-New/ports](https://github.com/PortsMaster/PortMaster-New/tree/main/ports)
- **port.json do nfshp2:**
  - Identificação: `version 4`, `"rtr": false`, `"exp": false`, `"genres": ["racing"]`, `"porter": ["manuelpllull"]`.
  - Requisitos: `"runtime": []`, `"reqs": []`, `"arch": ["armhf"]`, `"min_glibc": "2.17"`.
  - `inst` lista os IDs de disco aceitos (SLUS-20362, SLES-50731, SLPM-65181, SLPM-67527), o espaço livre (3 GB), o tempo (~10 min) e a dependência de python3. — [ports/nfshp2/port.json](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/port.json)
- **Validação dos requisitos.** `attr.runtime` deve ser um array JSON com chaves que existem em `runtimes/runtimes.json`. O script monta e desmonta o squashfs, e o harbourmaster só baixa. — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Como a GUI aplica os requisitos:**
  - `min_glibc` maior que a glibc do dispositivo oculta o port.
  - Com `runtime`, a arquitetura vem do runtime. Sem runtime, `arch` vira um requisito "a|b".
  - Prefixo `!` nega um requisito, e `|` significa OU. — [hm_ports.py match_requirements](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/pylibs/harbourmaster/hm_ports.py); [util/data.py](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/pylibs/harbourmaster/util/data.py)
- **README.md do port.** Deve ter "Notes" com agradecimento dedicado ao desenvolvedor ou criador, uma tabela de "Controls" e, de preferência, "Compile". — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Screenshot e cover.** A screenshot deve "Show gameplay… not just the title screen", usar 4:3 com no mínimo 640×480 e mostrar o letterbox quando houver. O AGENTS.md pede redimensionar para 640×480. A cover deve mostrar gameplay junto com boxart ou logo. Sem cover, a GUI usa a screenshot. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html); [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **gameinfo.xml.** Formato EmulationStation `<gameList><game>` com `path`, `name`, `desc`, `releasedate`, `developer`, `publisher`, `genre` e `image`. O CI falha se ele estiver dessincronizado com o disco. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html); [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Licenças.** Uma pasta `licenses/` plana, com um `LICENSE.<componente>.txt` por componente (incluindo gptokeyb, gl4es, box64, .so, fontes e ativos). — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md); [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Script de lançamento (modelo oficial):**
  1. Detecção do `controlfolder`: `/opt/system/Tools/PortMaster`, `/opt/tools/PortMaster`, `$XDG_DATA_HOME/PortMaster` ou `/roms/ports/PortMaster`.
  2. `source $controlfolder/control.txt`, que fornece `$ESUDO`, `$directory`, `$param_device` e a configuração SDL.
  3. `export PORT_32BIT="Y"`, só se o port for 32 bits.
  4. `[ -f mod_${CFW_NAME}.txt ] && source ...`, depois `get_controls`.
  5. `GAMEDIR=/$directory/ports/<port>` e o log `> "$GAMEDIR/log.txt" && exec > >(tee ...) 2>&1`.
  6. `XDG_DATA_HOME` ou `bind_directories` para os saves.
  7. `LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}"` e `SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"`.
  8. `libgl_${CFW_NAME}.txt` se houver gl4es.
  9. `$GPTOKEYB "exe" -c "./x.gptk.$ANALOG_STICKS" &`, depois `pm_platform_helper`, a execução de `./exe.${DEVICE_ARCH}` e `pm_finish`.
  - O RAM do dispositivo fica disponível em `$DEVICE_RAM` (GB) para lógica condicional (ex.: `< 2`). — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Regras de alto impacto no script:**
  - Line endings LF.
  - Nunca empacotar SDL2, libc/libstdc++, libGL/EGL/mesa. Empacotar só o que o `ldd` provar que falta.
  - Terminar com `pm_finish`, sem `trap` nem `pkill`.
  - Usar `$ESUDO`, `bind_directories`/`bind_files`. O script deve ser agnóstico de CFW.
  - Não fazer `chmod +x` no `.sh` (fica 644 no repositório) e corrigir o bit de execução do binário em tempo de execução.
  - Um branch por port, e correções vão no mesmo PR. — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Primeira execução com ISO: modelo nfshp2.**
  - Se `gamedata/NFSHP2/GLOBAL` não existe, o script procura `*.iso` em `gamedata/`.
  - Sem ISO, mostra `pm_message` e sai.
  - Com ISO, define `PATCHER_FILE`, `PATCHER_GAME`, `PATCHER_TIME` e faz `source "$controlfolder/utils/patcher.txt"`, que é o instalador com UI do PortMaster. Ele roda `tools/patchscript`, um script Python que extrai o ISO, e grava `patchlog.txt`.
  - Depois vêm `$GPTOKEYB2`, `pm_platform_helper` e `./nfshp2.armhf`.
  - Também foram acrescentadas linhas `SDL_GAMECONTROLLERCONFIG` para pads específicos (R36S "GO-Super Gamepad", Deeplay, muOS). — [ports/nfshp2/NFSHP2.sh](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/NFSHP2.sh)
- **Instruções de geração em instaladores.** O guia mostra uma função `install()` que usa `xdelta3` do controlfolder e um marcador `.installed`. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **Tamanho.** Arquivos maiores que 90 MB devem passar por `tools/build_data.py`, que divide em pedaços de 50 MB e adiciona ao `.gitignore`. O limite está no código: `LARGEST_FILE = 1024*1024*90`. Não achei limite de tamanho total por port. — [README](https://github.com/PortsMaster/PortMaster-New/blob/main/README.md); [tools/build_release.py](https://github.com/PortsMaster/PortMaster-New/blob/main/tools/build_release.py)
- **Pré-checagem local:** `python3 tools/build_release.py --do-check`. — [README](https://github.com/PortsMaster/PortMaster-New/blob/main/README.md)
- **Testes.** "Pull Requests submitted without documented testing in the #testing-n-dev channel will not be accepted."
  - CFWs: AmberELEC, dArkOS, MuOS, ROCKNIX (Libmali, Panfrost, Adreno opcional) e Knulli.
  - Resoluções: 640x480 e "Higher resolutions (e.g., 1280)", com 480x320 e 720x720 opcionais. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
  - O template de PR repete esse checklist e acrescenta: "I wrote and understand this script/patch myself, or clearly marked which parts came from an AI assistant" e "I can explain every non-standard line". — [PULL_REQUEST_TEMPLATE.md](https://github.com/PortsMaster/PortMaster-New/blob/main/.github/PULL_REQUEST_TEMPLATE.md)
  - O template do MV pede AmberELEC, ArkOS, ROCKNIX e muOS, com 480x320, 640x480 e 720x720. — [MV PR template](https://github.com/PortsMaster-MV/PortMaster-MV-New/blob/main/.github/PULL_REQUEST_TEMPLATE.md)
- **Rejeição de ports de IA.** "Overly verbose, AI-generated ports are the current #1 rejection reason and are rejected without discussion." Um port focado em um só dispositivo também é rejeitado: "If you have exclusively focused on one specific device we will reject your port submission." — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Revisão.**
  - O PR é checado por CI e depois revisado à mão pela equipe. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
  - Onze code owners são marcados automaticamente, e o PR do DaytonaArm indica que pelo menos 2 aprovações são necessárias. — [PR #2886](https://github.com/PortsMaster/PortMaster-New/pull/2886)
  - O nfshp2 foi aprovado por 2 (Cebion e Ganimoth) em cerca de 1 dia. Só as caixas do ROCKNIX apareciam marcadas no checklist. — [PR #2890](https://github.com/PortsMaster/PortMaster-New/pull/2890)
- **Molde de script.** Copiar a estrutura de um port recente e popular do mesmo tipo (nativo, Godot, Ren'Py, mono). Clonar o PortMaster-GUI para entender as funções. — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)

### Inferences
- **Para God Hand:**
  - `rtr: false` e `availability` omitido ou "full".
  - `inst` citando SLUS-21503 e, se possível, um hash do ISO ou do ELF, à maneira do md5 do perfectdark.
  - `genres: ["action"]` e `runtime: []`.
  - `arch: ["aarch64"]`. Ou `armhf`, se o binário for 32 bits como o nfshp2, com `export PORT_32BIT="Y"`.
  - Considerar `min_glibc` de acordo com o toolchain usado.
- **Primeira execução.** O fluxo "ISO em `gamedata/`, `patcher.txt` na primeira execução, extração" do nfshp2 é o molde mais próximo e mais recente, e a equipe já aceitou esse fluxo para um jogo de PS2.
- **Tamanho do pacote.** Um binário recompilado de PS2 pode passar de 90 MB e exigir `build_data.py`. Isso funciona, mas pesa no download e na revisão.
- **Script curto.** O script deve ficar próximo das ~70 linhas do NFSHP2.sh. Um script muito maior tende a ser recusado.

### Gaps
- Não encontrei a lista oficial de valores aceitos para `availability`. A contagem mostra full, paid, free e demo. Também não achei o significado oficial de `exp` (experimental) nem de `store`.
- Não achei um limite formal de tamanho por pacote além da regra de 90 MB por arquivo.
- Não consegui ler o conteúdo exato de `runtimes/runtimes.json` nem a documentação de runtimes do site, por isso não detalho westonpack e mono.

## 3. Cobertura de dispositivos/CFW: RK3326, 640x480, RAM mínima e o requisito "power"

### Takeaway
As capacidades vêm de `device_info.txt` na GUI, e o significado de cada uma está no código.
- **"power"**: o dispositivo **não** é RK3326/PX30. Um port com `reqs: ["power"]` some da lista no R36S.
- **Níveis de RAM**: `1gb`, `2gb`, `4gb`… pelo RAM detectado.
- **"ultra"**: 4 GB ou mais e um SoC fora de uma lista de chips fracos (rk3326, h700, a133…).
- **Resolução**: "lowres" (<640x480), "hires" (≥960 de largura ou ≥720 de altura), "wide", "square" e "WxH".

Não existe um campo para "declarar suporte a RK3326". O suporte é implícito pela ausência de `power`, `2gb` e `ultra` e por `arch` compatível.

### Cited Findings
- **Trechos de `device_info.txt`:**
  - `case "$CPU_LOWER" in *rk3326*|*px30*) ;; *) add_cap "power" ;; esac`
  - `ultra` só se `DEVICE_RAM -ge 4` e o CPU não for rk3326, h700, a133, a133plus, a527, px30, sun50iw9 ou sun50iw10.
  - `for tier in 1 2 4 8 16 32; do [ "$DEVICE_RAM" -ge "$tier" ] && add_cap "${tier}gb"`.
  - `hires` se largura ≥ 960 ou altura ≥ 720; `lowres` se largura < 640 ou altura < 480.
  - Também `opengl` (GL desktop real, sem gl4es nem mali), `vulkan`, `analog_N`, `armhf`/`aarch64`, `touch`, `rumble`. — [PortMaster-GUI device_info.txt](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/device_info.txt)
- **Uso do "power" na GUI.** Ports com `power` aparecem como "Title_P" e com `opengl` como "Title_F" na saída `portmd`. A lista de capacidades usada nos temas é `["aarch64","armhf","power","opengl","2gb","1gb","analog_0","analog_1","analog_2"]`. — [hm_ports.py](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/pylibs/harbourmaster/hm_ports.py); [theme_server/worker.py](https://github.com/PortsMaster/PortMaster-GUI/blob/main/PortMaster/pylibs/theme_server/worker.py)
- **Uso real no repositório principal.** 58 ports usam `power`, 4 usam `2gb` e 1 usa `ultra`. — [PortMaster-New/ports](https://github.com/PortsMaster/PortMaster-New/tree/main/ports)
- **SoCs que o PortMaster lista para teste:** rk3326 (Mali G31 MP2), rk3566 (G52 MP2), h700 (G31 MP2), a133p (PowerVR GE8300), Snapdragon 865 e 662, e S922X. — [AGENTS.md](https://github.com/PortsMaster/PortMaster-New/blob/main/AGENTS.md)
- **Resolução nos testes.** 640x480 é obrigatória e alta resolução também. A screenshot é 640×480. — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)
- **RK3326 de 32 bits.** O nfshp2 declara `arch: ["armhf"]`, `reqs: []` e diz "Tested on an R36S with dArkOS". Ou seja, foi aceito sem `power` e roda em RK3326. — [ports/nfshp2/README.md](https://github.com/PortsMaster/PortMaster-New/blob/main/ports/nfshp2/README.md)
- **RAM condicional em script.** `$DEVICE_RAM` está disponível no script (ex.: aplicar um patch de pouco RAM se `-lt 2`). — [packaging.html](https://github.com/PortsMaster/PortMaster-Website/blob/main/website/packaging.html)

### Inferences
- **R36S (RK3326, 1 GB).** Para que God Hand apareça no R36S, o port.json **não** pode ter `power`, `2gb` nem `ultra`. Se o port só for jogável em chips mais fortes, `power` é o requisito honesto, e ele exclui justamente o R36S.
- **Testes além do R36S.** Mesmo mirando o R36S, os revisores exigem testes em vários CFWs e resoluções (dArkOS, ROCKNIX, muOS, Knulli, AmberELEC; 640x480 e 1280x720). Um port testado só no R36S corre risco de ser rejeitado por focar em um único dispositivo.
- **640x480.** O caso nativo do R36S é 640x480. O PS2 renderiza em torno de 640x448/512 e escalar para 4:3 é natural, mas o port também precisa lidar com 720x720 e 16:9, mesmo que com letterbox.

### Gaps
- Não há documentação textual oficial dos significados de `power`, `ultra` e outros. Inferi tudo do código-fonte da GUI (`device_info.txt` de 2026-11-25), que pode mudar.
- Não encontrei regra de RAM mínima para aceitação. O requisito é só declarativo, via `1gb`/`2gb`.

## 4. Discussões públicas sobre ports de recompilação de PS2 (2025–2026)

### Takeaway
Não encontrei nenhuma submissão PortMaster de port baseado em PS2Recomp, nem de God Hand. Os precedentes relevantes são:
- **nfshp2**: decomp de PS2, com merge em 2026-11-24, sem discussão legal.
- **DaytonaArm**: recomp estático de arcade, em rascunho e sem resposta sobre o modelo "compile seus próprios módulos".

O PS2Recomp em si é noticiado como experimental em 2026.

### Cited Findings
- **Busca na API do GitHub** em PRs e issues de `org:PortsMaster` e `org:PortsMaster-MV`. "PS2Recomp" e "God Hand" não retornaram nada. "recomp" retornou só DaytonaArm (#2885 e #2886). "PS2" retornou nfshp2 (#2890) e Bully (#2564, que não é PS2). — [GitHub search](https://github.com/search?q=org%3APortsMaster+recomp&type=pullrequests)
- **nfshp2 (#2890).** Merge sem comentários. O commit tem o trailer "Co-Authored-By: Claude Opus 5.5", ou seja, assistido por IA e aceito mesmo assim. — [PR #2890](https://github.com/PortsMaster/PortMaster-New/pull/2890)
- **DaytonaArm (#2886).** O autor perguntou se "install-then-compile" é aceitável e, se não for, manteria o port auto-hospedado. Cebion pediu um tópico de testes no Discord, e o PR virou rascunho. — [PR #2886](https://github.com/PortsMaster/PortMaster-New/pull/2886)
- **Cobertura do PS2Recomp.** Imprensa e blogs descrevem o PS2Recomp como "experimento" e projeto de estágio inicial. — [Time Extension](https://www.timeextension.com/news/2026/01/native-pc-ports-of-ps2-games-could-be-on-the-way-thanks-to-new-recompilation-experiment); [retrohandhelds.gg](https://retrohandhelds.gg/ps2recomp-porting-playstation-2-games-to-pc/)

### Inferences
- **Primeiro do tipo.** Um port de God Hand via PS2Recomp seria provavelmente o primeiro recomp de PS2 submetido ao PortMaster. Espere perguntas sobre:
  1. MV ou principal (Capcom);
  2. o binário conter código traduzido do jogo;
  3. a verificação do ISO (SLUS-21503);
  4. testes multi-CFW no Discord.
- **Abordagem pragmática:** abrir o tópico no #testing-n-dev antes do PR e perguntar ali qual dos dois modelos é aceitável e em qual repositório:
  - "binário pronto + ISO" (modelo nfshp2);
  - "runtime + módulo gerado pelo usuário" (modelo DaytonaArm).

### Gaps
- O Reddit e o Discord não foram consultados diretamente; o Discord é inacessível. Não posso afirmar que não houve discussão informal lá.
- A resposta final ao PR #2886 era desconhecida em 2026-11-27.
