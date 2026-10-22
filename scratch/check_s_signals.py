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
print(f"n_pts={n_pts}, dim1={dim1}, dim2={dim2}")

offset = 24
freqs = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
offset += n_pts * 4
val1 = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
offset += n_pts * 4
val2 = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])

for i in range(0, 1001, 100):
    print(f"i={i}: f={freqs[i]:.3f}, val1={val1[i]:.4e}, val2={val2[i]:.4e}")

# What if val1 is magnitude or dB?
val1 = np.array(val1)
val2 = np.array(val2)
print("val1 min, max:", val1.min(), val1.max())
print("val2 min, max:", val2.min(), val2.max())
