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
[instalacao e linkagem](core/docs/INSTALLING_AND_LINKING.md). Os pacotes locais
de avaliacao ficam em `dist/` apos `mingw32-make package-release`; eles nao sao
uma publicacao, tag ou release definitiva. D1 esta concluido para
**miniSNN Core 1.0.0-rc.1**: os gates automaticos e a validacao manual do
Studio foram registrados. A API Core-Brain Bridge continua candidata e
provisoria ate D2; K0 esta concluido como fundacao deterministica do Worlds
Kernel.

Este repositorio esta preparado para abrigar produtos relacionados, com limites
de dependencia explicitos.

- **miniSNN Core**: a biblioteca e o laboratorio neural existentes, em
  [`core/`](core/README.md).
- **miniSNN Worlds Kernel**: produto headless e independente em
  [`worlds/kernel/`](worlds/kernel/), com K0-A a K0-D concluidos para
  lifecycle, tempo logico, entidades, comandos, eventos, PRNG por streams,
  hash canonico, observabilidade, cenario configuravel e artefatos tecnicos.
  K1 e o proximo bloco; Domain, Brain Bridge e App seguem nao criados.
- **miniSNN Worlds Domain**: ainda nao criado.
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
