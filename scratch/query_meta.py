import sqlite3

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()
cur.execute("SELECT name, d_text, d_int, d_float1, d_float2 FROM MetaData WHERE name LIKE '%Farfield%' OR name LIKE '%Gain%' OR name LIKE '%Directivity%' OR name LIKE '%Eff%' OR name LIKE '%Rad%'")
for r in cur.fetchall():
    print(r)
