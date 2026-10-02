"""Exercise the actual LP crop recognition and OpenGL name placement logic."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/pc/wide-lp-hud-test'
out.mkdir(parents=True, exist_ok=True)
hd = (root / 'src/pc/text/hd_text.c').read_text()
# Isolate the LP branch; unrelated card labels and digit sheets are not mocked.
start = hd.index('int HdText_Hud(int ')
end = hd.index('    make_labels();', start)
branch = hd[start:end] + '    return 0;\n}\n'
gl = (root / 'src/pc/render/gl_picture.c').read_text()
start = gl.index('static void name_over_panel(')
brace = gl.index('{', start)
depth, end = 1, brace + 1
while depth:
    depth += (gl[end] == '{') - (gl[end] == '}')
    end += 1
name_function = gl[start:end]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define MAX_FACTOR 8
#define PANEL_PAGE_X 704
#define PANEL_PAGE_Y 0
#define PANEL_CLUT_X 736
#define PANEL_CLUT_X_TURN 752
#define PANEL_CLUT_Y 252
#define PANEL_U 128
#define PANEL_V 128
#define PANEL_W 64
#define PANEL_SPLIT 32
#define PANEL_H 40
#define PANEL_SUM 0x14e82665u
#define HUD_TOP 10
#define CELL 16
static uint16_t words[1];
static uint32_t checksum=PANEL_SUM;
static int factor=4, panel_ok=1, opponent_name=1, scale=4, draws;
static unsigned generation=1,panel_made=1;
static struct {int bank,depth,page_x,page_y,clut_x,clut_y,pack;} state={0,0,704,0,736,252,0};
typedef struct {int x,y,u,v;} Vertex;
static const uint16_t *SoftGpu_Vram(void) {return words;}
static uint32_t panel_sum(const uint16_t *unused) {return checksum;}
static int make_atlas(int wanted) {factor=wanted;return 1;}
static int make_panel(const uint16_t *unused) {return 1;}
static int HdText_NameBox(int s,int which,int *au,int *av,int *x,int *y,int *w,int *h) {
 *au=*av=0;*x=0;*y=which?22:10;*w=32;*h=8;return 1;
}
static int drawn_y[2];
static void block(int x,int y,int w,int h,int au,int av,int u1,int v1,const Vertex *b,int flags) {
 assert(draws<2); drawn_y[draws++]=y;
}
'''
tests = r'''
int main(void) {
 int au,av,palette; Vertex full={248,16,128,128}, half={280,16,160,128}, cropped={248,24,128,136};
 for(palette=736;palette<=752;palette+=16) {
  assert(HdText_Hud(0,704,0,palette,252,128,128,64,40,4,&au,&av));
  assert(au==0 && av==HUD_TOP*CELL);
  assert(HdText_Hud(0,704,0,palette,252,160,128,32,40,4,&au,&av));
  assert(au==32 && av==HUD_TOP*CELL);
  assert(!HdText_Hud(0,704,0,palette,252,128,136,64,24,4,&au,&av));
  state.clut_x=palette;draws=0;name_over_panel(&full,64,40,4);
  assert(draws==2 && drawn_y[0]==104 && drawn_y[1]==152);
  draws=0;name_over_panel(&half,32,40,4);assert(!draws);
  name_over_panel(&cropped,64,24,4);assert(!draws);
 }
 checksum=0;assert(!HdText_Hud(0,704,0,736,252,128,128,64,40,4,&au,&av));
 checksum=PANEL_SUM;
 assert(!HdText_Hud(0,704,0,736,252,128,128,65,40,4,&au,&av));
 assert(!HdText_Hud(0,704,0,720,252,128,128,64,40,4,&au,&av));
 puts("PASS: preview.2 full and widened right-half LP panels use native HD crops; names draw once; cropped/foreign sprites rejected");
 return 0;
}
'''
c = out / 'test.c'
c.write_text(prefix + branch + name_function + tests)
exe = out / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '--target=i686-w64-mingw32',
                '-std=gnu11', '-O2', str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
