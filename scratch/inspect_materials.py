import sqlite3
import numpy as np

conn = sqlite3.connect(r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1\TS5\Result\Storage.sdb')
cur = conn.cursor()

for sid, name in [
    (22, 'Substrat_textile_eps_re'),
    (23, 'Substrat_textile_eps_im'),
    (24, 'Substrat_textile_eps_tgd'),
    (25, 'E_textile_Z_re'),
    (26, 'E_textile_Z_im')
]:
    cur.execute("SELECT table_id FROM SigHeader WHERE sig_id=?", (sid,))
    tid = cur.fetchone()[0]
    cur.execute(f"SELECT bData FROM SigData{tid} WHERE sig_id=?", (sid,))
    res = cur.fetchone()
    if res and res[0]:
        bdata = res[0]
        floats = np.frombuffer(bdata[24:], dtype=np.float32)
        print(f"Sig {sid} ({name}): len={len(floats)}")
        print(f"  vals: min={floats.min():.6f}, max={floats.max():.6f}, first 5={floats[:5]}")
