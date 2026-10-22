import sqlite3
import struct
import numpy as np
import os

projects = ['T', 'T2', 'TS5']
base_dir = r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies\TP1'

for prj in projects:
    sdb_path = os.path.join(base_dir, prj, 'Result', 'Storage.sdb')
    if os.path.exists(sdb_path):
        conn = sqlite3.connect(sdb_path)
        cur = conn.cursor()
        cur.execute("SELECT sig_id, name, choice, table_id FROM SigHeader WHERE name LIKE '%S1(1)1(1)%'")
        for sig_id, name, choice, table_id in cur.fetchall():
            cur.execute(f"SELECT bData FROM SigData{table_id} WHERE sig_id=?", (sig_id,))
            res = cur.fetchone()
            if res and res[0]:
                bdata = res[0]
                n_pts = struct.unpack("<Q", bdata[:8])[0]
                offset = 24
                freqs = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
                offset += n_pts * 4
                reals = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
                offset += n_pts * 4
                imags = struct.unpack(f"<{n_pts}f", bdata[offset : offset + n_pts*4])
                
                freqs = np.array(freqs)
                reals = np.array(reals)
                imags = np.array(imags)
                s11_db = 20 * np.log10(np.sqrt(reals**2 + imags**2))
                min_idx = np.argmin(s11_db)
                print(f"Project {prj} - Sig {sig_id} ({name}, choice={choice}):")
                print(f"  f_range: [{freqs[0]:.2f}, {freqs[-1]:.2f}] GHz")
                print(f"  Min S11: {s11_db[min_idx]:.2f} dB at f = {freqs[min_idx]:.4f} GHz")
                print(f"  S11 at 2.0 GHz: {s11_db[0]:.2f} dB, at 3.0 GHz: {s11_db[-1]:.2f} dB")
                # find -10 dB bandwidth
                below_10 = np.where(s11_db < -10)[0]
                if len(below_10) > 0:
                    fL = freqs[below_10[0]]
                    fH = freqs[below_10[-1]]
                    bw = (fH - fL) * 1000
                    print(f"  BW (-10dB): fL={fL:.4f} GHz, fH={fH:.4f} GHz, BW={bw:.1f} MHz")
                else:
                    print("  BW (-10dB): None")
