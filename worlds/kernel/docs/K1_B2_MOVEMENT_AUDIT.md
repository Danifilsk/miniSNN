# K1-B2 Movement Audit

## Escopo fechado

K1-B2 fecha o deslocamento discreto do Worlds Kernel sem ampliar o dominio:
comandos futuros `MOVE_ENTITY`, soma fixed-point verificada, limites
espaciais, occupancy K1-B1, eventos com origem/destino, diagnosticos e hash
canonico V4.

| Contrato | Evidencia | Resultado |
| --- | --- | --- |
| API publica sem ponteiros internos | `queue_move_entity`, `CommandInfo.displacement` | PASSOU |
| Ordem dentro do tick | plano temporario e ordenacao canonica K0-B | PASSOU |
| Sucesso e no-op | evento `ENTITY_MOVED`, orientacao preservada | PASSOU |
| Colisao e masks K1-B1 | AABB no destino, menor `related_entity` | PASSOU |
| Overflow e limites | soma/envelope verificados, rejeicao sem mutacao | PASSOU |
| Atomicidade global | falha interna de alocacao preserva tick/fila/hash | PASSOU |
| Hash versionado | V1/V2/V3 estritos, movimento em V4 | PASSOU |
| Escala | 20.000 entidades, 15.000 occupancies, 1.000 conflitos reais | PASSOU |
| Reprodutibilidade | repeticao e -O0/-O2 equivalentes | PASSOU |

## Demo reproduzivel

```powershell
mingw32-make demo-k1-b2
```

O demo gera `build/worlds/kernel/demo/k1_b2/trace.csv`, `events.csv`,
`positions.csv`, `manifest.ini` e `report.txt`. Ele cobre movimento livre,
sobreposicao nao bloqueante, conflito com menor ID, overflow, no-op,
clear-occupancy seguido de movimento e movimento apos remocao. O vetor de
referencia V4 e `0xA0AF3078F4B0E869` para a configuracao publicada.

## Gates

```powershell
mingw32-make test-k1-b2-movement
mingw32-make test-k1-b2-optimization-determinism
mingw32-make test-k1-b2-stress
mingw32-make test-k1-b2-sanitize
mingw32-make test-k1-b2-posix-smoke
mingw32-make audit-k1-b2
```

O sanitizer primeiro executa uma probe independente. Se a toolchain for
indisponivel, o gate informa indisponibilidade; depois de uma probe valida,
qualquer falha do projeto e falha real. O smoke POSIX e explicitamente
indisponivel em Windows.

## Fora de escopo

K1-B2 nao adiciona velocidade, fisica continua, links, grafo espacial,
pathfinding, agentes, Brain Bridge, Domain, Studio ou App. K1-B2 retains this original movement scope. K1-C1..C4 were completed later without invalidating it; K2 is the next Worlds Kernel block.
