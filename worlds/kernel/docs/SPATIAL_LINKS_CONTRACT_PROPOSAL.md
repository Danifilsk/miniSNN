# K1-C Spatial Links Contract

**Status:** K1-C1..C4 implementados e auditados. K1-C4
permanecem futuros e nao sao autorizados por este documento.

## 1. Estado atual auditado

O Worlds Kernel e C11, headless, generico e deterministico. A biblioteca nao
abre arquivos, nao renderiza e nao conhece Domain. O diretorio app/ contem
demos/parsers; include/ contem a superficie publica.

Em src/minisnn_worlds_kernel.c, prepare_step_plan seleciona os comandos do
tick, aplica a ordem canonica target_tick, priority, issuer.value e
command_id.value sobre copias planejadas, e minisnn_worlds_kernel_step promove
o plano apenas depois de sucesso interno. Rejeicao semantica consome o comando e
gera evento; falha de alocacao, overflow de ID ou estado interno aborta o tick
sem promocao.

EntityRecord contem ID, ticks de criacao/destruicao, vida, transform e
occupancy. O ID cresce monotonamente e nunca e reutilizado; nao existe campo de
geracao. Logo, um ID destruido nao volta a referir outra entidade antes do
esgotamento do contador.

O hash possui V1-V5. A selecao dinamica preserva hashes historicos para estados
compativeis; V5 cobre links, endpoints pendentes, eventos, affected_entity e
contadores estruturais. O encoding e FNV explicito, canonico e sem ponteiros,
padding ou capacidade.

## 2. Decisoes congeladas

- O link e dirigido parent -> child; cada child possui no maximo um parent e
  cada parent pode possuir varios children.
- Self-link, duplicata, ciclo e multiplos parents sao proibidos. O conjunto de
  links ativos forma uma floresta dirigida.
- Parent e child devem estar vivos e colocados no espaco.
- Transforms permanecem globais. Offset X/Y e derivado na criacao, nao e
  fornecido pelo chamador, nao e rotacionado e nao muda orientacoes.
- Criar ou remover link nao reposiciona entidade. Remover preserva o transform
  mundial atual do child.
- A identidade do link e exclusivamente (parent, child). Nao ha LinkId, tipo
  estrutural adicional, Domain ou semantica de produto.
- Somente uma raiz podera iniciar movimento direto. Em C2, a translacao de uma
  raiz move rigidamente toda a subarvore; membros internos sao ignorados entre
  si apenas nessa preflight de translacao. Regras K1-B1 continuam para todos os
  conflitos externos.
- Links nunca alteram categories, blocking masks ou occupancy.

## 3. Objetivo e fronteira de K1-C

K1-C representa uma relacao espacial estrutural, observavel e deterministica.
Ela permite manter offsets e, em C2, deslocar uma subarvore de forma atomica.
Nao e scene graph visual, inventario, fisica, joint ou feature de Domain.

K1-C1 introduz o estado publico minimo e o hash V5. K1-C2 introduz a
propagacao rigida. A separacao nao pode permitir estado invalido entre os
subblocos: C1 ja bloqueia lifecycle e movimentos que violariam offsets.

## 4. Modelo publico de dados

K1-C1 deve expor uma representacao explicita, proposta com estes nomes:

~~~c
typedef struct
{
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
    MiniSNNWorldsKernelScalar offset_x;
    MiniSNNWorldsKernelScalar offset_y;
} MiniSNNWorldsKernelSpatialLink;

typedef struct
{
    MiniSNNWorldsKernelEntityId parent;
    MiniSNNWorldsKernelEntityId child;
} MiniSNNWorldsKernelSpatialLinkEndpoints;
~~~

CommandInfo recebe um campo explicito spatial_link_endpoints, acompanhado por
has_spatial_link_endpoints. Somente os tipos CREATE_SPATIAL_LINK e
REMOVE_SPATIAL_LINK o usam. Para ambos, target_entity = parent e endpoints.parent
repete deliberadamente o parent; endpoints.child representa o outro endpoint.
Transform, occupancy e displacement nao carregam endpoints de link.

Um comando pendente de criacao ou remocao contem somente parent e child. O
offset da criacao ainda e desconhecido antes da aplicacao do tick; portanto,
nenhum zero e usado para fingir que ele existe.

Em C1, Event recebe os campos has_spatial_link e spatial_link. Evento aceito de
link tem has_spatial_link verdadeiro e um SpatialLink completo. Rejeicao de
link tem has_spatial_link falso e o conteudo de spatial_link e invalido, nunca
um substituto para offset desconhecido. C1 ja adiciona explicitamente o campo affected_entity e o inclui no V5. Em C1
ele vale sempre zero; C2 podera preencher o membro movel que encontrou um
blocker sem alterar o layout ou o encoding V5.

