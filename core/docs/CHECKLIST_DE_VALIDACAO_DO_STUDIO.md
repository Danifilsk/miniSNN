# Checklist de validação do Studio

## Patch C2 — histórico HTML

Status: **VALIDAÇÃO MANUAL PENDENTE**. A geração, segurança e integração de
código são validadas automaticamente; os cliques abaixo exigem uma sessão
humana do Studio.

- [ ] 1. Studio abre.
- [ ] 2. ABRIR HISTORICO permanece habilitado sem run atual.
- [ ] 3. Clicar ABRIR HISTORICO gera `history.html`.
- [ ] 4. O navegador abre o HTML, não `index.csv`.
- [ ] 5. O tema escuro aparece corretamente.
- [ ] 6. Cards mostram totais coerentes.
- [ ] 7. Runs aparecem da mais recente para a mais antiga.
- [ ] 8. Busca por `run_name` funciona.
- [ ] 9. Busca por topologia funciona.
- [ ] 10. Filtro de status funciona.
- [ ] 11. Link de métricas abre `metrics_report.html`.
- [ ] 12. Link de resumo abre `summary.txt`.
- [ ] 13. Link de manifesto funciona.
- [ ] 14. Links opcionais aparecem somente quando existem.
- [ ] 15. Run removida continua listada sem travar.
- [ ] 16. O link `index.csv` abre o dado bruto.
- [ ] 17. Nova execução adiciona uma linha ao CSV.
- [ ] 18. Reabrir o histórico mostra a nova execução.
- [ ] 19. Python ausente com HTML existente avisa e abre o último HTML.
- [ ] 20. Python ausente sem HTML mostra erro claro.
- [ ] 21. Nenhum CSV deixa de ser produzido.
- [ ] 22. O Studio não abre CSV silenciosamente.

## C2 — recompensa, punição e R-STDP

Status do bloco abaixo: **VALIDAÇÃO MANUAL PENDENTE**. A compilação do Studio
é automatizada, mas não substitui interação humana real.

- [ ] 1. Studio abre.
- [ ] 2. Config antiga abre com reward OFF.
- [ ] 3. Modal RECOMPENSA abre.
- [ ] 4. Modal PLASTICIDADE mostra os dois modos.
- [ ] 5. Cancelar preserva valores.
- [ ] 6. Direct STDP continua funcionando.
- [ ] 7. R-STDP pode ser selecionado.
- [ ] 8. Combinação inválida é recusada.
- [ ] 9. Campos inválidos são recusados.
- [ ] 10. Eventos são salvos.
- [ ] 11. Eventos são recarregados.
- [ ] 12. Reward positivo demo roda.
- [ ] 13. Punição demo roda.
- [ ] 14. Reward atrasado demo roda.
- [ ] 15. Mixed demo roda.
- [ ] 16. `reward_metrics.csv` é gerado.
- [ ] 17. `reward_events.csv` é gerado.
- [ ] 18. `eligibility_history.csv` é gerado.
- [ ] 19. GRAFICO RECOMPENSA funciona.
- [ ] 20. ABRIR RECOMPENSA abre HTML.
- [ ] 21. Relatório distingue recompensa e punição.
- [ ] 22. Relatório mostra elegibilidades.
- [ ] 23. ABRIR METRICAS inclui reward.
- [ ] 24. ABRIR PESOS identifica R-STDP.
- [ ] 25. Relatório de homeostase continua funcionando.
- [ ] 26. STDP direto antigo continua funcionando.
- [ ] 27. Homeostase continua funcionando.
- [ ] 28. Reward OFF preserva cenário antigo.
- [ ] 29. Comparação continua funcionando.
- [ ] 30. Histórico continua funcionando.
- [ ] 31. Nomes únicos continuam funcionando.
- [ ] 32. Python ausente mostra mensagem clara.
- [ ] 33. Cancelamentos não travam.
- [ ] 34. Last actual run directory é respeitado.

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

## C1.5 — homeostase

- [ ] Config antiga abre com homeostase OFF.
- [ ] Modal `HOMEOSTASE` abre e cancelar preserva valores.
- [ ] Threshold, scaling e ganho podem ser ativados separadamente.
- [ ] Campos inválidos são recusados e save/load preserva valores.
- [ ] Os três demos homeostáticos rodam.
- [ ] CSVs, `GRAFICO HOMEOSTASE` e `ABRIR HOMEOSTASE` funcionam.
- [ ] `ABRIR METRICAS` e `ABRIR PESOS` distinguem homeostase e STDP.
- [ ] STDP, histórico, diagnóstico, comparação e nomes únicos continuam funcionando.
- [ ] Python ausente e cancelamentos produzem mensagens claras sem travar.

Status C1.5: **VALIDAÇÃO MANUAL PENDENTE**.

## C3 — NEUROEVOLUCAO

- [ ] `NEUROEVOLUCAO` abre sem alterar o cenário principal.
- [ ] Config evolutiva válida carrega e preenche todos os controles.
- [ ] Config com caminho contendo espaços carrega e salva.
- [ ] Genes escalares e termos de fitness preservam as linhas.
- [ ] Bounds inválidos mostram erro claro.
- [ ] `RODAR EVOLUCAO` não bloqueia repintura da janela principal.
- [ ] O botão de nova evolução fica indisponível durante o processo.
- [ ] Uma segunda evolução simultânea é rejeitada.
- [ ] Status informa execução e conclusão.
- [ ] Falha do runner preserva o último experimento bem-sucedido.
- [ ] `RETOMAR EVOLUCAO` seleciona a pasta correta.
- [ ] Resume concluído não duplica gerações nos CSVs.
- [ ] `ABRIR EVOLUCAO` abre a pasta real.
- [ ] `ABRIR RELATORIO` abre `evolution_report.html`.
- [ ] `ABRIR GRAFICO` abre `evolution_overview.png`.
- [ ] `ABRIR MELHOR` abre o relatório ou pasta `best_run/`.
- [ ] `HISTORICO EVOLUTIVO` abre `results/evolution/history.html`.
- [ ] Python ausente usa HTML anterior com aviso, quando disponível.
- [ ] Python ausente sem HTML mostra erro e não abre `index.csv`.
- [ ] PNG e HTML são gerados após uma evolução bem-sucedida.
- [ ] Cancelar a janela não altera a config salva.
- [ ] Fechar o Studio durante evolução não encerra o processo à força.
- [ ] Handles são fechados após conclusão.
- [ ] Tema escuro e fonte retro permanecem legíveis.
- [ ] Campos longos não cortam informações críticas.
- [ ] Demos de peso, homeostase e plasticidade abrem corretamente.
- [ ] `last_experiment.txt` só aponta uma pasta existente após sucesso.
- [ ] Cenários normais continuam executando após usar a janela C3.
- [ ] Nenhum artefato é criado na raiz.

Status C3: **VALIDAÇÃO MANUAL PENDENTE**.
