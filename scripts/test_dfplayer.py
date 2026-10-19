"""Exercise the real reply parser against clone and delayed-folder replies."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
out = root / '.pio/native-tests/dfplayer'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/df_player.cpp').read_text(encoding='utf-8')
constants = source[source.index('constexpr uint8_t FRAME_START'):source.index('// ------------------------------------------------------------------ state')]
parser = source[source.index('bool frameValid('):source.index('// ------------------------------------------------------------------- pins')]
writer = source[source.index('void writeFrame('):source.index('/*\n * The reply parser.')]
header = (root / 'src/df_player.h').read_text(encoding='utf-8')
types = header[header.index('enum DfSource'):header.index('#if DFPLAYER_ENABLED')]
harness = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>
#define LOGF(...) ((void)0)
constexpr uint8_t DF_VOLUME_MAX=30;
uint32_t clockMs=100;
uint32_t millis() { return clockMs; }
struct Uart {
  std::deque<uint8_t> bytes;
  std::vector<uint8_t> sent;
  bool available() { return !bytes.empty(); }
  uint8_t read() { auto b=bytes.front(); bytes.pop_front(); return b; }
  void write(const uint8_t *p,size_t n) { sent.assign(p,p+n); }
  void flush() {}
} Serial2;
'''+types+constants+r'''
DfStatus status{};
bool sleeping=false, folderPending=false, scanComplete=false, resetAwaitingOnline=true;
bool requestCommandAck=true;
DfState liveState=DF_STOPPED;
uint8_t activeSource=DF_SRC_SD,scanAt=0,scanNext=0;
uint16_t folderCounts[DF_MAX_FOLDERS]{};
struct Locked { bool held=true; };
enum Op { OP_REFRESH };
struct Command { Op op; uint8_t cmd; uint16_t param; };
bool push(const Command &,uint32_t) { return true; }
void setError(const char *s) { snprintf(status.error,sizeof(status.error),"%s",s?s:""); }
void dropFolderIndex() { memset(folderCounts,0,sizeof(folderCounts)); scanAt=scanNext=0; folderPending=scanComplete=false; }
void handleFrame(uint8_t,uint16_t);
'''+writer+parser+r'''
void receive(uint8_t cmd,uint16_t param,bool checksum=true) {
  uint8_t f[10]={0x7e,0xff,6,cmd,0,(uint8_t)(param>>8),(uint8_t)param,0,0,0xef};
  uint16_t sum=0;for(int i=1;i<=6;i++)sum+=f[i];
  uint16_t c=(uint16_t)-sum;
  if(checksum) { f[7]=c>>8; f[8]=c; }
  for(auto b:f)Serial2.bytes.push_back(b);
  readSerial();
}
int main() {
  writeFrame(CMD_SOURCE,1);
  assert(Serial2.sent.size()==10&&Serial2.sent[4]==1&&frameValid(Serial2.sent.data()));
  writeFrame(Q_FILES_USB,0);
  assert(Serial2.sent[4]==0&&frameValid(Serial2.sent.data()));
  requestCommandAck=false;writeFrame(CMD_VOLUME,6);
  assert(Serial2.sent[4]==0&&frameValid(Serial2.sent.data()));
  receive(EV_ONLINE,0);assert(!status.sdPresent&&!status.usbPresent);
  assert(!resetAwaitingOnline);
  receive(EV_ONLINE,DEV_USB|DEV_SD);assert(status.sdPresent&&status.usbPresent);
  receive(EV_ONLINE,DEV_SD);assert(status.sdPresent&&!status.usbPresent);
  receive(Q_FILES_USB,126);assert(status.usbPresent&&status.totalTracks==0);
  receive(Q_FILES_SD,254);assert(status.totalTracks==254);
  // A parameter containing 0x7e is data, including checksum-free clone frames.
  receive(Q_FILES_SD,126,false);assert(status.totalTracks==126);
  receive(EV_REMOVED,DEV_SD);assert(!status.sdPresent&&status.totalTracks==0);
  Serial2.bytes={0x7e,0,0,0};
  receive(Q_VOLUME,7);assert(status.volume==7&&status.framesBad>0);
  receive(EV_ERROR,0x81);assert(status.errors==1&&status.lastErrorCode==0x81);
  assert(strstr(status.error,"0x0081"));
  // Unsolicited/late folder replies must not poison a new cache.
  scanAt=1;receive(Q_FOLDER_FILES,12);assert(folderCounts[0]==0);
  folderPending=true;scanNext=1;receive(Q_FOLDER_FILES,12);
  assert(folderCounts[0]==12&&scanNext==2&&!folderPending&&!scanComplete);
  receive(Q_FOLDER_FILES,99);assert(folderCounts[0]==12);
  scanAt=99;scanNext=99;folderPending=true;receive(Q_FOLDER_FILES,3);
  assert(folderCounts[98]==3&&scanComplete&&!scanNext);
  assert(status.folders==2);
  sleeping=true;status.state=DF_SLEEPING;receive(Q_STATUS,0x0201);
  assert(status.state==DF_SLEEPING);
  puts("DFPlayer: real parser/device masks/clone errors/folder reply ownership passed");
}
'''
cpp = out / 'dfplayer.cpp'
cpp.write_text(harness, encoding='utf-8')
exe = out / ('dfplayer.exe' if os.name == 'nt' else 'dfplayer')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', str(cpp), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
