"""Capture the one-time provisioning record from a physically attached speaker.

Connect this before first boot/factory reset. Does not reset or flash the device.
Output includes its setup secret: keep labels private; never commit them.
"""
import argparse,json,re,time
from pathlib import Path
import serial
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--port',required=True)
parser.add_argument('--out',type=Path,required=True);parser.add_argument('--timeout',type=int,default=120);args=parser.parse_args()
if args.out.exists():parser.error('Refusing to overwrite an existing provisioning label')
root=Path(__file__).resolve().parent.parent
if args.out.resolve().is_relative_to(root):parser.error('Store provisioning labels outside the repository')
with serial.Serial() as connection:
    connection.port=args.port;connection.baudrate=115200;connection.timeout=1;connection.dtr=False;connection.rts=False;connection.open()
    deadline=time.monotonic()+args.timeout
    while time.monotonic()<deadline:
        line=connection.readline().decode('ascii','ignore').strip()
        match=re.search(r'PROVISION\s+(\S+)\s+([0-9a-fA-F]{16,64})',line)
        if not match:continue
        args.out.parent.mkdir(parents=True,exist_ok=True)
        with args.out.open('x',encoding='utf-8') as output:
            json.dump({'device':match[1],'username':'admin','setupPassword':match[2]},output,indent=2)
        args.out.chmod(0o600)
        print('Provisioning label captured. Keep this file private.');break
    else:raise SystemExit('No provisioning record received; device was not changed')
