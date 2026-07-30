# Arquitetura do Worlds Kernel

O Worlds Kernel e um produto C11 headless, deterministico e independente.

- Kernel nao e Core.
- Kernel nao e Domain.
- Kernel nao e App.
- Kernel nao conhece Brain Bridge.

K0-A fornece configuracao, lifecycle, estados, erros, diagnostico e tempo
logico. K0-B adiciona somente um registro de entidades por ID forte, uma fila
de comandos futuros e uma janela de eventos. K0-C adiciona seed explicita,
streams PCG32 independentes, hash canonico e observabilidade por copia. A
execucao de cada tick prepara um plano temporario completo e o promove em um
unico commit. K0-D coloca parser INI, runner, CSV, manifesto e report somente
em `app/`; a biblioteca continua sem filesystem, CLI ou formatos de artefato.

A biblioteca nao inclui headers do Core, Win32 ou APIs de thread. O Core nao
inclui headers do Kernel. Uma integracao futura pertence somente a Brain
Bridge, que ainda nao existe. O Kernel nao define componentes, espacamento,
mapas, agentes ou semantica de dominio.

Os outputs ficam em `build/worlds/kernel/`; fontes e headers nunca recebem
objetos ou executaveis.
