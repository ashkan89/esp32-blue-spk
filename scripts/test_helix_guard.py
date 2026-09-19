"""Fault-inject allocations in the guarded, pinned Helix headers."""
import os, shutil, subprocess, sys
from pathlib import Path
from helix_guard import patch_tree

root=Path(__file__).resolve().parent.parent
dependency=root/'.pio/libdeps/esp32_wroom_32d_16mb/libhelix/src'
out=root/'.pio/native-tests/helix';out.mkdir(parents=True,exist_ok=True)
shutil.copytree(dependency,out/'src',dirs_exist_ok=True)
patch_tree(out/'src')
test=out/'allocation.cpp'
test.write_text('''#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>
#include <cstdio>
static int calls, failAt, live;
static void *testMalloc(size_t size) {
  if (++calls==failAt) return nullptr;
  void *p=std::malloc(size);if(p)++live;return p;
}
static void testFree(void *p) {if(p)--live;std::free(p);}
static void *testCalloc(size_t count,size_t size) {
  void *p=testMalloc(count*size);if(p)std::memset(p,0,count*size);return p;
}
void *operator new[](size_t size,const std::nothrow_t &) noexcept {return testMalloc(size);}
void operator delete[](void *p) noexcept {testFree(p);}
#define malloc testMalloc
#define calloc testCalloc
#define free testFree
#define HELIX_LOGGING_ACTIVE 0
#define LOGE_HELIX(...)
#define LOGI_HELIX(...)
#define LOGD_HELIX(...)
#include "CommonHelix.h"
#undef malloc
#undef calloc
#undef free
class Decoder : public libhelix::CommonHelix {
  void *state=nullptr;
 public:
  ~Decoder(){end();}
  size_t maxFrameSize() override{return 2048;}
  size_t maxPCMSize() override{return 5120;}
  bool allocateDecoder() override {
    state=libhelix::DefaultAllocator.allocate(256);return state;
  }
  void end() override {
    libhelix::CommonHelix::end();testFree(state);state=nullptr;
  }
  int decode() override{return -1;}
  int findSynchWord(int=0) override{return -1;}
};
int main(){
  // Fail decoder state, encoded frame and PCM independently, then retry.
  for(int failure=1;failure<=3;++failure){
    calls=0;failAt=failure;
    {Decoder d;assert(!d.begin());assert(!bool(d));d.end();assert(live==0);
      calls=0;failAt=0;assert(d.begin());assert(bool(d));d.end();}
    assert(live==0);
  }
  puts("Helix: every startup allocation failure returns, cleans up and retries");
}
''',encoding='utf-8')
exe=out/('allocation.exe' if os.name=='nt' else 'allocation')
for allocator in (0,1):
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17',f'-DUSE_ALLOCATOR={allocator}','-I'+str(out/'src'),str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
