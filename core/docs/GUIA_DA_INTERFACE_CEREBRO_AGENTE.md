# Guia da interface cerebro-agente

## Objetivo

C7.1 define uma fronteira numerica e deterministica entre um produtor de
sensores e um consumidor de acoes. C7.2 conecta o frame de sensores a entradas
neuronais por um encoder publico, sem interpretar os canais como objetos de um
dominio externo. C7.3 permanece reservado para a decodificacao de atividade
neural em acoes.

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
minisnn_sensor_encoder_encode_frame(encoder, &sensors_for_encoder, &inputs);
minisnn_neural_input_frame_apply_step(&inputs, brain_step, network, &encoder_error);
/* O chamador, e somente ele, chama minisnn_step(network). */
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
nao assume o que eles significam. C7.2 referencia canais por id, nunca pelo
nome, e aplica somente normalizacao e mapeamentos numericos declarados.

## Codificacao neural C7.2

Inclua `minisnn_sensor_encoder.h` (ou `minisnn.h`) para criar um
`MiniSNNSensorEncoder`. Cada `MiniSNNSensorEncodingSpec` declara o id do canal,
um intervalo contiguo de neuronios e um modo. Intervalos sobrepostos, ids
inexistentes, parametros nao finitos e taxa fora de `(0, 1]` sao rejeitados.
Neuronios sem mapeamento recebem corrente zero.

- `linear_current`: `bias + gain * normalizado`.
- `bipolar_current`: `bias + gain * (2 * normalizado - 1)`.
- `deterministic_rate`: acumulador de fase por mapeamento; `maximum_rate` e
  expresso em pulsos por passo neural e cada pulso escreve `pulse_current`.

Para `minimum == maximum`, o valor normalizado e zero. `phase_offset` representa
milesimos de fase e deve estar em `0..999`; valores fora do intervalo sao
rejeitados, sem aliases por modulo. A taxa e deterministica
para o mesmo frame, especificacao e estado de fase; ela nao usa PRNG nem
forca spikes. `minisnn_sensor_encoder_reset` restaura as fases iniciais.

`MiniSNNNeuralInputFrame` pertence ao chamador e armazena uma matriz
`brain_step x neuron_count`. O encoder calcula em buffers internos e so copia
o resultado e as fases quando o frame inteiro e valido. Em seguida,
`minisnn_neural_input_frame_apply_step` recebe tambem `out_error`, valida todo o
frame, o passo e as dimensoes antes de limpar entradas e entao aplica as
correntes pela API publica. Ela nao avanca a simulacao. Isso preserva explicitamente a ordem:
codificar, aplicar, chamar `minisnn_step` no chamador.

Os mapeamentos e o contrato possuem assinaturas FNV-1a 64-bit. O formato textual
versionado persiste somente ids, dimensoes e parametros numericos; leitura com
schema ou assinatura incompativel falha. Execute
`mingw32-make scenario-sensor-encoding` para gerar `config_source.ini` (copia
byte a byte do INI fornecido), `config_used.ini` (forma canonica efetivamente
executada), `sensor_encoding_trace.csv`, `sensor_encoding_summary.txt` e
`sensor_encoding_report.html`. O parser vive em `app/`, resolve nomes de canais
para IDs somente nessa fronteira e rejeita secoes, chaves e valores invalidos.

## Limitacoes

C7.2 nao implementa decodificacao, recompensa, reset da rede neural, evolucao,
Studio ou qualquer camada de dominio. Ele tambem nao chama o passo neural e nao
forca spikes. C7.3 cuidara somente da decodificacao de acoes.

## Validacao

```powershell
mingw32-make test-agent-io
mingw32-make test-sensor-encoder
mingw32-make scenario-sensor-encoding
mingw32-make check-c7
```

Os testes cobrem schemas, ownership, assinaturas, serializacao, atomicidade,
ticks, isolamento, faixas neuronais, normalizacao, taxa deterministica e os
modelos LIF, AdEx e Hodgkin-Huxley.
