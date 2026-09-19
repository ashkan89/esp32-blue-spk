"""Locate the project's PlatformIO packages without a workstation-only path."""
import os
from pathlib import Path

def packages():
    roots=[]
    if os.environ.get('PLATFORMIO_CORE_DIR'): roots.append(Path(os.environ['PLATFORMIO_CORE_DIR']))
    roots.extend([Path('C:/p'),Path.home()/'.platformio'])
    for root in roots:
        if (root/'packages'/'toolchain-xtensa-esp-elf').is_dir(): return root/'packages'
    raise FileNotFoundError('Build firmware first, or set PLATFORMIO_CORE_DIR to the installed PlatformIO core')

def tool(name):
    suffix='.exe' if os.name=='nt' else ''
    return packages()/'toolchain-xtensa-esp-elf'/'bin'/('xtensa-esp-elf-'+name+suffix)
