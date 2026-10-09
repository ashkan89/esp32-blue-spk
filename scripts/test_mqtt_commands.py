"""Run the firmware's actual MQTT callback and loop with fake peripherals."""
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / 'src/home_assistant.cpp').read_text(encoding='utf-8')
callback = source[source.index('void onMessage('):source.index('bool connectBroker()')]
loop = source[source.index('void ha_loop()'):source.index('void ha_announce()')]
light = source[source.index('bool applyLightCommand('):source.index('void onMessage(')]
discovery = source[source.index('void discoverLight()'):source.index('void discoverAlarm(')]
entities = source[source.index('struct Entity {'):source.index('/// The device block')]
table = source[source.index('void discoverTableEntity('):source.index('void discoverLight()')]
update = source[source.index('bool publishUpdate()'):source.index('bool publishState()')]
management = (root / 'src/management.cpp').read_text(encoding='utf-8')
store_audio = management[management.index('void management_store_audio()'):management.index('void management_store_leds()')]
harness = (root / 'tests/mqtt_commands.cpp').read_text(encoding='utf-8')
out = root / '.pio/native-tests'
out.mkdir(parents=True, exist_ok=True)
test = out / 'mqtt_commands.cpp'
test.write_text(harness.replace('// FIRMWARE_FUNCTIONS', store_audio + entities + table + light + discovery + update + callback + loop), encoding='utf-8')
executable = out / ('mqtt_commands.exe' if os.name == 'nt' else 'mqtt_commands')
json_headers = next((root / '.pio/libdeps').glob('esp32_w*/ArduinoJson/src'), None)
if json_headers is None:
    raise SystemExit('Build a PlatformIO environment first to install ArduinoJson.')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-Isrc',
                '-I' + str(json_headers), str(test), '-o', str(executable)], cwd=root, check=True)
subprocess.run([str(executable)], cwd=root, check=True)
