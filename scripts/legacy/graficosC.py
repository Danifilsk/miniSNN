import pandas as pd
import matplotlib.pyplot as plt

dados = pd.read_csv("spikes.csv")

plt.figure(figsize=(12,6))

plt.scatter(
    dados["tempo"],
    dados["neuronio"],
    s=4,
    marker=".",
)

plt.xlabel("Tempo (passos)")
plt.ylabel("Neurônio")
plt.title("Raster Plot - miniSNN")

plt.xlim(0, dados["tempo"].max())

plt.ylim(-1, dados["neuronio"].max() + 1)

plt.grid(alpha=0.3)

plt.tight_layout()

plt.show()