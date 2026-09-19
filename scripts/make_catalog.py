"""Build a bounded DFPlayer catalog from /01/001.mp3 or /MP3/0001.mp3.

Usage: python scripts/make_catalog.py <card-folder> --out catalog.json
Optional: pip install mutagen to read title/artist tags. Filenames are the fallback.
"""
import argparse
import json
import re
from pathlib import Path

def build(root):
    tracks=[]; seen=set()
    for folder in sorted(root.iterdir()):
        if not folder.is_dir(): continue
        if folder.name.upper()=='MP3': number=0; width=4
        elif re.fullmatch(r'\d{2}',folder.name) and 1<=int(folder.name)<=99: number=int(folder.name); width=3
        else: continue
        for file in sorted(folder.iterdir()):
            if file.suffix.lower() not in ('.mp3','.wav','.wma'): continue
            match=re.match(r'(\d{%d})'%width,file.stem)
            if not match: continue
            index=int(match.group(1))
            if not 1<=index<=(255 if number else 3000): continue
            if (number,index) in seen: raise ValueError(f'Duplicate DFPlayer address: {folder.name}/{index}')
            seen.add((number,index)); title=file.stem[width:].lstrip(' -_') or file.stem; artist=''
            try:
                from mutagen import File
                tags=File(file,easy=True)
                if tags:
                    title=(tags.get('title') or [title])[0];artist=(tags.get('artist') or [''])[0]
            except ImportError: pass
            # Firmware bounds are UTF-8 bytes, not Python characters.
            trim=lambda text,n:text.encode('utf-8')[:n].decode('utf-8','ignore')
            tracks.append(dict(folder=number,track=index,title=trim(title,63),artist=trim(artist,39)))
    if len(tracks)>96: raise ValueError('Catalog exceeds 96 tracks; choose a smaller card collection')
    return tracks

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path)
    parser.add_argument('--out',type=Path,default=Path('catalog.json'));args=parser.parse_args()
    data=json.dumps(build(args.root),ensure_ascii=False,separators=(',',':'))
    if len(data.encode('utf-8'))>14000: parser.error('Catalog exceeds the dashboard import limit of 14 KB')
    args.out.write_text(data,encoding='utf-8')
