#include "audio_eq.h"
#include <array>
#include <cassert>
#include <cmath>
#include <thread>
#include <cstdio>
int main(){
  EqConfig cfg;audio_eq_defaults(&cfg);audio_eq_configure(cfg);
  std::array<int16_t,512> pcm{};
  for(unsigned i=0;i<pcm.size();++i)pcm[i]=(int16_t)(i*53-13000);
  const auto original=pcm;audio_eq_process(pcm.data(),256);assert(pcm==original);
  cfg.preamp=-12;audio_eq_configure(cfg);pcm.fill(10000);audio_eq_process(pcm.data(),256);
  assert(pcm[0]==10000); // A setting change begins at the previous signal level.
  for(int i=0;i<5;++i){pcm.fill(10000);audio_eq_process(pcm.data(),256);}
  assert(pcm[500]>2400&&pcm[500]<2600);
  std::thread writer([]{
    for(unsigned n=0;n<3000;++n){EqConfig c;audio_eq_defaults(&c);c.gain[n%5]=(n%2)?12:-12;
      audio_eq_configure(c);audio_eq_set_sample_rate((n%3)==0?8000:(n%3)==1?44100:48000);}
  });
  for(unsigned n=0;n<3000;++n){
    for(unsigned i=0;i<pcm.size();++i)pcm[i]=(int16_t)(std::sin((n*512+i)*0.07)*20000);
    audio_eq_process(pcm.data(),256);
    assert(std::isfinite(audio_eq_headroom_db()));
  }
  writer.join();std::puts("EQ: bypass, gain transition, sample-rate changes and concurrent publication passed");
}
