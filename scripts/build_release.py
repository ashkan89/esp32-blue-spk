"""Build and sign both boards locally. Never uploads a release or flashes a unit."""
import argparse,base64,binascii,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec
from release_package import package,verify
from check_budgets import check
ROOT=Path(__file__).resolve().parent.parent

def load_signing_key(raw):
    """Load an unencrypted PEM/DER key, repairing common secret encodings."""
    raw=raw.strip().lstrip(b'\xef\xbb\xbf')
    candidates=[raw]
    if b'\\n' in raw:
        candidates.append(raw.replace(b'\\r\\n',b'\n').replace(b'\\n',b'\n'))
    # A frequent Actions setup mistake is to Base64-encode an already encoded
    # secret. Accept one extra layer, but never print any candidate key bytes.
    try:
        text=re.sub(rb'\s+',b'',raw)
        if text and re.fullmatch(rb'[A-Za-z0-9+/]*={0,2}',text):
            candidates.append(base64.b64decode(text,validate=True))
    except (binascii.Error,ValueError):
        pass
    for candidate in candidates:
        for loader in (serialization.load_pem_private_key,
                       serialization.load_der_private_key):
            try:
                key=loader(candidate,password=None)
                if not isinstance(key,ec.EllipticCurvePrivateKey) or not isinstance(key.curve,ec.SECP256R1):
                    raise ValueError('Signing key must be an ECDSA P-256 private key')
                return key
            except (TypeError,ValueError):
                pass
    raise ValueError(
        'Signing key is not a valid unencrypted PEM or DER private key. '
        'Store the PEM text directly, or Base64-encode the PEM file bytes.')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--key',type=Path,required=True)
    parser.add_argument('--out',type=Path,default=ROOT/'dist');parser.add_argument('--skip-build',action='store_true')
    parser.add_argument('--version',help='semantic version embedded in and assigned to both packages')
    parser.add_argument('--validate-key-only',action='store_true',help='validate the private key and exit')
    args=parser.parse_args()
    if args.key.resolve().is_relative_to(ROOT): parser.error('Production signing keys must stay outside the repository')
    try:key=load_signing_key(args.key.read_bytes())
    except ValueError as error:parser.error(str(error))
    header=(ROOT/'src/signing_public_key.h').read_text()
    public=json.loads(re.search(r'=\s*(".*?");',header,re.S).group(1)).encode()
    expected=serialization.load_pem_public_key(public)
    if key.public_key().public_numbers()!=expected.public_numbers(): parser.error('Signing key does not match the device public key')
    if args.validate_key_only:
        print('Signing key is valid and matches the firmware public key')
        return
    configured=re.search(r'#define FW_VERSION\s+"([^"]+)"',(ROOT/'src/app_config.h').read_text()).group(1)
    version=args.version or configured
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?',version):
        parser.error('Version must be semantic (for example 4.0.1)')
    targets={'wroom':'esp32_wroom_32d_16mb_release','wrover':'esp32_wrover_e_n16r8_release'}
    if not args.skip_build:
        build_env=os.environ.copy()
        if args.version:
            flag=f'-DFW_VERSION=\\"{version}\\"'
            build_env['PLATFORMIO_BUILD_FLAGS']=' '.join(
                value for value in (build_env.get('PLATFORMIO_BUILD_FLAGS',''),flag) if value)
        subprocess.run([sys.executable,'-m','platformio','run','-j','3',*[item for env in targets.values() for item in ('-e',env)]],cwd=ROOT,env=build_env,check=True)
    args.out.mkdir(parents=True,exist_ok=True);manifest={'version':version,'files':{},'budgets':[]}
    for board,environment in targets.items():
        manifest['budgets'].append(check(environment));build=ROOT/'.pio/build'/environment
        data=package((build/'firmware.bin').read_bytes(),board,version,key);verify(data,board,expected)
        (args.out/f'firmware-{board}.spk').write_bytes(data)
        for name in ('firmware.bin','firmware.elf','partitions.bin','bootloader.bin'):
            if (build/name).exists(): shutil.copy2(build/name,args.out/(board+'-'+name))
    shutil.copy2(ROOT/'platformio.ini',args.out/'platformio.ini')
    for file in sorted(args.out.iterdir()):
        if file.is_file() and file.name!='manifest.json':manifest['files'][file.name]=hashlib.sha256(file.read_bytes()).hexdigest()
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(f'Verified signed packages and matching debug artifacts: {args.out}')
if __name__=='__main__':main()
