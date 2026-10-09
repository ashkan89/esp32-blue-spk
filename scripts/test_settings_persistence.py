"""Run the actual deferred settings functions with a counting NVS substitute."""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
source = (root / 'src/management.cpp').read_text(encoding='utf-8')


def function(name):
    match = re.search(r'^void ' + re.escape(name) + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = match.start()
    brace = match.end() - 1
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


functions = '\n'.join(function(name) for name in (
    'saveAudioSettings', 'saveLedSettings', 'serviceSettingsPersistence',
    'flushSettingsPersistence', 'management_flush_settings',
    'management_store_leds', 'scheduleReboot'))
loop = source[source.index('void management_loop()'):]
assert loop.index('serviceSettingsPersistence();') < loop.index('if (!radio_mode_has_wifi')
assert 'ledsDirty = audioDirty = false;\n  prefs.end();' in source
power = (root / 'src/power.cpp').read_text(encoding='utf-8')
assert power.index('management_flush_settings();') < power.index('WiFi.disconnect(true);')
out = root / '.pio/native-tests'
out.mkdir(parents=True, exist_ok=True)
test = out / 'settings_persistence.cpp'
test.write_text((root / 'tests/settings_persistence.cpp').read_text(encoding='utf-8')
                .replace('// FIRMWARE_FUNCTIONS', functions), encoding='utf-8')
executable = out / ('settings_persistence.exe' if os.name == 'nt' else 'settings_persistence')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', str(test),
                '-o', str(executable)], cwd=root, check=True)
subprocess.run([str(executable)], cwd=root, check=True)
