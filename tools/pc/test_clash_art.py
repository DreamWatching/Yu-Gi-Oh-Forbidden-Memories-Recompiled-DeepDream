"""Test runtime atlas reconstruction against local disc texels; no redistributable pixels."""
from pathlib import Path
import shutil
import subprocess
import sys
import struct
from PIL import Image
from fm_editor.disc import DiscImage
from extract_images import decode, read_palette

ROOT = Path(__file__).resolve().parents[2]
MOD = ROOT / 'examples/mods/duel-options'
OUT = ROOT / 'tmp/pc/clash-art-test'
OUT.mkdir(parents=True, exist_ok=True)
disc_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'game/Yu-Gi-Oh Forbidden Memories USA.bin'
with DiscImage(disc_path) as disc:
    assert disc.find('SLUS_014.11'), 'USA disc required'
    wa, _ = disc.find('DATA/WA_MRG.MRG')
    raw = disc.read(wa + 0xb50000 // 2048, 0x51000)
(OUT / 'private-fixture.bin').write_bytes(raw)
c = OUT / 'test.c'
c.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "clash_art.h"
static int missing=1,reads;
static int start(const MemoriesModHost *h,const char *path){return missing ? -1 : 100;}
static int read_sectors(const MemoriesModHost *h,int lba,int count,void *out){
 FILE *f=fopen("private-fixture.bin","rb");size_t n;
 assert(f);assert(lba==100+0xb50000/2048);reads++;
 n=fread(out,2048,count,f);fclose(f);return (int)n;
}
int main(void){
 MemoriesModHost h={0};FILE *out;int i;
 h.disc_file_start=start;h.disc_read=read_sectors;
 assert(!ClashArt_Load(&h));missing=0;assert(ClashArt_Load(&h));
 assert(ClashArt_Load(&h) && reads==1);
 out=fopen("private-art.argb","wb");assert(out);
 for(i=0;i<10;i++)fwrite(clash_backs[i],4,6400,out);
 for(i=0;i<10;i++)fwrite(clash_fronts[i],4,6400,out);
 fwrite(clash_crossed_swords,4,6400,out);fclose(out);return 0;
}
''')
exe = OUT / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '-std=gnu11', '-O2',
               '-I'+str(ROOT/'src'), '-I'+str(MOD), str(c), str(MOD/'clash_art.c'),
               '-o', str(exe)], check=True)
subprocess.run([str(exe)], cwd=OUT, check=True)
data = (OUT / 'private-art.argb').read_bytes()
images = []
for i in range(21):
    argb = struct.unpack_from('<6400I', data, i*25600)
    rgba = bytes(v for p in argb for v in ((p>>16)&255,(p>>8)&255,p&255,p>>24))
    images.append(Image.frombytes('RGBA',(80,80),rgba))
palette = read_palette(raw,0x33600,256)
w,h,rgba = decode(raw,0x23000,64,256,8,palette)
back = Image.frombytes('RGBA',(w,h),rgba).crop((58,130,106,190))
assert images[0].crop((16,10,64,70)).tobytes() == back.tobytes()
star_palette = read_palette(raw,0x10500,16)
w,h,rgba = decode(raw,0,64,256,4,star_palette)
stars = Image.frombytes('RGBA',(w,h),rgba)
positions=[(128,48),(144,48),(160,48),(176,48),(192,48),(208,48),(224,48),(240,48),(128,64),(144,64)]
for i,(sx,sy) in enumerate(positions):
    icon = stars.crop((sx,sy,sx+16,sy+16))
    for y in range(30):
        for x in range(30):
            pixel=icon.getpixel((x*16//30,y*16//30))
            if pixel[3]: assert images[10+i].getpixel((25+x,14+y)) == pixel
assert images[-1].getbbox() and images[-1].getbbox()[0]>=0
sheet = Image.new('RGBA',(7*100,3*100),(15,12,20,255))
for i,img in enumerate(images):sheet.alpha_composite(img,((i%7)*100+10,(i//7)*100+10))
sheet.save(OUT/'private-reconstruction.png')
print('PASS: original card back and all ten Guardian Star CLUTs/UVs match disc; missing-disc retry and one-time load')
print('Local-only preview:',OUT/'private-reconstruction.png')
