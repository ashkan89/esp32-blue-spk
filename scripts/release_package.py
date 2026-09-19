"""Create and verify board-bound ECDSA firmware packages. Requires cryptography.

keygen --key <outside-repo.pem> --header src/signing_public_key.h
pack --key <private.pem> --board wroom|wrover --version 4.0.0 --image firmware.bin --out firmware-wroom.spk
verify --key <private-or-public.pem> --package firmware-wroom.spk --board wroom
"""
import argparse
import hashlib
import json
import struct
import re
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

PREFIX = struct.Struct('<8sIIII32s32s')
HEADER = struct.Struct('<8sIIII32s32sI80s')
BOARDS = {'wroom': 1, 'wrover': 2}

def validate_image(image, board, version):
    if len(image) < 288 or image[0] != 0xe9 or len(image) > 0x640000:
        raise ValueError('Expected an ESP32 application image fitting the OTA slot')
    # ESP app descriptor begins in the first segment, after the 24+8 byte headers.
    if image[32:36] != b'\x32\x54\xcd\xab':
        raise ValueError('Bootloader and merged factory images cannot be used for OTA')
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?', version):
        raise ValueError('Expected a semantic release version')
    identities = set(re.findall(rb'SPKAPP:([12]):([^:\x00]{1,31}):END', image))
    if identities != {(str(BOARDS[board]).encode(), version.encode())}:
        raise ValueError('Embedded application board/version does not match the release')

def package(image, board, version, key):
    validate_image(image, board, version)
    if not isinstance(key.curve, ec.SECP256R1):
        raise ValueError('Signing requires an ECDSA P-256 key')
    encoded = version.encode('ascii')
    if len(encoded) > 31:
        raise ValueError('Version is too long')
    prefix = PREFIX.pack(b'SPKFW001', 1, BOARDS[board], len(image), 1,
                         encoded, hashlib.sha256(image).digest())
    signature = key.sign(prefix, ec.ECDSA(hashes.SHA256()))
    return prefix + struct.pack('<I80s', len(signature), signature) + image

def verify(data, board, key):
    if len(data) < HEADER.size:
        raise ValueError('Truncated header')
    magic, schema, target, size, settings, version, digest, n, signature = HEADER.unpack_from(data)
    if magic != b'SPKFW001' or schema != 1 or settings != 1 or target != BOARDS[board] or not 1 <= n <= 80:
        raise ValueError('Invalid package schema or board')
    key.verify(signature[:n], data[:PREFIX.size], ec.ECDSA(hashes.SHA256()))
    image = data[HEADER.size:]
    if len(image) != size or hashlib.sha256(image).digest() != digest:
        raise ValueError('Corrupt or truncated application')
    validate_image(image, board, version.rstrip(b'\0').decode())
    return {'board': board, 'version': version.rstrip(b'\0').decode(), 'bytes': size,
            'sha256': digest.hex()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    k = sub.add_parser('keygen'); k.add_argument('--key', type=Path, required=True)
    k.add_argument('--header', type=Path, required=True)
    for action in ('pack', 'verify'):
        p = sub.add_parser(action); p.add_argument('--key', type=Path, required=True)
        p.add_argument('--board', choices=BOARDS, required=True)
        if action == 'pack':
            p.add_argument('--image', type=Path, required=True); p.add_argument('--out', type=Path, required=True)
            p.add_argument('--version', required=True)
        else:
            p.add_argument('--package', type=Path, required=True)
    args = parser.parse_args()
    if args.action == 'keygen':
        root = Path(__file__).resolve().parent.parent
        if args.key.resolve().is_relative_to(root):
            parser.error('Store the private key outside the repository')
        if args.key.exists():
            parser.error('Refusing to overwrite an existing signing key')
        key = ec.generate_private_key(ec.SECP256R1())
        args.key.parent.mkdir(parents=True, exist_ok=True)
        with args.key.open('xb') as f:
            f.write(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                      serialization.NoEncryption()))
        args.key.chmod(0o600)
        public = key.public_key().public_bytes(serialization.Encoding.PEM,
                                               serialization.PublicFormat.SubjectPublicKeyInfo).decode()
        args.header.write_text('#pragma once\nstatic const char SPEAKER_SIGNING_PUBLIC_KEY[] =\n' +
                               json.dumps(public) + ';\n', encoding='utf-8')
        print('Key created. Back up the private key securely; only the public header belongs in source control.')
    else:
        raw = args.key.read_bytes()
        if args.action == 'pack':
            key = serialization.load_pem_private_key(raw, password=None)
            data = package(args.image.read_bytes(), args.board, args.version, key)
            args.out.write_bytes(data)
            print(json.dumps(verify(data, args.board, key.public_key())))
        else:
            try: key = serialization.load_pem_public_key(raw)
            except ValueError: key = serialization.load_pem_private_key(raw, password=None).public_key()
            print(json.dumps(verify(args.package.read_bytes(), args.board, key)))

if __name__ == '__main__':
    main()
