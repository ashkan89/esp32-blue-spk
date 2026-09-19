"""Enforce linked-image budgets. Runtime reserves still require device tests."""
import argparse,json,subprocess
from pathlib import Path
from toolchain_paths import tool
ROOT=Path(__file__).resolve().parent.parent
def check(environment):
    build=ROOT/'.pio/build'/environment
    sections={}
    for line in subprocess.check_output([str(tool('size')),'-A',str(build/'firmware.elf')],text=True).splitlines():
        fields=line.split()
        if len(fields)>=3 and fields[0].startswith('.'):
            sections[fields[0]]=int(fields[1])
    iram=sum(size for name,size in sections.items() if name.startswith('.iram0'))
    dram=sum(sections.get(name,0) for name in ('.dram0.data','.dram0.bss'))
    binary=(build/'firmware.bin').stat().st_size
    dram_limit=100000 if 'wroom' in environment else 112000
    report=dict(environment=environment,applicationBytes=binary,freeIram=0x20000-iram,staticDram=dram,staticDramLimit=dram_limit)
    if binary>0x640000: raise ValueError(f'{environment}: OTA slot overflow')
    if report['freeIram']<512: raise ValueError(f'{environment}: IRAM reserve under 512 bytes: {report}')
    if dram>dram_limit: raise ValueError(f'{environment}: static internal RAM exceeds {dram_limit} bytes; protect radio heap')
    return report
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('environments',nargs='+');parser.add_argument('--out',type=Path);args=parser.parse_args()
    reports=[check(e) for e in args.environments];text=json.dumps(reports,indent=2);print(text)
    if args.out: args.out.write_text(text+'\n',encoding='utf-8')
