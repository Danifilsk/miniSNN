# Dependencias planejadas

```text
miniSNN Core        sem dependencia de Worlds
Worlds Kernel       independente de miniSNN Core
Worlds Domain       futura camada de produto
Brain Bridge        futuro ponto de integracao entre Core e Domain
```

Durante M1, somente `core/` e construido. O Makefile raiz coordena o Makefile
do Core e nao exige a existencia de qualquer pasta de Worlds.
