# Auditoria da fundacao K0-A

## Escopo entregue

K0-A cria o produto Worlds Kernel, sua biblioteca estatica, API publica,
lifecycle, configuracao versionada, clock logico, passo atomico, diagnostico,
demo e testes. K0-A esta concluido quando `audit-k0-a` passa. K0-B estende essa
fundacao com entidades, comandos e eventos; consulte
`K0_B_ENTITY_COMMAND_EVENT_AUDIT.md` para a auditoria complementar.

| ID | Severidade | Area | Problema | Correcao | Teste | Estado |
| --- | --- | --- | --- | --- | --- | --- |
| K0A-001 | alta | fronteira | Kernel nao podia herdar dependencias do Core | biblioteca e headers isolados | `audit-k0-a` | PASS |
| K0A-002 | alta | lifecycle | objeto precisava iniciar de forma deterministica | create/destroy e config versionada | `test_worlds_kernel` | PASS |
| K0A-003 | alta | tempo | tick nao pode depender de tempo real | fases atomicas e commit unico | `test_worlds_kernel` | PASS |
| K0A-004 | media | overflow | contador nao pode voltar para zero | deteccao antes do commit | `test_worlds_kernel` | PASS |
| K0A-005 | media | entrega | API precisava ser verificavel fora dos fontes | headers isolados, demo e simbolos auditados | `audit-k0-a` | PASS |
| K0A-006 | alta | configuracao | copia total podia ler alem de prefixo truncado | leitura individual de prefixo e cauda ignorada | `test_worlds_kernel`, `test-k0-a-sanitize` | PASS |

## Atomicidade e determinismo

Os testes comparam 1000 passos diretos com 400 mais 600 passos. Os diagnosticos
comprometidos sao identicos: tick 1000, estado `READY` e erro `NONE`.

## Portabilidade

O build usa C11 com avisos estritos. O checker executa um smoke com o compilador
disponivel; se nenhum compilador C11 existir, o resultado e `UNAVAILABLE`, nao
um PASS sintetico.

A configuracao versionada le somente `struct_size` antes de confirmar que o
prefixo contem `format_version`. Estruturas menores sao rejeitadas sem ler
campos ausentes, e uma cauda maior com formato V1 e ignorada. A regressao
ASan/UBSan e `PASS`, `FAIL` ou `UNAVAILABLE`; indisponibilidade ambiental e
aceita somente quando o probe minimo demonstra ausencia do sanitizer.

## Limites preservados

K0-A continua sendo a fundacao de lifecycle e tempo. K0-B nao adiciona
sistemas, aleatoriedade, espaco, persistencia, Domain, Brain Bridge ou App.
Esses contratos permanecem para os sub-blocos seguintes.
