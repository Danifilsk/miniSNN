# Arquitetura do Worlds Kernel

O Worlds Kernel e um produto C11 headless, deterministico e independente.

- Kernel nao e Core.
- Kernel nao e Domain.
- Kernel nao e App.
- Kernel nao conhece Brain Bridge.

K0-A possui apenas configuracao, lifecycle, estados, erros, diagnostico e
tempo logico. A biblioteca nao inclui headers do Core, Win32 ou APIs de thread.
O Core nao inclui headers do Kernel. Uma integracao futura pertence somente a
Brain Bridge, que ainda nao existe.

Os outputs ficam em `build/worlds/kernel/`; fontes e headers nunca recebem
objetos ou executaveis.
