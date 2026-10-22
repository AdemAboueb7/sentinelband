import pypdf
import sys

sys.stdout.reconfigure(encoding='utf-8')
reader = pypdf.PdfReader(r'c:\Users\admab\OneDrive\Bureau\projet wearable tech\files\sentinelband-mobile-backend\sentinelband\backend\Cahier des charges du rapport technique.pdf')
with open('scratch/cahier_des_charges.txt', 'w', encoding='utf-8') as f:
    for i, p in enumerate(reader.pages):
        f.write(f"\n=== PAGE {i+1} ===\n")
        f.write(p.extract_text() or "")

print("Wrote scratch/cahier_des_charges.txt, pages:", len(reader.pages))
