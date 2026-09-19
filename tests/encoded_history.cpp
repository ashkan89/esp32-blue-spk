#include "encoded_history.h"
#include <cassert>
#include <vector>
#include <cstdio>

int main() {
  // MPEG1 Layer III, 128 kbps / 44.1 kHz, 417-byte frames.
  std::vector<uint8_t> frames(417 * 20, 0);
  for (unsigned i = 0; i < 20; ++i) {
    frames[i*417]=0xff; frames[i*417+1]=0xfb; frames[i*417+2]=0x90;
    frames[i*417+4]=(uint8_t)i;
  }
  uint8_t memory[417*8], output[500];
  EncodedHistory history; history.begin(memory,sizeof(memory));
  history.append(frames.data(),417*5);
  assert(history.available()==417*5);
  assert(history.read(output,417)==417 && output[4]==0);
  assert(history.seek(420));
  assert(history.cursor()==834);
  history.read(output,417); assert(output[4]==2);
  history.append(frames.data()+417*5,417*10);
  assert(history.retained()==sizeof(memory));
  assert(history.oldest()==417*7);
  assert(history.overruns()==1);
  assert(history.cursor()>=history.oldest());
  assert(history.seek(0));
  history.read(output,417); assert(output[4]==7);
  assert(history.seek(history.end()-417*2));
  history.read(output,417); assert(output[4]==13);
  history.begin(nullptr,0); assert(!history.active());
  std::puts("Encoded history: wrap, retention, overrun, bounds and MP3 frame alignment passed");
}
