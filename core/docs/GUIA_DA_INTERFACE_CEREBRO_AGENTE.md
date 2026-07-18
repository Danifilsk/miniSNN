# Guia da interface cerebro-agente

## Objetivo

C7.1 define uma fronteira numerica e deterministica entre um produtor de
sensores e um consumidor de acoes. Ela ainda nao converte valores em corrente,
spikes ou qualquer formato neuronal. Tambem nao interpreta os canais como
objetos de um dominio externo.

O fluxo valido e:

```text
schema de sensores + schema de acoes
  -> frame de sensores no tick N
  -> consumo do sensor no tick N
  -> frame de acoes no tick N
  -> finalizacao do tick N
  -> consumo da acao no tick N
```

## Schemas

Inclua `minisnn_agent_io.h` e crie `MiniSNNSensorSchema` e
`MiniSNNActionSchema` a partir de arrays de specs. Cada canal possui `id`,
`name`, `minimum`, `maximum` e `default_value`.

- Um schema ativo possui de 1 a 256 canais.
- IDs e nomes sao unicos.
- Nomes nao sao vazios e possuem no maximo 96 bytes ASCII imprimiveis
  (`0x20..0x7E`). Bytes de controle, `DEL` e bytes acima de `0x7E` sao
  rejeitados.
- Todos os limites e defaults sao finitos e obedecem
  `minimum <= default_value <= maximum`.
- A ordem declarada pelo chamador e a ordem canonica dos valores do frame.
- Valores fora do intervalo sao rejeitados; C7.1 nao faz clamp implicito.

O schema copia os nomes e os valores. Portanto, o array e as strings usados na
criacao podem ser liberados em seguida.

## Frames e ownership

Inicialize frames com zero, por exemplo `MiniSNNSensorFrame frame = {0};`, e
depois use `minisnn_sensor_frame_init`. O frame passa a possuir seu vetor
`values`; use `minisnn_sensor_frame_destroy` para libera-lo. As funcoes de set
rejeitam NaN e infinito sem alterar os valores ja presentes.

`MiniSNNAgentIOContext` copia ambos os schemas na criacao e copia os dados de
todo frame submetido. O contexto nao retem ponteiros do chamador. Frames
recuperados por `minisnn_agent_io_copy_last_sensor_frame` e
`minisnn_agent_io_copy_last_action_frame` sao copiados para buffers ja
inicializados pelo chamador.

## Ciclo por tick

Para cada tick logico, envie exatamente um sensor frame, consuma-o uma vez,
envie uma action frame com o mesmo tick, finalize e consuma a action uma vez:

```c
minisnn_agent_io_submit_sensor_frame(context, &sensors);
minisnn_agent_io_consume_sensor_frame(context, &sensors_for_encoder);
/* C7.2 conectara o encoder neural neste ponto. */
minisnn_agent_io_submit_action_frame(context, &actions);
minisnn_agent_io_finish_tick(context);
minisnn_agent_io_consume_action_frame(context, &actions_for_consumer);
```

Cada consume copia para um buffer inicializado pelo chamador e nao expoe
ponteiros internos. A action nao pode ser submetida antes de o sensor ser
consumido. Um tick posterior e bloqueado enquanto a action finalizada anterior
nao for consumida. Ticks precisam crescer estritamente depois de finalizados.
O contexto rejeita sensor duplicado, consumo duplicado, action antes do sensor,
action antes do consumo do sensor, action duplicada, tick de action diferente,
tick repetido e tick regressivo. Erros sao locais ao contexto e podem ser
consultados com `minisnn_agent_io_last_error` e
`minisnn_agent_io_error_string`. Um erro nao modifica buffers, flags pendentes
nem o ultimo tick finalizado.

`minisnn_agent_io_reset` elimina todos os ticks, consumos e frames finalizados
do contexto e restaura os buffers internos aos defaults dos schemas.

## Assinaturas e texto

As assinaturas de sensor schema, action schema e contrato usam FNV-1a 64-bit
padrao e versionado. O hash serializa explicitamente versao, quantidade, id, nome,
minimo, maximo e default. Ele nao inclui padding, ponteiros ou enderecos.

`minisnn_sensor_schema_write_file` e sua variante de action escrevem arquivos
versionados estaveis. Valores double sao armazenados por seus bits hexadecimais
para evitar dependencia de locale. Nomes preservam ASCII imprimivel e
percent-encodam todos os bytes fora de `A-Z`, `a-z`, `0-9`, `_`, `-` e `.`.
Os readers rejeitam versao, estrutura ou valores incompativeis de forma
deterministica.

## Exemplo numerico

Um schema de sensores pode descrever dois canais normalizados, com defaults
`0.0` e limites `[0.0, 1.0]`. Um schema de acoes pode descrever dois valores em
`[-1.0, 1.0]`. Esses nomes e limites sao apenas contrato de transporte: C7.1
nao assume o que eles significam e nao os aplica a uma rede neural.

## Limitacoes

C7.1 nao implementa codificacao sensorial, entradas de corrente, spike
encoding, decodificacao, recompensa, reset da rede neural, evolucao, Studio ou
qualquer camada de dominio. C7.2 cuidara somente da codificacao de sensores;
C7.3 cuidara somente da decodificacao de acoes.

## Validacao

```powershell
mingw32-make test-agent-io
mingw32-make check-c7
```

O teste cobre schemas, ownership, assinaturas, serializacao, atomicidade,
ticks, isolamento de duas instancias e destruicao repetida.
