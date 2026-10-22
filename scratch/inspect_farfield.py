import sqlite3

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()

cur.execute("SELECT sig_id, dom, codom_real, codom_imag FROM SigData5")
for row in cur.fetchall():
    print(row)
