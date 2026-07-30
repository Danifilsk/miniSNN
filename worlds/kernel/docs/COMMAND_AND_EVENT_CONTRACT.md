# Contrato de comandos e eventos

K0-B usa comandos agendados para toda mutacao de entidade. As APIs publicas
aceitam `CREATE_ENTITY` e `DESTROY_ENTITY` e retornam um
`MiniSNNWorldsKernelCommandId` somente apos a submissao ser aceita. Uma falha
de validacao ou alocacao nao consome Command ID e nao altera a fila.

Um comando deve apontar para um tick futuro. O emissor zero representa uma
origem externa. Um emissor diferente de zero precisa estar vivo no instante em
que o comando for aplicado. A destruicao exige um Entity ID nao zero; o alvo
pode deixar de estar vivo ate o tick, caso em que o comando e semanticamente
rejeitado.

Antes de cada tick, a fila elegivel e ordenada canonicamente por:

1. `target_tick` crescente;
2. `priority` crescente;
3. Entity ID do emissor crescente;
4. Command ID crescente.

Essa ordem nao depende da ordem de insercao, enderecos ou detalhes de
alocacao. `pending_command_at` expoe a mesma ordem sem permitir mutacao.

Para cada comando consumido, o Kernel emite exatamente um evento no mesmo
tick. Os tipos atuais sao `ENTITY_CREATED`, `ENTITY_DESTROYED` e
`COMMAND_REJECTED`. Eventos possuem `EventId` monotonicamente crescente e nao
reutilizado. A janela publica contem somente os eventos do ultimo tick
concluido; um tick bem-sucedido sem comandos limpa a janela.

Conflitos semanticos, como dois destroys para o mesmo alvo, sao sucesso do
motor: o primeiro comando aplicavel produz destruicao e o seguinte produz
`COMMAND_REJECTED` com motivo explicito. Falhas internas, como alocacao ou
overflow de identificador, nao consomem comandos, nao trocam a janela de
eventos, nao avancam o tick e nao mudam os contadores comprometidos.

O plano completo do tick e preparado em memoria temporaria: comandos ordenados,
eventos, registro de entidades e contadores futuros. Somente depois de toda a
prevalidacao o plano e promovido ao estado observavel. Portanto, uma rejeicao
semantica recebe evento; uma falha interna preserva o estado anterior.
