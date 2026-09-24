"""Test the real HTTP body reader and download/flash flow with fault injection."""
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
out = root / '.pio/native-tests'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/management.cpp').read_text(encoding='utf-8')
helpers = source[source.index('bool responseEncoding('):source.index('// ------------------------------------------------------------- metadata -----')]
transfer = source[source.index('bool flashFromStream('):source.index('// TLS needs two separate record buffers')]
startup = source[source.index('void servicePendingGithubJob()'):source.index('bool startGithubJob(bool install)')]
(out / 'update_transfer_impl.inc').write_text(helpers + transfer + startup, encoding='utf-8')
json_include = root / '.pio/libdeps/esp32_wroom_32d_16mb_release/ArduinoJson/src'
if not json_include.exists():
    raise SystemExit('Install the WROOM PlatformIO dependencies before this test')
exe = out / ('update_transport.exe' if os.name == 'nt' else 'update_transport')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17',
                '-I' + str(root / 'src'), '-I' + str(out), '-I' + str(json_include),
                str(root / 'tests/update_transport.cpp'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
