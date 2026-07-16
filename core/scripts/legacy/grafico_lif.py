import pandas as pd
import matplotlib.pyplot as plt

dados = pd.read_csv("lif.csv")

plt.figure(figsize=(12,5))

plt.plot(dados["tempo"], dados["V"], label="Potencial de membrana")

# Marca os spikes em vermelho
spikes = dados[dados["spike"] == 1]
plt.scatter(spikes["tempo"], spikes["V"], s=20, label="Spike")

plt.xlabel("Tempo")
plt.ylabel("Potencial (mV)")
plt.title("Neurônio LIF")

plt.grid(True)
plt.legend()

plt.tight_layout()
plt.show()