# miniSNN Worlds

`worlds/` abriga produtos futuros separados do miniSNN Core. O produto presente
e o **miniSNN Worlds Kernel**, em `worlds/kernel/`, iniciado em K0-A e
estendido por K0-B e K0-C.

O Kernel e headless, deterministico e generico. Ele nao depende do Core, nao
conhece Domain, Brain Bridge ou interface grafica. O Core permanece um produto
independente em `core/`.

K0-A estabelece lifecycle, configuracao, tempo logico e diagnostico minimo.
K0-B estabelece entidades, comandos futuros e eventos deterministas. K0-C
estabelece streams PRNG, hash canonico e observabilidade. K0-D estabelece um
runner configuravel, artefatos tecnicos e auditoria de stress fora da
biblioteca. K0 e K1 estao concluidos: K1-A, K1-B1/B2 e K1-C1..C4 formam a fundacao espacial deterministica. K2-A concluiu o snapshot canonico em memoria, K2-B concluiu restore e save/load e K2-C concluiu command replay deterministico e K2-D concluiu auditoria final de persistencia; WD0 - Domain minimo, WB0 - Brain Bridge minimo e WD1 - persistencia do Domain estao concluidos; WF0 - organismo neural headless e o proximo bloco.

## K1-A Space And Transforms

K0 and K1 are complete. K1-A through K1-C4 form the deterministic spatial foundation; K2-A through K2-D are complete; WD0 - Worlds Domain and WB0 - Brain Bridge minimo are complete; WD1 - Domain persistence is complete; WF0 - neural organism headless is next. The public Worlds
Kernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the K1-A coordinate and
transform contracts in worlds/kernel/docs.

## K1-B1 Occupancy

K1-B1/B2 e K1-C1..C4 estao concluidos. O Kernel fornece occupancy AABB, movimento atomico, links espaciais canonicos, translacao rigida e hash V5; K2-A ate K2-D estao concluidos; WD0 - Domain minimo, WB0 - Brain Bridge minimo e WD1 - persistencia do Domain estao concluidos; WF0 - organismo neural headless e o proximo bloco. The Worlds Kernel supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/K1_B1_OCCUPANCY_BARRIER_AUDIT.md.
## K1-B2 Atomic Movement

K1-B2 is complete. The Worlds Kernel now supports command-only atomic fixed-point displacement, destination AABB validation, canonical conflict rejection, movement diagnostics and state hash v4. See worlds/kernel/docs/MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and worlds/kernel/docs/K1_B2_MOVEMENT_AUDIT.md.
## K1-C1 Spatial Links

K1-C1 is complete in `worlds/kernel/`: canonical directed parent-to-child
links, explicit lifecycle rejection, guarded linked movement and state hash V5
are available. K1-C2 traduz subarvores atomicas; C3 completa ordering e C4 fecha o demo, artefatos e stress.
remain outside this release.

K1-C2 in Worlds Kernel provides deterministic atomic rigid translation for C1 spatial-link subtrees. No C3/C4 behavior is included.

K1-C4 fecha a fundacao espacial do Kernel com demo reproduzivel, artefatos canonicos, golden V5 e stress deterministico.