Nao ha ponteiro mutavel para LinkRecord interno. Consultas copiam um
MiniSNNWorldsKernelSpatialLink completo.

## 5. Invariantes de armazenamento

1. Todo link ativo possui endpoints vivos, distintos e colocados.
2. Cada child ocorre em no maximo um link ativo; a chave (parent, child) e
   unica.
3. A busca de ancestrais de parent nunca alcanca child. A deteccao de ciclo e
   iterativa, limitada por entity_count e independente da ordem fisica do
   vetor.
4. Os links ativos sao armazenados e consultados em ordem canonica
   (parent.value, child.value).
5. Criar/remover link preserva transform, occupancy e orientacao de ambas as
   entidades.
6. Falha interna preserva links, entidades, comandos pendentes, eventos e
   contadores oficiais. Rejeicao semantica preserva estado estrutural, consome
   somente o comando e gera a observacao definida.

## 6. Comandos e guarda de movimento em C1

K1-C1 deve expor comandos equivalentes a:

~~~c
minisnn_worlds_kernel_queue_create_spatial_link(
    kernel, target_tick, priority, issuer, parent, child, out_command_id);
minisnn_worlds_kernel_queue_remove_spatial_link(
    kernel, target_tick, priority, issuer, parent, child, out_command_id);
~~~

CREATE_SPATIAL_LINK deriva checked:

~~~text
offset_x = child.x - parent.x
offset_y = child.y - parent.y
~~~

e falha com SPATIAL_LINK_OFFSET_OVERFLOW sem alterar o plano. REMOVE exige a
chave exata e preserva o transform global.

C1 ainda nao implementa propagacao. Para nao quebrar offsets antes de C2:

- MOVE_ENTITY em child ligado rejeita TARGET_HAS_SPATIAL_PARENT.
- MOVE_ENTITY em parent com algum child ligado rejeita
  TARGET_HAS_SPATIAL_CHILDREN.
- MOVE_ENTITY em raiz sem children permanece exatamente K1-B2.

C2 substitui somente a segunda guarda por translacao rigida da subarvore. Esta
restricao temporaria de C1 evita movimento parcial de uma estrutura ja ativa.

## 7. Eventos de K1-C1

C1 preserva exatamente um evento por comando consumido. A capacidade atual
event_capacity = command_count, last_tick_event_count e total_events_emitted
permanece valida nesta etapa.

Eventos aceitos novos:

~~~text
SPATIAL_LINK_CREATED
SPATIAL_LINK_REMOVED
~~~

Para ambos, subject = parent, related_entity = child, rejection = NONE,
has_spatial_link = true e spatial_link contem parent, child, offset_x e
offset_y completos.

Para uma rejeicao de CREATE_SPATIAL_LINK ou REMOVE_SPATIAL_LINK:

- subject e sempre o parent solicitado;
- related_entity e sempre o child solicitado, inclusive no self-link;
- rejection contem o motivo estrutural especifico;
- has_spatial_link e falso; nao existe payload de link parcial;
- affected_entity existe desde C1, vale zero em todos os eventos C1 e ja entra no V5.

Assim, parent morto, child morto, endpoint nao colocado, self-link, duplicata,
child com parent, ciclo, overflow de offset e link inexistente sao observaveis
sem sobrecarga de campos existentes. Falha interna, capacidade insuficiente ou
overflow de ID nao gera COMMAND_REJECTED: aborta o tick sem promocao.

## 8. Rejeicoes e contadores de K1-C1

Os nomes finais devem seguir o prefixo publico existente. A semantica ja fica
congelada:

~~~text
SPATIAL_LINK_PARENT_NOT_ALIVE
SPATIAL_LINK_CHILD_NOT_ALIVE
SPATIAL_LINK_SELF
SPATIAL_LINK_PARENT_NOT_PLACED
SPATIAL_LINK_CHILD_NOT_PLACED
SPATIAL_LINK_DUPLICATE
SPATIAL_LINK_CHILD_HAS_PARENT
SPATIAL_LINK_CYCLE
SPATIAL_LINK_OFFSET_OVERFLOW
SPATIAL_LINK_NOT_FOUND
TARGET_HAS_SPATIAL_PARENT
TARGET_HAS_SPATIAL_CHILDREN
TARGET_HAS_SPATIAL_LINKS
~~~

