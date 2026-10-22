import os
import sys
import pypdf

sys.stdout.reconfigure(encoding='utf-8')
folder = r'C:\Users\admab\OneDrive\Bureau\projet wearable tech'
for f in os.listdir(folder):
    if f.endswith('.pdf'):
        fpath = os.path.join(folder, f)
        print("FOUND PDF:", repr(f))
        reader = pypdf.PdfReader(fpath)
        for i, page in enumerate(reader.pages):
            print(f"--- {f} PAGE {i+1} ---")
            print(page.extract_text())
