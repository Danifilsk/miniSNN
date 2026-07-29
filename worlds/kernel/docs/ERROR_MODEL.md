# Modelo de erros

As funcoes mutaveis retornam `MiniSNNWorldsKernelError`. `create` retorna
ponteiro nulo em falha e preenche `out_error` quando ele existe. Nao ha erro
global.

Erros de configuracao e alocacao nao criam objeto parcial. Erro de overflow
preserva o tick anterior e continua observavel por `last_error`. Um passo
normal limpa o erro anterior ao concluir o commit. Argumento nulo nao produz
um estado `STEPPING` ou `FAULTED` em outra instancia.

Uma configuracao cujo `struct_size` nao alcanca `format_version` falha com
`INVALID_CONFIG` sem acessar o campo ausente e sem tentar alocar. Para uma
estrutura maior com versao V1, K0-A usa somente o prefixo conhecido e ignora a
cauda. Ponteiro totalmente invalido continua sendo erro do chamador.

`get_diagnostics` copia um snapshot apenas em sucesso. Em erro, o buffer de
saida permanece inalterado.
