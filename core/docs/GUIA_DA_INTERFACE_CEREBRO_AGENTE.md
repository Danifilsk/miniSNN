# Guia da interface cerebro-agente

## Decodificacao de atividade C7.3

`MiniSNNActionDecoder` recebe um `MiniSNNNeuralActivityFrame` proprietario do
chamador. A matriz e `spikes[brain_step][neuron]`; todos os passos devem ser
capturados antes da decisao. `capture_step` le `minisnn_get_spike`, nao chama
`minisnn_step` e nao limpa o estado neural.

Os mappings usam somente `action_channel_id` e intervalos de neuronios. Os
modos sao `population_rate`, `bipolar_difference`, `threshold` e `wta_member`.
Population rate limita explicitamente entre `minimum_rate` e `maximum_rate`;
bipolar silencioso usa o default do canal; WTA calcula confianca como diferenca
entre vencedor e segundo colocado e desempata pelo menor ID. Esse desempate e
apenas deterministico, nao e preferencia semantica.

`minisnn_action_decoder_decode_to_agent_io` valida a janela, produz a action e
chama somente `minisnn_agent_io_submit_action_frame`. Ele nao finaliza o tick,
nao consome a action e nao inicia outro tick. Falhas preservam frame e
diagnosticos anteriores. Antes do decode, o decoder compara sua assinatura de
schema de acao com a copia privada do AgentIO; diferenca retorna
`SIGNATURE_MISMATCH` sem alterar o contexto. `action_decoder.txt` persiste schema signature,
dimensoes, mappings ativos e assinaturas FNV-1a versionadas.

O demo `mingw32-make scenario-action-decoding` usa o INI autoritativo em
`configs/action_decoding_demo.ini` e registra `config_source.ini` byte a byte,
`config_used.ini` canonico, atividade, trace, resumo e HTML. C7.3 nao
implementa recompensa, consequencia externa, reset de episodio, Studio ou
ciclo cerebro-agente completo; gera intencoes numericas abstratas cujo
significado pertence a uma camada de dominio posterior.

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

C7.2 permanece restrito a codificacao de sensores: nao implementa recompensa,
reset da rede neural, evolucao, Studio ou camada de dominio. Ele tambem nao
chama o passo neural e nao forca spikes.

## Ciclo generico C7.4

`MiniSNNAgentCycle` conecta somente contratos numericos existentes e nao possui
semantica de dominio. A rede, `MiniSNNAgentIOContext`, encoder e decoder sao
referencias nao proprietarias; o ciclo possui os frames temporarios e feedbacks.

Para um tick aceito, a ordem e: validar contratos e ausencia de action pendente;
entregar feedback devido; consumir e codificar sensor; aplicar entrada e avancar
a rede exatamente `brain_steps_per_tick` vezes; capturar cada passo; decodificar
a janela completa; publicar e finalizar a action atomicamente. A action nao e
consumida pelo ciclo. O proximo tick somente inicia depois do consumo externo.

Feedback e externo: `source_tick` aponta uma action ja produzida e
`delivery_tick` define quando o valor chega a C2. Com `episode_terminal = 0`,
ele e entregue antes do passo zero do tick de destino. Com
`episode_terminal = 1`, a action deve ter sido consumida e o feedback e aplicado
na fronteira do episodio por C2, sem passo neural ficticio e sem reset
automatico. O ciclo nunca calcula recompensa. Sem R-STDP, somente reward zero e
aceito. O reset explicito descarta feedback futuro do episodio, limpa estado
transitorio, traces e rewards pendentes, mas preserva pesos, topologia e a
cronologia estrutural; somente os rate traces estruturais transitorios sao
limpos. `episode_tick` no diagnostico descreve o tick executado. Nao existe
rollback da rede depois do primeiro passo: falha inesperada deixa o ciclo em
`FAULTED` ate o reset.

`scenario-agent-cycle` gera `config_source.ini`, `config_used.ini`, trace,
feedback, resumo e HTML locais. O demo e uma tarefa numerica fechada; nao
demonstra um dominio externo nem afirma aprendizado estatistico.

## Checkpoint e replay C7.5-A

O checkpoint integrado e uma extensao do ciclo C7.4. Ele persiste a fronteira
numerica, nao uma camada de dominio. O chamador continua sendo dono da rede,
AgentIO, encoder e decoder; o ciclo nao serializa seus ponteiros.

Use somente fronteiras estaveis:

- `READY`: a action anterior ja foi consumida e nao ha tick parcialmente aberto.
- `ACTION_PENDING`: o tick terminou, a action foi publicada uma vez e aguarda
  apenas o consumo externo.

O formato grava `manifest.txt`, estado da rede, AgentIO, fases do encoder,
contrato do decoder e estado do ciclo. O manifesto usa hashes FNV-1a
versionados para detectar arquivo ausente, truncamento ou alteracao. O load
recusa schemas, mappings, modelo, dimensoes, topologia e contratos
incompativeis antes de concluir a substituicao do estado vivo. A action
pendente restaurada continua consumivel exatamente uma vez, sem republicacao.

`scenario-agent-cycle-checkpoint` compara uma trajetoria continua com duas
retomadas, uma em `READY` e outra em `ACTION_PENDING`. Os arquivos
`continuous_trace.csv`, `ready_resume_trace.csv`,
`action_pending_resume_trace.csv` e `checkpoint_comparison.csv` deixam a
comparacao auditavel. `config_source.ini` preserva o INI byte a byte e
`config_used.ini` registra a forma canonica efetivamente executada.

O checkpoint preserva estado temporal e de aprendizado, inclusive correntes
agendadas, traces STDP, eligibility R-STDP, reward pendente, homeostase,
plasticidade estrutural, PRNG e feedback futuro. Ele nao cria semantica de
dominio e nao transforma o ciclo em uma camada de ambiente.

## Validacao

```powershell
mingw32-make test-agent-io
mingw32-make test-sensor-encoder
mingw32-make scenario-sensor-encoding
mingw32-make test-agent-cycle
mingw32-make scenario-agent-cycle
mingw32-make test-agent-cycle-checkpoint
mingw32-make scenario-agent-cycle-checkpoint
mingw32-make check-c7
```

Os testes cobrem schemas, ownership, assinaturas, serializacao, atomicidade,
ticks, isolamento, faixas neuronais, normalizacao, taxa deterministica e os
modelos LIF, AdEx e Hodgkin-Huxley.

## Auditoria integrada C7.5-B

Execute `mingw32-make test-c7` para a matriz completa e
`mingw32-make scenario-c7-integrated-audit` para gerar a evidencia local. O
cenario reutiliza a interface publica para LIF, AdEx e Hodgkin-Huxley; nao
atribui semantica de dominio aos nomes dos canais. Checkpoint/replay e reset
seguem os contratos C7.4/C7.5-A. A referencia de ownership e limites esta em
[Auditoria C7](AUDITORIA_C7_INTERFACE_CEREBRO_AGENTE.md).

O fechamento de C7 deixa D1 como auditoria e estabilizacao pre-Worlds. Seu
resultado planejado e `miniSNN Core v1.0-rc` com uma API candidata Core-Brain
Bridge estavel provisoriamente. A congelacao definitiva da API e reservada a
D2, apos a primeira integracao real.
