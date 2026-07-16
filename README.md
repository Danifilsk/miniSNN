# miniSNN Monorepo

Este repositorio esta preparado para abrigar produtos relacionados, com limites
de dependencia explicitos.

- **miniSNN Core**: a biblioteca e o laboratorio neural existentes, em
  [`core/`](core/README.md).
- **miniSNN Worlds**: produto futuro; ainda nao foi criado neste repositorio.
- **Worlds Kernel**: futura biblioteca generica independente.

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