Os contadores publicos minimos sao:

- active_spatial_links: numero atual de links ativos;
- total_spatial_links_created: criacoes aceitas desde a inicializacao;
- total_spatial_links_removed: remocoes aceitas desde a inicializacao;
- total_spatial_link_commands_processed: CREATE/REMOVE consumidos, aceitos ou
  rejeitados.

Comandos rejeitados continuam incrementando apenas os contadores gerais ja
existentes de rejeicao, alem de total_spatial_link_commands_processed quando o
tipo e structural link. Todos os quatro contadores entram no V5.

## 9. Lifecycle explicito desde C1

Nao existe auto-unlink em K1-C1, C2, C3 ou C4 deste contrato. Nao existem
eventos implicitos, causa de auto-remocao, descendants soltos
automaticamente, reparenting ou multiplos eventos causados por DESTROY_ENTITY.

DESTROY_ENTITY em entidade com qualquer link incidente rejeita
TARGET_HAS_SPATIAL_LINKS. REMOVE_ENTITY_FROM_SPACE em entidade com qualquer
link incidente rejeita o mesmo motivo. O chamador deve remover explicitamente
todos os links incidentes antes de destruir ou retirar a entidade do espaco.

Isso mantem um evento por comando em C1, nao deixa endpoint morto/sem transform
e preserva atomicidade simples. Exemplos no mesmo tick obedecem a ordem
canonica:

- REMOVE_SPATIAL_LINK -> DESTROY_ENTITY pode aceitar destroy quando o unlink
  anterior removeu o ultimo incidente.
- DESTROY_ENTITY -> REMOVE_SPATIAL_LINK rejeita destroy; o unlink posterior
  observa o estado planejado resultante normal e pode ser aceito.

## 10. Ordering canonico

A ordem existente continua autoritativa. A matriz fechada e:

| Sequencia | Resultado |
| --- | --- |
| create link -> move parent | C1 rejeita o move por child ativo; C2 move a nova subarvore. |
| move parent -> create link | C1 move parent sozinho se ainda nao havia child; create deriva offset das posicoes planejadas finais. Em C2 vale o mesmo principio. |
| remove link -> move child | child vira raiz e pode mover por K1-B2/C2. |
| move parent -> remove link | C1 rejeita o move se havia child; C2 move a subarvore e unlink preserva posicoes finais. |
| destroy -> create link | create rejeita endpoint morto se destroy foi aceito. |
| create link -> destroy endpoint | destroy rejeita TARGET_HAS_SPATIAL_LINKS. |
| remove link -> destroy endpoint | destroy pode aceitar apos o ultimo unlink. |
| move parent -> move child | em C2, segundo move rejeita TARGET_HAS_SPATIAL_PARENT. |
| dois links para o mesmo child | primeiro valido aceita; segundo rejeita CHILD_HAS_PARENT. |
| remove-from-space com link | rejeitado TARGET_HAS_SPATIAL_LINKS. |
| unlink -> remove-from-space | pode aceitar conforme ordem canonica. |

PLACE antes de create pode satisfazer placement. CREATE antes de PLACE rejeita
o endpoint nao colocado. Nao ha resolucao simultanea especial.

## 11. Propagacao rigida em K1-C2

C2 substitui a guarda de parent com children por este fluxo atomico:

1. coletar a subarvore no estado planejado de forma iterativa;
2. ordenar a travessia raiz primeiro, parent-before-child e children de cada
   parent por EntityId crescente;
3. calcular todos os destinos com o mesmo delta checked;
4. validar ponto, envelope AABB e limites de cada membro;
5. preflightar colisao externa de todos os destinos;
6. reservar todos os eventos, IDs e buffers antes da primeira mutacao do
   plano;
7. promover todos os transforms ou nenhum;
8. emitir eventos na ordem da travessia, dentro do grupo do comando.

Todos os eventos do grupo usam o command_id do MOVE_ENTITY raiz. Para a raiz,
subject = raiz e related_entity = 0. Para descendant, subject = descendant e
related_entity = seu parent imediato.

Delta zero preserva K1-B2: o comando e aceito, somente o ENTITY_MOVED primario
da raiz e emitido, nenhum descendant recebe evento causal,
total_entities_moved nao aumenta e links/offsets nao mudam. O evento e os
contadores causais ainda podem selecionar V5.

## 12. Occupancy, conflito e eventos causais em C2

Membros da subarvore em movimento sao ignorados somente entre si durante a
preflight da translacao rigida. Occupants externos continuam usando K1-B1.
Entre todos os conflitos possiveis, o resultado canonico ordena por:

