# miniSNN Worlds Kernel

K0-A, K0-B e K0-C fornecem uma biblioteca estatica C11 independente para
lifecycle, tempo logico deterministico, entidades, comandos futuros, eventos,
PRNG por streams, hash canonico e observabilidade. A
biblioteca resultante e
`build/worlds/kernel/lib/libminisnn_worlds_kernel.a`.

```powershell
mingw32-make worlds-kernel
mingw32-make worlds-kernel-test
mingw32-make audit-k0-a
mingw32-make audit-k0-b
mingw32-make audit-k0-c
```

Um consumidor normal inclui somente `minisnn_worlds_kernel.h`. O Kernel nao
usa relogio real, threads, Core, Studio ou conceitos de dominio. A
aleatoriedade K0-C e PCG32 interno, versionado e derivado de seed explicita.
Consulte [a arquitetura](docs/ARCHITECTURE.md), o
[contrato de tempo](docs/TIME_AND_TICK_CONTRACT.md), o
[contrato de Entity IDs](docs/ENTITY_ID_CONTRACT.md) e o
[contrato de comandos e eventos](docs/COMMAND_AND_EVENT_CONTRACT.md), o
[contrato de aleatoriedade](docs/RANDOMNESS_CONTRACT.md), o
[contrato de hash](docs/STATE_HASH_CONTRACT.md) e o
[contrato de observabilidade](docs/OBSERVABILITY_CONTRACT.md).

K0-A, K0-B e K0-C estao concluidos. K0 permanece em andamento; K0-D e o
proximo sub-bloco.
