# miniSNN Worlds Kernel

K0-A fornece uma biblioteca estatica C11 independente para lifecycle e tempo
logico deterministico. A biblioteca resultante e
`build/worlds/kernel/lib/libminisnn_worlds_kernel.a`.

```powershell
mingw32-make worlds-kernel
mingw32-make worlds-kernel-test
mingw32-make audit-k0-a
```

Um consumidor normal inclui somente `minisnn_worlds_kernel.h`. O Kernel nao
usa relogio real, aleatoriedade, threads, Core, Studio ou conceitos de dominio.
Consulte [a arquitetura](docs/ARCHITECTURE.md) e o
[contrato de tempo](docs/TIME_AND_TICK_CONTRACT.md).
