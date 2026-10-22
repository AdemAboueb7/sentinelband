import os
import sys
import zipfile
import xml.etree.ElementTree as ET

sys.stdout.reconfigure(encoding='utf-8')

def read_docx(docx_path):
    print(f"\n=================== DOCX: {os.path.basename(docx_path)} ===================")
    try:
        with zipfile.ZipFile(docx_path) as z:
            xml_content = z.read('word/document.xml')
        tree = ET.fromstring(xml_content)
        paragraphs = []
        for p in tree.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}p'):
            texts = [t.text for t in p.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t') if t.text]
            if texts:
                paragraphs.append(''.join(texts))
        print('\n'.join(paragraphs))
    except Exception as e:
        print(f"Error reading {docx_path}: {e}")

folder = r'C:\Users\admab\OneDrive\Bureau\projet wearable tech'
for f in os.listdir(folder):
    if f.endswith('.docx'):
        read_docx(os.path.join(folder, f))

read_docx(r'C:\Users\admab\OneDrive\Bureau\RENDU.docx')
