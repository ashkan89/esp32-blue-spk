"""Compile and run the real C++ audio/history implementations on the host."""
import os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parent.parent
out=root/'.pio'/'native-tests';out.mkdir(parents=True,exist_ok=True)
from test_arabic_shaping import V
literal=lambda text:'"'+''.join('\\x%02x'%b for b in text.encode('utf-8'))+'"'
cases=['#include "text_arabic.h"', '#include <cassert>', '#include <cstring>', '#include <cstdio>', 'int main(){char out[512];']
for logical,visual,_ in V:
    cases.append('text_arabic_visual(%s,out,sizeof(out));assert(!strcmp(out,%s));'%(literal(logical),literal(''.join(map(chr,visual)))))
cases.extend(['char tiny[3]={1,1,1};text_arabic_visual("abc",tiny,2);assert(tiny[1]==0&&tiny[2]==1);', 'std::puts("Arabic: real C++ shaping passed all reference vectors and buffer bounds");}'])
arabic=out/'text_arabic_cases.cpp';arabic.write_text('\n'.join(cases),encoding='utf-8')
for name,sources,flags in [
    ('encoded_history',['tests/encoded_history.cpp'],[]),
    ('text_arabic',[str(arabic),'src/text_arabic.cpp'],[]),
    ('audio_eq',['tests/audio_eq.cpp','src/audio_eq.cpp'],['-DSERIAL_LOG=0','-Itests/host']),
]:
    executable=out/(name+('.exe' if os.name=='nt' else ''))
    subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-Isrc',*flags,*sources,'-o',str(executable)],cwd=root,check=True)
    subprocess.run([str(executable)],cwd=root,check=True)