1. blocker externo EntityId crescente;
2. moving member EntityId crescente como desempate.

No COMMAND_REJECTED de movimento estrutural, subject e a raiz comandada,
related_entity e o menor blocker externo e affected_entity e o membro movel
que colidiu. affected_entity nao reutiliza issuer, transform, occupancy ou
outro campo de semantica diferente. Esse campo e zero quando nao se aplica e
entra no V5.

C2 tambem evolui explicitamente o sistema de eventos para:

~~~text
um evento primario por comando
+ zero ou mais eventos causais deterministas
~~~

O plano calcula o total real de eventos, com arithmetic checked, a partir dos
comandos e cardinalidades de subarvores. Reserva capacidade e faixas de
EventId antes de mutar o plano. last_tick_event_count e
total_events_emitted passam a usar o total real. Falha de reserva ou overflow
de ID desfaz o plano inteiro. V5 ja possui blocos explicitos de quantidade,
ordem e affected_entity; C2 passa a preencher os valores causais sem criar V6.

## 13. Consultas publicas minimas

As consultas propostas sao:

~~~c
bool minisnn_worlds_kernel_entity_has_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child);

MiniSNNWorldsKernelError minisnn_worlds_kernel_entity_spatial_parent(
    const MiniSNNWorldsKernel *kernel,
    MiniSNNWorldsKernelEntityId child,
    MiniSNNWorldsKernelSpatialLink *out_link);

size_t minisnn_worlds_kernel_spatial_link_count(
    const MiniSNNWorldsKernel *kernel);

MiniSNNWorldsKernelError minisnn_worlds_kernel_spatial_link_at(
    const MiniSNNWorldsKernel *kernel,
    size_t canonical_index,
    MiniSNNWorldsKernelSpatialLink *out_link);
~~~

entity_spatial_parent e spatial_link_at retornam copia completa. O indice de
spatial_link_at usa ordem (parent.value, child.value). Nao ha ponteiro interno,
API de grafo generica ou enumeracao de descendants nesta versao.

## 14. Atomicidade e plano

C1 estende StepPlan com copia planejada de links e seus contadores. Todo buffer
necessario para a operacao atual deve ser reservado antes de alterar estado
oficial. Falha interna preserva tick, entidades, transforms, occupancies,
links, fila pendente, eventos e contadores oficiais.

Em C1, CREATE/REMOVE geram no maximo o unico evento do proprio comando. C2 e
o primeiro bloco autorizado a exigir capacidade para eventos extras; C3 nao
adiciona mutacoes implicitas de lifecycle.

## 15. State hash V5 e persistencia futura

V5 pertence a K1-C1, pois a partir desse ponto links ativos, comandos pendentes,
eventos e contadores sao estado publico observavel. Manter V4 para esse estado
causaria colisao semantica de hashes.

K1-C1 deve introduzir:

- constante publica STATE_HASH_VERSION_V5;
- encoding explicito de links ativos em ordem (parent, child);
- endpoints de comandos pendentes CREATE/REMOVE;
- eventos SPATIAL_LINK_CREATED/SPATIAL_LINK_REMOVED e o payload completo;
- offset_x/offset_y;
- active_spatial_links e todos os contadores C1;
- count de eventos C1 e seu payload de link completo, em ordem de emissao;
- predicado de compatibilidade historica e selecao dinamica V1-V5;
- testes unitarios de encoding V5.

V1-V4 permanecem estritos e seus goldens nao mudam. Estados inteiramente
pre-K1-C continuam selecionando a versao historica compativel. Qualquer estado
com link ativo, comando de link pendente, evento de link no ultimo tick,
contador de link nao zero ou outro campo causal K1-C seleciona V5. Assim, V5
permanece selecionado depois de todos os links serem removidos se houver
historico causal.

C2 estende o bloco de eventos V5 com o total real, a ordem de eventos causais,
transforms movidos e affected_entity. K1-C4 cria o golden integrado, nao a
fundacao de V5. K2 podera reutilizar a mesma representacao para persistir links, offsets,
comandos pendentes, eventos, contadores, transforms, occupancies e assinatura;
K2 nao e implementado aqui.

## 16. Limites e falhas internas

Armazenamento usa vetores dinamicos checked, limitado por SIZE_MAX, alocacao e
identificadores. Deteccao de ciclo e coleta futura usam no maximo entity_count
membros, sem recursao. Overflow de contador/ID, capacidade insuficiente ou
estrutura interna invalida sao falhas internas atomicas, nunca
COMMAND_REJECTED.

