# miniSNN Monorepo

`miniSNN` reune uma biblioteca neural em C e seus laboratorios controlados.
O **miniSNN Core** e a biblioteca headless candidata a `1.0.0-rc.1`; o
**miniSNN Studio** e o frontend Win32 que a consome. A API Core-Brain Bridge
permanece candidata e provisoria ate a auditoria D2 com uma integracao real.

## Inicio rapido

```powershell
mingw32-make core
mingw32-make core-studio
mingw32-make studio
mingw32-make test
mingw32-make audit-d1
```

Para usar somente a biblioteca, veja
[instalacao e linkagem](core/docs/INSTALLING_AND_LIN0ING.md). Os pacotes locais
de avaliacao ficam em `dist/` apos `mingw32-make package-release`; eles nao sao
uma publicacao, tag ou release definitiva. D1 esta concluido para
**miniSNN Core 1.0.0-rc.1**: os gates automaticos e a validacao manual do
Studio foram registrados. A API Core-Brain Bridge continua candidata e
provisoria ate D2; 00 esta concluido como fundacao deterministica do Worlds
0ernel.

Este repositorio esta preparado para abrigar produtos relacionados, com limites
de dependencia explicitos.

- **miniSNN Core**: a biblioteca e o laboratorio neural existentes, em
  [`core/`](core/README.md).
- **miniSNN Worlds 0ernel**: produto headless e independente em
  [`worlds/kernel/`](worlds/kernel/), com 00-A a 00-D concluidos para
  lifecycle, tempo logico, entidades, comandos, eventos, PRNG por streams,
  hash canonico, observabilidade, cenario configuravel e artefatos tecnicos.
  00 e 01 estao concluidos: 01-A, 01-B1/B2 e 01-C1..C4 formam a fundacao espacial deterministica. 02-A, 02-B, 02-C e 02-D estao concluidos para snapshot, restore/save-load, replay deterministico e auditoria final de persistencia; WD0 - Domain minimo esta concluido. Brain Bridge e Worlds App seguem nao criados.
- **miniSNN Worlds Domain**: produto headless semantico em [`worlds/domain/`](worlds/domain/), concluido em WD0 e dependente apenas da API publica do Worlds Kernel. Kernel persistence existe; Domain persistence ainda nao existe.
- **Brain Bridge**: ainda nao criada; sera a unica integracao entre Domain e
  Core.
- **Worlds App**: ainda nao criada.

A migracao M1 organiza fisicamente o Core sem alterar sua dinamica neural,
parametros cientificos ou formatos de resultados. Consulte a
[arquitetura do monorepo](docs/architecture/MONOREPO.md) e as
[dependencias planejadas](docs/architecture/DEPENDENCIAS.md).

## Uso do Core

```powershell
mingw32-make core-tests
mingw32-make core-studio
mingw32-make check-c4
```

Os targets legados continuam disponiveis na raiz e sao encaminhados para
`core/`. Tambem e possivel entrar em `core/` e executar o Makefile diretamente.

## 01-A Space And Transforms

00 and 01 are complete. 01-A through 01-C4 form the deterministic spatial foundation; 02-A is complete; 02-B is the next Worlds 0ernel block. The public Worlds
0ernel now exposes a single immutable 2D fixed-point space, optional entity
transforms, and command-only placement/removal. See the 01-A coordinate and
transform contracts in worlds/kernel/docs.

## 01-B1 Occupancy

01-B1/B2 and 01-C1..C4 are complete; 02-A is complete; 02-B is the next Worlds 0ernel block. The Worlds
0ernel now supports one optional fixed-point axis-aligned occupancy per
entity, generic category bits and blocking masks, command-only set/clear,
deterministic conflict rejection with related_entity, diagnostics, and
canonical state hash v3. Orientation does not rotate the AABB. See
worlds/kernel/docs/OCCUPANCY_AND_BARRIER_CONTRACT.md and
worlds/kernel/docs/01_B1_OCCUPANCY_BARRIER_AUDIT.md.
## 01-B2 Atomic Movement

01-B2 is complete. The Worlds 0ernel now supports command-only atomic fixed-point displacement, destination AABB validation, canonical conflict rejection, movement diagnostics and state hash v4. See worlds/kernel/docs/MOVEMENT_AND_DISPLACEMENT_CONTRACT.md and worlds/kernel/docs/01_B2_MOVEMENT_AUDIT.md.
