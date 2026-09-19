"""Exercise the firmware's actual HSV and gamma/brightness functions."""
import os, subprocess, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
source = (root/'src/leds.cpp').read_text(encoding='utf-8')
colors = source[source.index('static inline uint32_t rgb'):source.index('/// The hue of a picked colour')]
gamma = source[source.index('static uint8_t gamma_lut'):source.index('// ------------------------------------------------------------------- ring')]
out = root/'.pio/native-tests'; out.mkdir(parents=True, exist_ok=True)
test = out/'led_output.cpp'
test.write_text('''#include <stdint.h>
#include <math.h>
#include <cassert>
#include <cstdio>
''' + colors + gamma + '''
int main() {
  build_gamma();
  assert(hsv(0,255,255)==0xff0000);
  assert(hsv(21846,255,255)==0x00ff00);
  assert(hsv(43692,255,255)==0x0000ff);
  assert(hsv(5461,255,255)==0xff7f00);
  assert(hsv(0,0,255)==0xffffff);
  for (unsigned v=0;v<256;++v) {
    assert(scaled_channel(v,0)==0);
    assert(scaled_channel(v,200.0f/255)<=200);
  }
  // A dim full-color scene must remain visible, including sunrise at 2/255.
  assert(scaled_channel(255,24.0f/255*200.0f/255)==19);
  assert(scaled_channel(255,2.0f/255*200.0f/255)>=1);
  assert(scaled_channel(255,160.0f/255*200.0f/255)==125);
  puts("LED output: hue sweep, dark, current ceiling and dim scenes passed");
}
''', encoding='utf-8')
exe=out/('led_output.exe' if os.name=='nt' else 'led_output')
subprocess.run([sys.executable,'-m','ziglang','c++',str(test),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
