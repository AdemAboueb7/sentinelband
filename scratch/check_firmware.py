import re

def search_in_file(path, patterns):
    print(f"\n=== {path} ===")
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
        for p in patterns:
            matches = re.findall(rf'.*{p}.*', content, re.IGNORECASE)
            print(f"Pattern '{p}' ({len(matches)} matches):")
            for m in matches[:5]:
                print(" ", m.strip())
    except Exception as e:
        print("Error:", e)

search_in_file(
    r'c:\Users\admab\OneDrive\Bureau\projet wearable tech\files\sentinelband-mobile-backend\sentinelband\firmware\SentinelBand\SentinelBand.ino',
    ['threshold', 'seuil', '0.35', '0.45', '0.20', 'mq2', 'gas', 'temp', 'lm35', 'freefall', 'impact', 'yellow', 'red', 'jaune', 'rouge', 'baud', 'pin', 'gpio']
)

search_in_file(
    r'c:\Users\admab\OneDrive\Bureau\projet wearable tech\files\sentinelband-mobile-backend\sentinelband\backend\SentinelBand_NoSIM\SentinelBand_NoSIM.ino',
    ['threshold', 'seuil', '0.35', '0.45', '0.20', 'mq2', 'gas', 'temp', 'freefall', 'impact']
)
