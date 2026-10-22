import sqlite3
import struct
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()
cur.execute("SELECT bData FROM SigData4 WHERE sig_id=37")
bdata = cur.fetchone()[0]

# Let's inspect all floats in bdata
floats = np.frombuffer(bdata[24:], dtype=np.float32)
print("Total float32s:", len(floats))

# If interleaved: (freq, re, im) or (re, im, freq) or something?
# Let's check where 2.0, 2.001, etc. are:
print("First 10 float32s:", floats[:10])
print("Middle float32s at 1001:", floats[1001:1011])
print("Floats at 2002:", floats[2002:2012])

# Let's check doubles (float64)
doubles = np.frombuffer(bdata[24:], dtype=np.float64)
print("Total float64s:", len(doubles))
print("First 10 float64s:", doubles[:10])
