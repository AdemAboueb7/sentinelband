import sqlite3
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()

cur.execute("SELECT bData FROM SigData6 WHERE sig_id=57")
bdata = cur.fetchone()[0]

doubles = np.frombuffer(bdata[40:], dtype=np.float64)
cols = doubles[9:].reshape(9, 360)

print("Col 0:", cols[0, :10])
for i in range(9):
    print(f"Index {i}: min={cols[i].min():.4f}, max={cols[i].max():.4f}")

# Also check row-major (360, 9)
rowm = doubles[9:].reshape(360, 9)
print("\nRow major Col 0:", rowm[:10, 0])
for i in range(9):
    print(f"RowMajor Col {i}: min={rowm[:, i].min():.4f}, max={rowm[:, i].max():.4f}")
