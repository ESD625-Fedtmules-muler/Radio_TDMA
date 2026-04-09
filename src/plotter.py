import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
# --- KONFIGURATION ---
# Vælg: 's' for sekunder, 'ms' for millisekunder, 'us' for mikrosekunder
enhed = 's' 

# Ordbog til at styre skalering og labels
skalering_map = {
    's':  (1, "[s]"),
    'ms': (1e3, "[ms]"),
    'us': (1e6, "[µs]")
}

faktor, label_tekst = skalering_map.get(enhed, (1, "Sekunder [s]"))
# ---------------------

df = pd.read_csv(r"data\scope_4.csv", skiprows=[1])
df['x-axis'] = (df['x-axis'] - df['x-axis'].min()) * faktor
x_max = df['x-axis'].max()

fig = plt.figure()
ax = fig.add_subplot(1, 1, 1)

ax.plot(df['x-axis'], df['1'], color='gold', label='PPS', linewidth=1.5)
ax.plot(df['x-axis'], df['2'], color='green', label='Node 0', linewidth=1)
ax.plot(df['x-axis'], df['3'], color='blue', label="Node 9", linewidth=1)


ax.set_title(f"Node 0 and 9 timing")
ax.set_xlabel(label_tekst)
ax.set_ylabel("[V]")
ax.legend()
plt.tight_layout()

minor_ticks = np.arange(0, df['x-axis'].max(), 5)
ax.set_xticks(minor_ticks, minor=True)
#ax.set_yticks(minor_ticks, minor=True)
ax.grid(True, 'both',alpha=0.3)
ax.set_xlim(0, 1.8)
ax.set_ylim(-0.7, 5)

plt.show()