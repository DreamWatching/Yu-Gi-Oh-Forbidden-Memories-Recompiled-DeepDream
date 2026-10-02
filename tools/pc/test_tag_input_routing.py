"""Compile actual Tag pad routing and native pad backup/restore across all control modes."""
from pathlib import Path
import subprocess,shutil,re
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'tmp/pc/tag-input-routing-test';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'examples/mods/tag-duel-menu/tag_duel_runtime.c').read_text();m=re.search(r'static void side_input\(void\)\s*\{',s);start=m.start();end=m.end();depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
backup=(ROOT/'src/game/input_pad1_backup.c').read_text();backup=backup[backup.index('u16 gInput_wPad1Held;'):]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include "game/input.h"
static u16 gInput_wPad2Held,gInput_wPad2Pressed,gInput_wPad2Repeat;
enum{TAG_CONTROL_AI,TAG_CONTROL_PLAYER_ONE,TAG_CONTROL_PLAYER_TWO};
static struct {int active,active_member[2],partner_control;} state,*tag=&state;
static int D_8009B1D5,seen_h,seen_p,seen_r,calls;
static void observe(void){seen_h=gInput_wPad1Held;seen_p=gInput_wPad1Pressed;seen_r=gInput_wPad1Repeat;calls++;}
static void DuelScene_Update(void){observe();}
static void *original_side_input=(void*)observe;
'''
# Header declares extern pads; definitions must match their linkage.
prefix=prefix.replace('static u16 gInput_wPad2','u16 gInput_wPad2')
tests=r'''
int main(void){
 for(int control=0;control<3;control++)for(int side=0;side<2;side++)for(int member=0;member<2;member++)for(int active=0;active<2;active++){
  state.active=active;state.partner_control=control;state.active_member[0]=member;D_8009B1D5=side;
  gInput_wPad1Held=0x40;gInput_wPad1Pressed=0x2000;gInput_wPad1Repeat=0x10;
  gInput_wPad2Held=0x80;gInput_wPad2Pressed=0x8000;gInput_wPad2Repeat=0x20;calls=0;
  side_input();int second=active&&side==0&&member==1&&control==TAG_CONTROL_PLAYER_TWO;
  assert(calls==1&&seen_h==(second?0x80:0x40)&&seen_p==(second?0x8000:0x2000)&&seen_r==(second?0x20:0x10));
  assert(gInput_wPad1Held==0x40&&gInput_wPad1Pressed==0x2000&&gInput_wPad1Repeat==0x10);
  assert(gInput_wPad2Held==0x80&&gInput_wPad2Pressed==0x8000&&gInput_wPad2Repeat==0x20);
 }
 puts("PASS: 24 active/inactive/side/member/control combinations route correct held/pressed/repeat words and restore both pads");return 0;
}
'''
c=OUT/'test.c';c.write_text(prefix+backup+s[start:end]+tests);exe=OUT/'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2','-I'+str(ROOT/'src'),str(c),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
