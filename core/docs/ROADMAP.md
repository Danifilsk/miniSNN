# Roadmap

O roadmap indica ordem de trabalho, não promessa de prazo.

## Implementado

- [x] A1 — topologias completas e opções de conectividade.
- [x] A2 — visualização do neurônio detalhado.
- [x] A3 — comparação de execuções.
- [x] A3.1 — seletor iniciando em `results/scenarios`.
- [x] A4 — nomes únicos, organização e históricos.
- [x] A5 — métricas ampliadas e diagnóstico `off/basic/full`.
- [x] A6 — consolidação documental e validação de referências.
- [x] Bloco B — auditoria numérica, estrutural, robustez, regressão e benchmark.
- [x] C1 — STDP aditivo por traces para sinapses de origem EXC.
- [x] C1.1 — relatórios HTML locais de métricas e pesos.
- [x] C1.5 — homeostase e estabilidade neural simplificadas.
- [x] C2 — recompensa, punição e R-STDP com trace de elegibilidade.
- [x] C3 — neuroevolução serial com topologia fixa, fitness configurável e checkpoint.
- [x] C4 — topologia adaptativa, evolução estrutural e plasticidade estrutural de lifetime.
- [x] C5 — modelos neuronais avançados: LIF, AdEx e Hodgkin-Huxley.

## Próximo

- [x] C6 — protocolos temporais: C6.1 memoria de trabalho, C6.2 memoria associativa e C6.3 sequencias e previsao.
- [ ] C7 — estados internos.
- [ ] D1 — integração do Core com o próximo estágio de domínio.
- [ ] Revisão humana do checklist do Studio.

A parte automática do Bloco B foi concluída. ASan/UBSan permanece pendente em
toolchain compatível.

## Ordem oficial posterior

```text
C6 -> C7 -> D1 -> Worlds
```

O C8 — otimização iterativa automatizada — pode evoluir em paralelo e não
bloqueia Worlds. O Bloco E permanece pausado até nova decisão de produto.

## Registro histórico

- C2 — recompensa e punição (concluído).
- C3 — neuroevolução (concluído). No fechamento histórico do C2, o estado era: C3 — neuroevolução (próximo; não implementado).
- C4 — topologia adaptativa e evolução estrutural (concluído).
- C5 — modelos neuronais avançados (concluído).

## Experimental

Os programas em `experiments/`, o STDP experimental do C1, a homeostase
simplificada do C1.5, o R-STDP do C2, a neuroevolução do C3, as heurísticas
estruturais do C4 e as heurísticas diagnósticas apoiam exploração, mas não
tornam Worlds implementado. LIF, AdEx e Hodgkin-Huxley são recursos do Core
concluídos no C5. Novas
hipóteses devem entrar no roadmap antes de serem descritas como recurso.