## 17. Alternativas consideradas

| Decisao | Alternativa | Avaliacao | Escolha |
| --- | --- | --- | --- |
| parents | grafo com varios parents | offsets, ciclos e ownership ambiguos | um parent por child |
| offset | fornecido/repositiona child | mistura link com movimento e occupancy | derivado sem mover |
| lifecycle | auto-unlink no destroy | mutacao implicita e varios eventos por comando | rejeicao explicita |
| child direto | mover e recalcular offset | amplia semantica e falhas | rejeitar |
| observacao | LinkId | lifecycle, stale handle e hash extras | chave (parent, child) |
| propagacao | evento agregado | perde causalidade por entidade | evento por membro em C2 |

## 18. Recomendacao final

Adotar a floresta dirigida com dados publicos explicitos, lifecycle explicito,
um evento por comando em C1 e hash V5 desde o primeiro estado estrutural.
Bloquear destroy/remove-from-space incidentes elimina links pendurados sem
auto-unlink. C2 implementa, de forma separada e atomica, a translacao rigida e
os eventos causais extras.

## 19. Decomposicao corrigida

### K1-C1 - nucleo de spatial links e hash V5

Inclui armazenamento planejado/oficial, struct publico, comandos create/remove,
offset checked, self/duplicata/parent unico/ciclo, queries, eventos de
create/remove, diagnosticos, contadores, rejeicao de destroy e
remove-from-space incidentes, guarda de MOVE para preservar offsets, V5,
compatibilidade V1-V4 e testes unitarios/de atomicidade.

Exclui propagacao de movimento, multiplos eventos por comando, parser, demo e
stress integrado.

### K1-C2 - movimento rigido de subarvore

Inclui coleta canonica, movimento de raiz, rejeicao de child, preflight de
destinos, overflow, limites, occupancy externa, ignorar internos somente na
translacao, conflito canonico, affected_entity, eventos causais, delta zero,
rollback e preenchimento completo dos blocos V5.

### K1-C3 - ordering e hardening

Inclui matriz completa create/remove/move/destroy/remove-from-space/place/
occupancy, falhas de alocacao, overflows de capacidade e IDs, consultas
canonicas, long runs intermediarios e auditoria de invariantes. Nao altera a
regra de lifecycle explicita definida em C1.

### K1-C4 - demo, stress e fechamento

Inclui parser/config em app, demo reproduzivel, golden integrado K1-C, stress
de arvores, determinismo, O0/O2, ASan/UBSan, POSIX, checker, documentacao
final, auditoria integrada e regressao K0/K1-A/K1-B1/K1-B2.

## 20. Matriz de testes

K1-C1 deve cobrir dados/copias, offset derivado, orientacao independente,
unlink sem teleport, endpoints mortos/nao colocados, self, duplicata, parent
duplicado, ciclo, overflow, eventos aceitos/rejeitados, contadores, lifecycle
explicito, ordering basico, V5 e rollback de alocacao.

K1-C2 deve acrescentar arvore/cadeia, ordem de travessia, raiz, child
rejeitado, delta zero, limites, overflow de descendant, conflitos externos,
blocker/membro canonicos, multiplos eventos, total real de eventos e rollback.

C3/C4 cobrem matriz completa, long run, O0/O2, sanitizer, POSIX, stress,
golden e determinismo.

## 21. Riscos e mitigacoes

A propagacao exige buffers temporarios e eventos extras; C2 deve preflightar
capacidade, IDs e todos os destinos antes do commit. Ignorar membros internos
so e seguro para translacao rigida do conjunto inteiro. APIs futuras de
reparenting, offset explicito ou movimento direto do child exigem contratos
novos, nao extensoes silenciosas.

## 22. Questoes resolvidas

Nao resta questao semantica aberta para K1-C1 sobre payload, lifecycle, hash,
eventos, contadores, queries, diagnosticos ou ordering. A grafia exata dos
identificadores C deve seguir o prefixo local, sem alterar os significados
congelados neste documento. Auto-unlink, reparenting, LinkId, tipos de link e
Domain nao serao improvisados em C1.

## 23. Fora de escopo

Fisica, joints, rotacao de offset, velocidade, aceleracao, colisao continua,
pathfinding, navegacao, renderer, scene graph visual, animacao, Domain, Core,
Brain Bridge, GUI, persistencia K2, save/load, replay e semantica de produto.
## K1-C2 implementation note

K1-C2 implements the proposed rigid subtree translation using the planned C1 forest. Links and offsets remain immutable; C3 and C4 remain out of scope.
