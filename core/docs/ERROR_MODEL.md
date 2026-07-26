# Modelo de erros

miniSNN usa contratos locais em vez de um erro global. Funcoes booleanas
retornam `1` ou `0`; `minisnn_step` retorna contagem de spikes ou `-1` quando
o dispatch neuronal falha. Objetos C7 mantem `last_error` proprio e algumas
operacoes aceitam `out_error`.

Erros distinguem argumento nulo, dimensao, schema/assinatura, formato,
alocacao, estado de maquina e valor nao finito quando isso e relevante ao
modulo. `out_error` e inicializado deterministicamente pelos leitores de
schema. Falhas de frame, decoder e AgentIO sao atomicas: nao publicam valores,
ticks ou diagnosticos parciais.

Um erro neuronal aborta o timestep: nao e contado como spike, nao transmite
sinapse e deve ser propagado pelo runner. Checkpoint/load valida contrato,
modelo e assinaturas antes de substituir estado vivo. Rollback completo de uma
evolucao ja executada nao e prometido; a avaliacao invalida o individuo ou a
replica em vez de continuar silenciosamente.
