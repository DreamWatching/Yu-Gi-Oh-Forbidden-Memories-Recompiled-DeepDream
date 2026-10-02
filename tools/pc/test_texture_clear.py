"""Compare actual HD texture clearing against the original wrapped per-word loop."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/pc/texture-clear-test'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/pc/render/texture_dump.c').read_text()
start = source.index('void TextureDump_Cleared(')
brace = source.index('{', start)
depth, end = 1, brace + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SOFT_GPU_WIDTH 1024
#define SOFT_GPU_HEIGHT 512
static uint32_t tags[1024*512], expected_tags[1024*512];
static uint16_t shadow[4096*512], expected_shadow[4096*512];
static uint32_t *TextureDump_Tags=tags;
static uint16_t *TextureDump_Shadow=shadow;
static int calls, args[4];
static void record(int x,int y,int w,int h) {calls++; args[0]=x;args[1]=y;args[2]=w;args[3]=h;}
static void (*TextureDump_Forget)(int,int,int,int)=record;
static uint32_t *tag_at(int x,int y) {return tags+(y&511)*1024+(x&1023);}
static uint16_t *TextureDump_Cell(int x,int y,int sub) {return shadow+(y&511)*4096+(x&1023)*4+sub;}
'''
tests = r'''
int main(void) {
 int test,i,j,x,y,w,h,has_shadow;
 for(test=0;test<80;test++) {
  x=test==0?1000:rand()%4096-2048; y=rand()%1024-512;
  w=test==0?2049:rand()%1300; h=rand()%550;
  has_shadow=test%2; TextureDump_Shadow=has_shadow?shadow:NULL;
  memset(tags,0x5a,sizeof(tags));memcpy(expected_tags,tags,sizeof(tags));
  memset(shadow,0xc3,sizeof(shadow));memcpy(expected_shadow,shadow,sizeof(shadow));
  for(j=0;j<h;j++) for(i=0;i<w;i++) {
   size_t at=((y+j)&511)*1024+((x+i)&1023);
   expected_tags[at]=0;
   if(has_shadow) memset(expected_shadow+at*4,0,8);
  }
  calls=0;TextureDump_Cleared(x,y,w,h);
  assert(!memcmp(tags,expected_tags,sizeof(tags)));
  assert(!memcmp(shadow,expected_shadow,sizeof(shadow)));
  assert(calls==1 && args[0]==x && args[1]==y && args[2]==w && args[3]==h);
 }
 TextureDump_Tags=NULL; calls=0;TextureDump_Cleared(1,2,3,4);assert(!calls);
 puts("PASS: HD texture row clears match original loop for wrapping, negative coordinates, absent shadow and callback");
 return 0;
}
'''
c = out / 'test.c'
c.write_text(prefix + source[start:end] + tests)
exe = out / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '--target=i686-w64-mingw32',
                '-std=gnu11', '-O2', str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
