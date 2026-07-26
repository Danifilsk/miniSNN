# Limites candidatos

Os limites validos pertencem aos modulos e parsers; D1-A os inventaria sem
fixar uma ABI final.

| Area | Limite ou regra |
|---|---|
| Rede | `neuron_count` do scenario e limitado a 1000; indices e delays sao validados antes do acesso. |
| Sinapses | Conexoes rejeitam endpoints invalidos, duplicatas quando proibidas e delay fora do maximo configurado. |
| Cenarios | Buffers de nome, topologia e caminhos possuem constantes declaradas nos headers `app/`. |
| C7 schemas | IDs, ranges, contagens, mappings, feedbacks e passos por tick usam limites publicos ou de parser; nomes usam ASCII contratual. |
| Persistencia | Arquivos e paths usam buffers limitados e rejeitam truncamento, assinatura invalida e contagem inconsistente. |
| Alocacao | Contagem multiplicada por `sizeof` e validada pelos construtores antes de alocar; entradas zero e nao finitas sao rejeitadas conforme o contrato. |

Os testes de fronteira evitam alocacoes absurdas. Limites quantitativos de
desempenho, stress e sanitizers completos ficam para D1-B.
