# Auditoria K0-C: PRNG, hash e observabilidade

## Escopo entregue

K0-C adiciona seed-mestra tail-compatible, PCG32 versionado, streams
independentes, sorteios inteiros bounded sem vies, hash canonico FNV-1a,
diagnosticos e trace por copia. K0-A, K0-B e K0-C estao concluidos; K0 ainda
esta em andamento e K0-D e o proximo sub-bloco.

| ID | Severidade | Area | Problema | Correcao | Teste | Estado |
| --- | --- | --- | --- | --- | --- | --- |
| K0C-001 | alta | seed | config antiga nao conhecia seed | cauda por `struct_size` e default estavel | `test-k0-c-random` | PASS |
| K0C-002 | alta | PRNG | ordem de chamadas podia acoplar subsistemas | streams por chave forte e PCG32 V1 | random e determinism | PASS |
| K0C-003 | alta | bounded | modulo simples introduz vies | rejection sampling | random | PASS |
| K0C-004 | alta | atomicidade | stream lazy podia publicar falha parcial | candidato temporario e commit unico | random e sanitizer | PASS |
| K0C-005 | alta | hash | memoria fisica podia entrar na comparacao | encoding explicito canonico | hash e determinism | PASS |
| K0C-006 | media | diagnostico | estado aleatorio nao era auditavel | streams, diagnosticos e trace | observability | PASS |
| K0C-007 | media | portabilidade | otimizacao podia divergir | comparacao byte a byte O0/O2 | optimization determinism | PASS |

## Decisoes e limites

O hash e diagnostico, nao assinatura criptografica. A seed e imutavel depois
de `create`; nao ha setter publico. A biblioteca normal nao exporta hooks de
teste. ASan/UBSan usa classificacao `PASS`, `FAIL` ou `UNAVAILABLE` somente se
o probe minimo da toolchain nao estiver disponivel.

K0-C nao implementa persistencia, replay, parser INI, trace CSV, stress longo,
sistemas, callbacks, Domain, Brain Bridge ou Worlds App. K0-D fica responsavel
por configuracao autoritativa, cenario integrado, trace CSV, manifesto,
relatorio, corrupcao, stress e fechamento completo de K0.
