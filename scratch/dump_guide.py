import sys, zipfile, xml.etree.ElementTree as ET

sys.stdout.reconfigure(encoding='utf-8')
with zipfile.ZipFile(r"C:\Users\admab\OneDrive\Bureau\projet wearable tech\SentinelBand — Guide d'implémentation en 5 phases.docx") as z:
    xml_content = z.read('word/document.xml')
tree = ET.fromstring(xml_content)
paragraphs = []
for p in tree.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}p'):
    texts = [t.text for t in p.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t') if t.text]
    if texts:
        paragraphs.append(''.join(texts))

with open('scratch/guide_5_phases.txt', 'w', encoding='utf-8') as f:
    f.write('\n'.join(paragraphs))

print("Wrote scratch/guide_5_phases.txt, lines:", len(paragraphs))
