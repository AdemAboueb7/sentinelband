import re

tex_path = r'c:\Users\admab\OneDrive\Bureau\projet wearable tech\files\sentinelband-mobile-backend\sentinelband\backend\rapport_technique_sentinelband.tex'

with open(tex_path, 'r', encoding='utf-8') as f:
    lines = f.readlines()

print(f"Total lines in {tex_path}: {len(lines)}")

acomps = []
for idx, line in enumerate(lines, 1):
    matches = re.findall(r'\\acomp\{([^}]+)\}', line)
    for m in matches:
        acomps.append((idx, m.strip(), line.strip()))

print(f"Found {len(acomps)} \\acomp markers:")
for idx, m, line in acomps:
    print(f"L{idx}: {m}")
