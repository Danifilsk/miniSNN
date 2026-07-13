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

## C1 — plasticidade STDP

- [ ] Studio abre.
- [ ] Config antiga abre com STDP OFF.
- [ ] Janela `PLASTICIDADE` abre.
- [ ] Cancelar não altera os valores.
- [ ] Ativar STDP funciona.
- [ ] Campos inválidos são recusados.
- [ ] Config STDP salva.
- [ ] Config salva recarrega com os mesmos valores.
- [ ] `stdp_ltp_demo` roda.
- [ ] `stdp_ltd_demo` roda.
- [ ] `stdp_mixed_demo` roda.
- [ ] `weights_initial.csv` abre.
- [ ] `weights_final.csv` abre.
- [ ] `weight_history.csv` abre.
- [ ] `GRAFICO STDP` funciona.
- [ ] `ABRIR STDP` abre o PNG correto.
- [ ] STDP OFF preserva cenário antigo.
- [ ] Histórico geral continua funcionando.
- [ ] Diagnóstico continua funcionando.
- [ ] Comparação continua funcionando.
- [ ] Nomes únicos continuam funcionando.
- [ ] Cancelamentos não travam.
- [ ] Mensagens de erro são compreensíveis.

Status C1: **VALIDAÇÃO MANUAL PENDENTE**.
