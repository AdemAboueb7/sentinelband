import sqlite3
import struct
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()

cur.execute("SELECT bData FROM SigData4 WHERE sig_id=37")
bdata = cur.fetchone()[0]

n_pts = struct.unpack("<Q", bdata[:8])[0]
dim1 = struct.unpack("<Q", bdata[8:16])[0]
dim2 = struct.unpack("<Q", bdata[16:24])[0]
print(f"n_pts={n_pts}, dim1={dim1}, dim2={dim2}, len={len(bdata)}")

# If n_pts=1001:
# freq: 1001 floats (4 bytes each) = 4004 bytes
# real: 1001 floats = 4004 bytes
# imag: 1001 floats = 4004 bytes
# total = 24 + 4004 * 3 = 12036 bytes exactly!

offset = 24
freqs = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
offset += n_pts * 4
reals = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
offset += n_pts * 4
imags = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])

freqs = np.array(freqs)
reals = np.array(reals)
imags = np.array(imags)

s11_mag = np.sqrt(reals**2 + imags**2)
s11_db = 20 * np.log10(s11_mag)

min_idx = np.argmin(s11_db)
print(f"CST S11 Result:")
print(f"  Min S11: {s11_db[min_idx]:.2f} dB at f_r = {freqs[min_idx]:.4f} GHz")
print(f"  Freq range: {freqs[0]:.4f} GHz to {freqs[-1]:.4f} GHz")

# S11 at 2.45 GHz:
idx_245 = np.argmin(np.abs(freqs - 2.45))
print(f"  S11 at {freqs[idx_245]:.4f} GHz: {s11_db[idx_245]:.2f} dB")
