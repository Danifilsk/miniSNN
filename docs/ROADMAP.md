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

## Próximo

- [ ] **Bloco B — validação científica e engenharia de robustez.**

O Bloco B deve aprofundar testes numéricos do LIF, determinismo, topologias pelo
runner/Studio, fórmulas de métricas, sanitizers e regressões.

## Ordem oficial posterior

```text
Bloco A
-> Bloco B
-> Core v0.2
-> E0: entrega científica inicial
-> Worlds mínimo como testbed
-> Bloco C
-> Bloco D
-> E avançado
-> expansão completa do Worlds
```

`Core v0.2`, E0 e Worlds mínimo são marcos planejados, não releases existentes.

## Planejado — Bloco C

- C1 — STDP.
- C1.5 — homeostase.
- C2 — recompensa.
- C3 — neuroevolução.
- C4 — topologia adaptativa.
- C5 — modelos neurais avançados.
- C6 — memória.
- C7 — estados internos.
- C8 — otimização iterativa automatizada.

## Experimental

Os programas em `experiments/` e as heurísticas diagnósticas apoiam exploração,
mas não tornam plasticidade, Worlds ou modelos avançados implementados. Novas
hipóteses devem entrar no roadmap antes de serem descritas como recurso.
