import sqlite3
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()
cur.execute("SELECT bData FROM SigData4 WHERE sig_id=37")
bdata = cur.fetchone()[0]

floats = np.frombuffer(bdata[24:], dtype=np.float32)

freqs = floats[:1001]
p1 = floats[1001::2]
p2 = floats[1002::2]

print(f"freqs: {len(freqs)}, p1: {len(p1)}, p2: {len(p2)}")
print(f"At f=2.0 GHz: p1[0]={p1[0]}, p2[0]={p2[0]}")
print(f"Hypothesis A: p1=real, p2=imag -> mag={np.sqrt(p1[0]**2 + p2[0]**2):.4f}, dB={20*np.log10(np.sqrt(p1[0]**2 + p2[0]**2)):.2f} dB")
print(f"Hypothesis B: p1=mag, p2=phase -> mag={p1[0]:.4f}, dB={20*np.log10(p1[0]):.2f} dB")
print(f"Hypothesis C: p2=mag, p1=phase -> mag={p2[0]:.4f}, dB={20*np.log10(p2[0]):.2f} dB")

# Let's test Hypothesis C (where p2 is around 0.932 -> -0.61 dB)
# Let's see the minimum of p2 across all frequencies!
min_idx_c = np.argmin(p2)
print(f"\nIf p2 is magnitude:")
print(f"  Min magnitude: {p2[min_idx_c]:.4f} at f={freqs[min_idx_c]:.4f} GHz -> {20*np.log10(p2[min_idx_c]):.2f} dB")

# Let's test Hypothesis A (p1=real, p2=imag):
mag_a = np.sqrt(p1**2 + p2**2)
min_idx_a = np.argmin(mag_a)
print(f"\nIf p1=real, p2=imag:")
print(f"  Min magnitude: {mag_a[min_idx_a]:.4f} at f={freqs[min_idx_a]:.4f} GHz -> {20*np.log10(mag_a[min_idx_a]):.2f} dB")
