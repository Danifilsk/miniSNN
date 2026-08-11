# K1-B2 Atomic Movement And Displacement Contract

K1-B2 adiciona deslocamento espacial deterministico ao miniSNN Worlds Kernel.
Ele permanece uma biblioteca C11 headless e nao conhece fisica, velocidade,
colisao resolvida, rota, sprites, agentes ou dominio.

## API e unidades

`minisnn_worlds_kernel_queue_move_entity` agenda um unico comando
`MOVE_ENTITY` para um tick futuro. `delta_x` e `delta_y` usam a mesma unidade
fixed-point documentada em `SPACE_COORDINATE_CONTRACT.md`; sao deslocamentos,
nao velocidade nem aceleracao. A orientacao do transform e preservada.

A submissao valida os argumentos gerais de comandos, incluindo Kernel pronto,
ID de alvo nao zero, emissor valido e tick estritamente futuro. Ela nao aplica
o movimento nem reserva uma posicao: a semantica e decidida no plano atomico
do tick.

## Resolucao atomica

No tick, comandos sao ordenados por `target_tick`, `priority`, ID do emissor e
Command ID. Cada `MOVE_ENTITY` observa o estado planejado produzido pelos
comandos anteriores nessa mesma ordem. Portanto placement, occupancy,
clear-occupancy, remove e destroy no mesmo tick podem alterar de forma
reprodutivel o resultado de um movimento posterior.

O destino e calculado com soma inteira verificada. Para um alvo colocado, o
Kernel verifica, nesta ordem:

1. overflow de `position + delta`;
2. limites do ponto de destino;
3. limites do envelope AABB no destino, quando ha occupancy;
4. conflito bloqueante com AABBs vivos colocados, ignorando a propria entidade.

Uma colisao mantem transform e occupancy originais. Quando mais de um blocker
se sobrepoe com area positiva, `related_entity` e o menor Entity ID elegivel.
Contato apenas pela borda continua permitido. Category bits e blocking masks
usam exatamente a regra K1-B1; AABBs sem bloqueio podem se sobrepor.

`delta_x == 0 && delta_y == 0` e um no-op aceito: produz `ENTITY_MOVED` com
origem e destino iguais, preserva o estado e conta como comando aplicado, mas
nao incrementa `total_entities_moved`.

## Eventos, rejeicoes e observabilidade

Um movimento aplicado emite `ENTITY_MOVED`. O evento traz
`has_previous_transform`, `previous_transform`, `has_transform` e `transform`,
permitindo derivar o delta sem expor ponteiro interno. Rejeicoes semanticas
emitem `COMMAND_REJECTED` e preservam o estado oficial:

- `TARGET_NOT_ALIVE`;
- `TARGET_NOT_PLACED`;
- `POSITION_OUT_OF_BOUNDS`;
- `OCCUPANCY_OUT_OF_BOUNDS`;
- `OCCUPANCY_CONFLICT` com `related_entity` canonico;
- `DESTINATION_OVERFLOW`.

`MiniSNNWorldsKernelDiagnostics` inclui comandos de movimento processados,
entidades efetivamente movidas e rejeicoes de overflow. Os contadores fazem
parte do estado causal observado pelo hash.

## Hash e compatibilidade

K1-B2 introduz `STATE_HASH_VERSION_V4`. V4 acrescenta ao encoding explicito os
deltas pendentes, origem de eventos de movimento e os tres contadores de
movimento. Nenhuma struct, padding, ponteiro ou endereco e hasheado.

As versoes historicas continuam estritas: um estado K0 compativel permanece
V1; um estado K1-A sem occupancy/movimento permanece V2; um estado K1-B1 sem
qualquer historico de movimento permanece V3. Depois de um movimento pendente,
aplicado ou rejeitado, V3 e recusado em vez de interpretar o estado como
historico. A API de hash atual seleciona dinamicamente a versao historica
compativel; estados com qualquer historico de movimento usam V4, enquanto a
macro publica da versao canonica atual permanece definida como V4.

## Limites assumidos

K1-B2 nao implementa swept collision, tunelamento, velocidade, aceleracao,
orientacao de AABB, solucao de penetracao, pathfinding, grafo de links,
multiplas formas, camadas fisicas ou resposta de colisao. Ele apenas agenda e
aplica deslocamentos discretos inteiros, com validacao de destino atomica.

## K1-C2 spatial subtrees

A root with spatial children translates its full canonical subtree atomically for nonzero deltas. Direct movement of a child remains rejected. Joint preflight ignores only internal subtree occupancy conflicts and chooses external conflicts canonically; accepted descendants emit causal movement events after the root.
