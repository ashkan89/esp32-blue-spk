"""Exercise the updater's actual TLS admission probe with fragmented heaps."""
import os
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / 'src/management.cpp').read_text(encoding='utf-8')
probe = source[source.index('bool githubTlsMemoryAvailable()'):source.index('// ------------------------------------------------------------------ job -----')]
out = root / '.pio/native-tests'
out.mkdir(parents=True, exist_ok=True)
test = out / 'update_memory.cpp'
test.write_text(r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 16384
static std::vector<size_t> blocks;
struct Allocation { size_t block, bytes; };
static unsigned live, calls, failAt;
static char message[128];
size_t heap_caps_get_free_size(uint32_t caps) {
  assert(caps == 3);
  size_t total = 0;
  for (auto n : blocks) total += n;
  return total;
}
size_t heap_caps_get_largest_free_block(uint32_t caps) {
  assert(caps == 3);
  return *std::max_element(blocks.begin(), blocks.end());
}
void *heap_caps_malloc(size_t bytes, uint32_t caps) {
  assert(caps == 3);
  if (++calls == failAt) return nullptr;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (blocks[i] >= bytes) {
      blocks[i] -= bytes;
      ++live;
      return new Allocation{i, bytes};
    }
  }
  return nullptr;
}
void heap_caps_free(void *p) {
  if (!p) return;
  auto *a = static_cast<Allocation *>(p);
  blocks[a->block] += a->bytes;
  --live;
  delete a;
}
void updateSet(const char *phase, const char *text, bool busy) {
  assert(!strcmp(phase, "error") && !busy);
  snprintf(message, sizeof(message), "%s", text);
}
''' + probe + r'''
void check(std::vector<size_t> heap, bool expected, unsigned fail = 0) {
  blocks = heap;
  calls = 0;
  failAt = fail;
  message[0] = 0;
  assert(githubTlsMemoryAvailable() == expected);
  assert(live == 0 && blocks == heap);
  if (!expected) assert(strstr(message, "upload firmware locally."));
  else assert(!message[0]);
}
int main() {
  // Similar to the reported heap, after reserving the worker's 16 KB stack.
  // No 45 KB block exists, but both record buffers and headroom fit.
  check({34804, 30000, 21940}, true);
  // High total free memory cannot compensate for unusably small holes.
  check({17000, 17000, 17000, 17000, 17000, 17000}, false);
  // The first buffer fits, the second does not; release the first on failure.
  check({34804, 10000, 10000, 10000, 10000, 10000}, false);
  check({60000}, false); // No certificate/network reserve.
  check({34804, 30000, 21940}, false, 1);
  check({34804, 30000, 21940}, false, 2);
  check({34804, 30000, 21940}, true); // Retry after failure leaks nothing.
  puts("Update TLS memory: fragmentation, reserve, failures and cleanup passed");
}
''', encoding='utf-8')
exe = out / ('update_memory.exe' if os.name == 'nt' else 'update_memory')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', str(test), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
