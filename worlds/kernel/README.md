# miniSNN Worlds Kernel

K0-A e K0-B fornecem uma biblioteca estatica C11 independente para lifecycle,
tempo logico deterministico, entidades, comandos futuros e eventos. A
biblioteca resultante e
`build/worlds/kernel/lib/libminisnn_worlds_kernel.a`.

```powershell
mingw32-make worlds-kernel
mingw32-make worlds-kernel-test
mingw32-make audit-k0-a
mingw32-make audit-k0-b
```

Um consumidor normal inclui somente `minisnn_worlds_kernel.h`. O Kernel nao
usa relogio real, aleatoriedade, threads, Core, Studio ou conceitos de dominio.
Consulte [a arquitetura](docs/ARCHITECTURE.md), o
[contrato de tempo](docs/TIME_AND_TICK_CONTRACT.md), o
[contrato de Entity IDs](docs/ENTITY_ID_CONTRACT.md) e o
[contrato de comandos e eventos](docs/COMMAND_AND_EVENT_CONTRACT.md).

K0-A e K0-B estao concluidos. K0 permanece em andamento; K0-C sera o proximo
sub-bloco apos a auditoria K0-B bem-sucedida.
