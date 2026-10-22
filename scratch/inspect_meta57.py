import sqlite3

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()

cur.execute("SELECT bData FROM SigMetaBlobData WHERE sig_id=57")
row = cur.fetchone()
if row and row[0]:
    bdata = row[0]
    print("Sig 57 Meta len:", len(bdata))
    # print printable ascii strings
    import re
    strings = re.findall(rb'[\x20-\x7e]{3,}', bdata)
    print("Strings in Sig 57 meta:")
    for s in strings:
        print(" ", s.decode('ascii', errors='ignore'))
