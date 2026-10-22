import sqlite3
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()
cur.execute("SELECT bData FROM SigData4 WHERE sig_id=37")
bdata = cur.fetchone()[0]

floats = np.frombuffer(bdata[24:], dtype=np.float32)
freqs = floats[:1001]
p1 = floats[1001::2] # real
p2 = floats[1002::2] # imag

mag = np.sqrt(p1**2 + p2**2)
s11_db = 20 * np.log10(mag)

min_idx = np.argmin(s11_db)
f_r = freqs[min_idx]
s11_min = s11_db[min_idx]

below_10 = np.where(s11_db <= -10.0)[0]
f_L = freqs[below_10[0]]
f_H = freqs[below_10[-1]]
bw_ghz = f_H - f_L
bw_mhz = bw_ghz * 1000

print(f"CST Simulation Results:")
print(f"  Resonance Frequency f_r,sim: {f_r:.4f} GHz ({f_r*1000:.1f} MHz)")
print(f"  Minimum S11: {s11_min:.2f} dB")
print(f"  f_L (-10 dB): {f_L:.4f} GHz")
print(f"  f_H (-10 dB): {f_H:.4f} GHz")
print(f"  Bandwidth (-10 dB): {bw_mhz:.1f} MHz ({bw_ghz:.4f} GHz)")
print(f"  Fractional Bandwidth: {(bw_ghz/f_r)*100:.2f}%")
print(f"  S11 at 2.450 GHz: {s11_db[np.argmin(np.abs(freqs - 2.45))]:.2f} dB")
