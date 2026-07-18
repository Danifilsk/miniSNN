# Cognicao experimental C6

O C6 reune tres protocolos experimentais controlados. Eles medem propriedades
da dinamica e dos pesos de uma SNN; nao demonstram memoria geral, inteligencia,
linguagem, planejamento, memoria episodica, semantica ou autobiografica.

## Contratos comuns

Cada protocolo usa configuracao validada, seed explicita, passos inteiros,
tratamento imediato de erro neuronal e outputs locais. Durante avaliacao com
plasticidade congelada, os pesos nao podem mudar. A resposta neural e
decodificada antes de o alvo esperado entrar no calculo de metricas.

## Semantica oficial

| Protocolo | Onde a informacao reside | Reset/reconstrucao | Avaliacao |
| --- | --- | --- | --- |
| C6.1 memoria de trabalho | Estado dinamico transitorio | Apaga a retencao | Cue, delay e probe sem copia direta para o readout |
| C6.2 memoria associativa | Pesos aprendidos por STDP | Preserva a associacao ao recarregar o blueprint | Recall em rede limpa; alvo so mede a resposta |
| C6.3 predicao sequencial | Pesos de transicao e contexto | Preserva a previsao ao recarregar o blueprint | Prefixo sem alvo externo no delay/probe |

C6.2 e C6.3 usam um `teacher pulse supervisionado` somente durante treino
para gerar atividade pos-sinaptica do STDP. O alvo nunca recebe esse estimulo
na avaliacao.

## Persistencia

Os protocolos sinapticos escrevem um checkpoint de blueprint com modelo,
assinatura de configuracao neuronal, tipos EXC/INH, topologia, pesos e delays.
Ele nao armazena tensao, spikes, traces ou eligibility. Um checkpoint com
modelo ou assinatura incompativeis e rejeitado.

O checkpoint de blueprint nao e um cenario autossuficiente: ele deve ser usado
junto de uma configuracao efetiva compativel, normalmente `config_used.ini` da
mesma run. A configuracao fornece os parametros neuronais completos; o
checkpoint fornece a topologia herdada e os pesos aprendidos que sao validados
contra a assinatura neuronal antes da reconstrucao limpa.

## Suite integrada

Execute:

```powershell
mingw32-make scenario-c6-suite
```

Os arquivos ficam em `results/scenarios/c6_suite/`:
`c6_suite_summary.csv`, `c6_suite_summary.txt` e `c6_suite_report.html`.
O relatorio mostra cada protocolo separadamente, seus controles e margens; nao
combina os resultados em um score universal.

Os demos LIF sao as referencias cientificas. LIF, AdEx e Hodgkin-Huxley possuem
smokes de criacao e execucao finita; isso nao implica que os tres modelos tenham
o mesmo desempenho sem calibracao especifica.
