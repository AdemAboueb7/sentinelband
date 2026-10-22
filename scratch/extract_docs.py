import pypdf
import zipfile
import xml.etree.ElementTree as ET
import os
import glob

def read_pdf(pdf_path):
    out = f"\n=================== PDF: {pdf_path} ===================\n"
    try:
        reader = pypdf.PdfReader(pdf_path)
        out += f"Num pages: {len(reader.pages)}\n"
        for i, page in enumerate(reader.pages):
            out += f"\n--- Page {i+1} ---\n" + (page.extract_text() or "")
        return out
    except Exception as e:
        return f"Error reading {pdf_path}: {e}\n"

def read_docx(docx_path):
    out = f"\n=================== DOCX: {docx_path} ===================\n"
    try:
        with zipfile.ZipFile(docx_path) as z:
            xml_content = z.read('word/document.xml')
        tree = ET.fromstring(xml_content)
        paragraphs = []
        for p in tree.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}p'):
            texts = [t.text for t in p.iter('{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t') if t.text]
            if texts:
                paragraphs.append(''.join(texts))
        return out + '\n'.join(paragraphs) + '\n'
    except Exception as e:
        return f"Error reading {docx_path}: {e}\n"

dirs_to_scan = [
    r'c:\Users\admab\OneDrive\Bureau\projet wearable tech',
    r'C:\Users\admab\OneDrive\Bureau\5SLEAM1\Wearable Technologies',
    r'C:\Users\admab\OneDrive\Bureau',
]

with open('scratch/all_extracted_docs.txt', 'w', encoding='utf-8') as outfile:
    for d in dirs_to_scan:
        if os.path.exists(d):
            for fname in os.listdir(d):
                fpath = os.path.join(d, fname)
                if os.path.isfile(fpath):
                    ext = os.path.splitext(fname)[1].lower()
                    if ext == '.pdf':
                        print(f"Reading PDF: {fname}")
                        outfile.write(read_pdf(fpath) + "\n\n")
                    elif ext == '.docx':
                        print(f"Reading DOCX: {fname}")
                        outfile.write(read_docx(fpath) + "\n\n")
                    elif ext in ['.txt', '.md']:
                        print(f"Reading text: {fname}")
                        try:
                            with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                                outfile.write(f"\n=================== TEXT: {fpath} ===================\n" + f.read() + "\n\n")
                        except Exception as e:
                            print(f"Error reading {fpath}: {e}")
