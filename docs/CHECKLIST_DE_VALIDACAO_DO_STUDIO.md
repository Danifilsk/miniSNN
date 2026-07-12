# Checklist de validação do Studio

Este checklist é manual. Compilar não confirma cliques ou mensagens da janela.
Status atual: **VALIDAÇÃO MANUAL PENDENTE**.

## Preparação e cenários

- [ ] Compilar com `mingw32-make studio-build` e abrir o Studio.
- [ ] Confirmar janela, barra escura, campos e botões legíveis.
- [ ] Criar, carregar e salvar uma config.
- [ ] Rodar chain, ring, all-to-all, random, random balanced, small-world e feedforward.
- [ ] Confirmar nomes únicos e cancelamentos sem travamento.

## Resultados

- [ ] Abrir resultados, última execução e histórico.
- [ ] Gerar gráficos gerais e do neurônio detalhado.
- [ ] Executar diagnóstico `off`, `basic` e `full`.
- [ ] Abrir métricas e diagnóstico.
- [ ] Comparar runs e abrir a comparação.

## Falhas controladas

- [ ] Testar Python ausente e conferir mensagem clara.
- [ ] Selecionar Python válido e repetir.
- [ ] Testar resultado ausente e cancelamentos.
- [ ] Confirmar pastas iniciais corretas e erros compreensíveis.

Registrar data, Windows, Python e falhas após execução humana. Não marcar como
passou apenas porque a suíte automatizada compilou.

