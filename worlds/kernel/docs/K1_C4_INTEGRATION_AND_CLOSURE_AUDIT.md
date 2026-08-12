# K1-C4 - Integracao e Fechamento de K1

K1-C4 integra contratos entregues por K1-A, K1-B1, K1-B2 e K1-C1..C3; nao adiciona mecanicas ao Worlds Kernel.

O demo k1_spatial_links_demo carrega uma configuracao INI limitada na camada app.
Ele exercita bootstrap, floresta com multiplas raizes, movimento BFS de subarvore, delta
zero, rejeicao de movimento de child, overlap interno permitido na translacao conjunta,
colisao externa canonica, overflow, ordering no mesmo tick e lifecycle com unlink explicito.

Artefatos canonicos: config_used.ini, commands.csv, events.csv, entities.csv,
spatial_links.csv, diagnostics.csv, state_hash.txt e summary.txt. Nao contem tempo de
parede, endereco de memoria ou caminho absoluto. O checker C4 valida headers, ordem,
eventos causais, rejeicoes, links finais, contadores, V5 e invariantes.

O golden integrado e 0xF0FACC91E072D6D4. Ele foi observado em duas execucoes do demo
e congelado em tests/golden/k1_c4_integrated_hash.txt; nao substitui goldens C1--C3.

O stress deterministico executavel do gate usa 2.048 entidades e 1.024 links, com uma
arvore larga, doze arvores profundas, churn, placement, occupancy, lifecycle e
movimentos. O perfil originalmente recomendado de 10.000/5.000 excede o custo
quadratico do validador test-only atual e fica como escala de laboratorio, nao como
sucesso fingido de CI local. O harness chama o validador nos checkpoints dinamicos
deterministicos e no final, alem de repetir o workload e comparar hash e diagnosticos.

K1-C4 preserva state hash V5. Nao implementa reparenting, offsets mutaveis, joints,
containers semanticos ou tipos adicionais de link.
## Fechamento Oficial

K1-C4 esta concluido. `audit-k1-c4` encadeia C3, C2 e C1; estes, por sua vez,
encadeiam B2, B1, A e K0. `audit-k1-c` representa C1..C4, e o target raiz
`audit-k1` acrescenta arquitetura, documentacao e analyzer antes de delegar a
cadeia completa do Kernel.

O stress normal continua na escala de 2.048 entidades iniciais, 1.024 links e
64 ticks. O resultado deterministico de referencia do stress normal e 4.527
entidades movidas, 6 rejeicoes, 2 conflitos, 1 overflow, 1.027 links criados,
3 links removidos, 8.657 eventos e hash `0xDE7D773259958770`.

Quando ASan/UBSan estiver disponivel, o harness usa o mesmo teste estrutural
com `MINISNN_K1_C4_SANITIZER_STRESS`: 256 entidades, 128 links, raiz larga,
seis arvores profundas, occupancy interna, blocker externo, overflow,
lifecycle, churn e ordering. A reducao limita o custo quadratico do validador
test-only sob instrumentacao; ela nao substitui o stress normal nem cria um
novo golden publico. Sem uma toolchain sanitizer valida, o gate informa
`UNAVAILABLE`; apos uma probe valida, toda falha de compilacao, execucao ou
checker e `FAIL`.

K1 esta concluido como fundacao espacial. K2-A (snapshot canonico) esta concluido; K2-B e o proximo
bloco do Worlds Kernel; fisica, pathfinding, Domain, Brain Bridge, save/load e
replay ainda nao pertencem a K1.

> Historical roadmap note: this closure was written before K2-B. K2-B is now complete; K2-C deterministic command replay is next.
