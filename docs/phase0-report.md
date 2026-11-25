# Fase 0: plano, estimativa preliminar e recomendação

Data: 25/11/2026. Nenhuma medição foi feita ainda: não há acesso ao aparelho nem ao ISO.
Tudo abaixo está marcado como **fato** (lido em código/documentação), **inferência** ou **suposição**.

## Resumo

- **Recomendação preliminar: provável no-go para a meta mínima (20 FPS), a confirmar com medição.**
- **Fato:** já existe um port ARM64 (Android, GPL-3.0, descontinuado). Num celular cerca de 10x mais forte em CPU
  e 40x mais forte em GPU, ele chegou a ~42–58 campos/s em cenas cheias, ainda com travadas
  (ver `research/prior-art.md`).
- **Inferência:** escalando isso para o RK3326, a CPU sozinha daria algo como 3–8 campos/s em luta, muito abaixo
  de 20 FPS. Margem de erro grande (até ~3x), porque não sabemos se o celular estava limitado por CPU ou por GPU.
- **Mudança proposta no método:** medir direto no R36S com um build ARM64 headless (sem tela), em vez de medir no
  PC e dividir por uma razão de benchmark. O port Android já resolveu o ARM64, então esse build é barato.
  Isso exige escrever código de build, o que o prompt adia para depois da Fase 0: **precisa da sua autorização.**

## O que preciso de você

1. Saída de `scripts/collect_hw_info.sh` rodado no R36S (ver `docs/hardware.md`).
2. Decidir o caminho da medição:
   - **A (prompt original):** você roda o build Windows no seu PC, com instrumentação, e envia os logs.
   - **B (recomendado):** eu preparo um build Linux ARM64 headless a partir do fork Android; você copia para o
     R36S, roda com o seu ISO e envia o log. Mede a CPU real no aparelho, sem estimativa.
3. O ISO fica só com você. Nenhum dos caminhos me envia arquivos do jogo.

## Plano da Fase 0

| Passo | O quê | Como | Fato ou a descobrir |
| --- | --- | --- | --- |
| 1 | Resolução interna 1x | `godhand.ini`, `[Video] InternalResolution=1` (512x448) | Fato: ajuste localizado |
| 2 | Tempo por componente | Reaproveitar `[frame-split]`, `[iop-thread]` e `[gs-wait]` do port Android; adicionar contadores para EE recompilado, VU1, SPU2, DMA e I/O | A implementar |
| 3 | Cenas | Título; luta com 4+ inimigos (fase 1); cutscene; boss da fase 1; cena de efeitos (God Hand liberado). 60 s cada, média, p95, pior quadro | A medir |
| 4 | Métricas gráficas | Draws/campo, vértices, shaders distintos, RAM (RSS), VRAM, versão GL/GLES exigida | A medir |
| 5 | Estimativa no alvo | Caminho A: razão de benchmark PC/R36S. Caminho B: medição direta de campos/s com o GS desligado | Depende da decisão |

Saída: tabela de tempo por componente e recomendação go, no-go ou go com renderizador próprio.

## Estimativa preliminar no RK3326

Sem medição. Serve só para decidir se vale gastar esforço na Fase 0.

| Componente | Evidência | Estimativa no RK3326 | Confiança |
| --- | --- | --- | --- |
| EE recompilado + IOP + VU1 (CPU) | Celular: 42–58 campos/s em multidão; núcleo A35 ~1/10–1/15 do Cortex-X2 | ~3–8 campos/s em luta | Baixa (±3x) |
| GS, backend CPU do runtime | 1 FPS no título já no celular | Inviável | Alta |
| GS, ponte PCSX2 sobre Vulkan | Precisa de Vulkan; falhou em Mali de celular | Provavelmente não abre na Mali-G31 | Média |
| GS, renderizador GLES próprio | Funciona no celular, mas lento em jogo | Possível tecnicamente; desempenho desconhecido | Baixa |
| RAM | 32 MB de RAM do PS2 + binário recompilado + caches; sem número publicado | Desconhecido; 1 GB é apertado | Baixa |
| Áudio | SPU2 no modo mais barato já existe no port Android | Custo pequeno frente ao resto | Média |

O número decisivo é o primeiro. Se a CPU não passar de ~20 campos/s com o GS desligado, nenhum renderizador salva
o projeto. É isso que o caminho B mede em um dia.

## Riscos principais

- **CPU:** o código recompilado é monothread por natureza (o EE é um só núcleo); os quatro núcleos do A35 não somam.
- **GPU e driver:** o GS precisa de GL desktop 4.x ou Vulkan; na Mali-G31 só há GLES 3.x garantido.
- **RAM:** 1 GB com o sistema operacional ocupando parte.
- **Base imatura:** o jogo só foi verificado até a fase 3, mesmo no PC.

## Testes da Fase 2, ajustados

Só fazem sentido se a medição sair go.

1. Build Linux x86_64 a partir do fork Android (não do repositório Windows). Aceite: chega ao menu.
2. Build ARM64 headless, GS desligado. Aceite: campos/s medidos no R36S, 5 cenas.
3. Executável mínimo SDL2 + GLES no R36S sem desktop: tela, controle, áudio, 256 MB alocados. Aceite: log e foto.
4. Benchmark do renderizador GLES próprio com a carga medida no passo 2 da Fase 0. Aceite: tempo de quadro.
5. RAM: RSS em 30 minutos, sem crescer. Aceite: gráfico de RSS.
