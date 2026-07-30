# miniSNN Worlds Kernel

K0-A, K0-B, K0-C e K0-D fornecem uma biblioteca estatica C11 independente para
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
mingw32-make demo-k0-d
mingw32-make audit-k0-d
mingw32-make audit-k0
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
[contrato de observabilidade](docs/OBSERVABILITY_CONTRACT.md), a
[configuracao de cenarios](docs/SCENARIO_CONFIG_CONTRACT.md) e os
[artefatos tecnicos](docs/ARTIFACT_CONTRACT.md).

K0 esta concluido como fundacao deterministica. K1 e o proximo bloco de
entidades genericas, transforms, espaco, ocupacao, barreiras, deslocamento e
links. K0-D continua headless e nao implementa Domain, Brain Bridge ou App.
