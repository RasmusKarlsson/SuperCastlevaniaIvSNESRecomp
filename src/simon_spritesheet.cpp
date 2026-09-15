/* Editable assembled Simon body poses. Body pieces are identified as a compact
 * four-slot OAM group; the more widely spread whip/effect pieces stay native. */
#include "simon_spritesheet.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include "snes/ppu.h"
#include "third_party/stb_image.h"
#include "third_party/stb_image_write.h"
namespace {
enum{CELL=64,COLS=8,ROWS=16,W=COLS*CELL,H=ROWS*CELL,MAX=COLS*ROWS};
const char *PNG="simon_body_frames.png",*MAP="simon_body_frames.map";
const uint8_t sizes[8][2]={{8,16},{8,32},{8,64},{16,32},{16,64},{32,64},{16,32},{16,32}};
struct Part{int16_t x,y;uint16_t tile;uint8_t size,flip,pal;};struct Pose{Part p[4];uint8_t bx,by;};
std::vector<Pose> poses;std::vector<uint8_t> img(W*H*4);bool init=false,loaded=false;time_t stamp=0;unsigned tick=0;
void color(uint16_t c,uint8_t*q){q[0]=(c&31)*255/31;q[1]=((c>>5)&31)*255/31;q[2]=((c>>10)&31)*255/31;q[3]=255;}
unsigned near(const uint8_t*q,const uint16_t*p){unsigned z=1,bd=~0u;for(unsigned i=1;i<16;i++){uint8_t c[4];color(p[i],c);int r=q[0]-c[0],g=q[1]-c[1],b=q[2]-c[2];unsigned d=r*r+g*g+b*b;if(d<bd)bd=d,z=i;}return z;}
int ox(const Ppu*p,int s){int x=p->oam[s*2]&255;x|=((p->highOam[s>>2]>>((s&3)*2))&1)<<8;return x>=256?x-512:x;}
bool pose(const Ppu*p,Pose&o){int best=-1,bs=9999;for(int s=0;s<=124;s++){int x0=999,x1=-999,y0=999,y1=-999;bool ok=true;for(int i=0;i<4;i++){int y=p->oam[(s+i)*2]>>8,x=ox(p,s+i),hi=(p->highOam[(s+i)>>2]>>(((s+i)&3)*2))&3,z=sizes[PPU_objSize(p)][hi>>1];if(y<32||y>=224||z<16){ok=false;break;}x0=std::min(x0,x);x1=std::max(x1,x+z);y0=std::min(y0,y);y1=std::max(y1,y+z);}if(ok&&x1-x0<=64&&y1-y0<=64){int score=abs((x0+x1)/2-128)+abs((y0+y1)/2-160);if(score<bs)bs=score,best=s;}}if(best<0)return false;int mx=999,my=999;for(int i=0;i<4;i++){int s=best+i,y=p->oam[s*2]>>8;uint16_t a=p->oam[s*2+1];int hi=(p->highOam[s>>2]>>((s&3)*2))&3;o.p[i]={(int16_t)ox(p,s),(int16_t)y,(uint16_t)(a&0x1ff),sizes[PPU_objSize(p)][hi>>1],(uint8_t)(a>>14),(uint8_t)((a>>9)&7)};mx=std::min(mx,(int)o.p[i].x);my=std::min(my,y);}for(auto&q:o.p)q.x-=mx,q.y-=my;return true;}
bool same(const Pose&a,const Pose&b){return !memcmp(a.p,b.p,sizeof a.p);}
unsigned adr(const Ppu*p,const Part&q,int x,int y){unsigned base=(q.tile&0x100)?PPU_objTileAdr2(p):PPU_objTileAdr1(p);unsigned t=(((q.tile>>4)+(y>>3))<<4)|(((q.tile&15)+(x>>3))&15);return(base+t*16)&0x7fff;}
unsigned get(const Ppu*p,unsigned a,int x,int y){const uint16_t*t=&p->vram[(a+(y&7))&0x7fff];uint32_t v=t[0]|(uint32_t(t[8])<<16);unsigned b=7-(x&7);return((v>>b)&1)|((v>>(b+7))&2)|((v>>(b+14))&4)|((v>>(b+21))&8);}
void put(Ppu*p,unsigned a,int x,int y,unsigned c){unsigned r=(a+(y&7))&0x7fff,b=7-(x&7);uint16_t l=p->vram[r],h=p->vram[(r+8)&0x7fff];l=(l&~(0x101u<<b))|((c&1)<<b)|(((c>>1)&1)<<(b+8));h=(h&~(0x101u<<b))|(((c>>2)&1)<<b)|(((c>>3)&1)<<(b+8));p->vram[r]=l;p->vram[(r+8)&0x7fff]=h;}
void origin(int n,int&x,int&y){x=n%COLS*CELL;y=n/COLS*CELL;}
void capture(const Ppu*p,Pose&o,int n){int bx,by;origin(n,bx,by);for(int y=0;y<CELL;y++)memset(&img[((by+y)*W+bx)*4],0,CELL*4);for(auto&q:o.p){auto pal=&p->cgram[128+q.pal*16];for(int y=0;y<q.size;y++)for(int x=0;x<q.size;x++){int sx=(q.flip&1)?q.size-1-x:x,sy=(q.flip&2)?q.size-1-y:y,c=get(p,adr(p,q,sx,sy),sx,sy),dx=bx+q.x+x,dy=by+q.y+y;if(c&&dx>=bx&&dx<bx+CELL&&dy>=by&&dy<by+CELL)color(pal[c],&img[(dy*W+dx)*4]);}}int best=-1;for(int y=0;y<=CELL-48;y++)for(int x=0;x<=CELL-32;x++){int score=0;for(int yy=0;yy<48;yy++)for(int xx=0;xx<32;xx++)score+=img[((by+y+yy)*W+bx+x+xx)*4+3]!=0;if(score>best)best=score,o.bx=x,o.by=y;}for(int y=0;y<CELL;y++)for(int x=0;x<CELL;x++)if(x<o.bx||x>=o.bx+32||y<o.by||y>=o.by+48)memset(&img[((by+y)*W+bx+x)*4],0,4);}
void save(){stbi_write_png(PNG,W,H,4,img.data(),W*4);FILE*f=fopen(MAP,"wb");if(f){uint32_t h[2]={0x31464253,(uint32_t)poses.size()};fwrite(h,sizeof h,1,f);fwrite(poses.data(),sizeof(Pose),poses.size(),f);fclose(f);}struct stat s{};if(!stat(PNG,&s))stamp=s.st_mtime;fprintf(stderr,"[Simon frames] %zu complete body poses in %s\n",poses.size(),PNG);}
void load(){FILE*f=fopen(MAP,"rb");if(f){uint32_t h[2];if(fread(h,sizeof h,1,f)==1&&h[0]==0x31464253&&h[1]<=MAX){poses.resize(h[1]);if(fread(poses.data(),sizeof(Pose),h[1],f)!=h[1])poses.clear();}fclose(f);}int w,h,n;uint8_t*p=stbi_load(PNG,&w,&h,&n,4);if(p&&w==W&&h==H)memcpy(img.data(),p,img.size()),loaded=true;if(p)stbi_image_free(p);struct stat s{};if(!stat(PNG,&s))stamp=s.st_mtime;}
void reload(){struct stat s{};if(stat(PNG,&s)||s.st_mtime==stamp)return;stamp=s.st_mtime;int w,h,n;uint8_t*p=stbi_load(PNG,&w,&h,&n,4);if(p&&w==W&&h==H)memcpy(img.data(),p,img.size()),loaded=true,fprintf(stderr,"[Simon frames] reloaded edited atlas\n");if(p)stbi_image_free(p);}
void apply(Ppu*p,const Pose&o,int n){int bx,by;origin(n,bx,by);for(auto&q:o.p){auto pal=&p->cgram[128+q.pal*16];for(int y=0;y<q.size;y++)for(int x=0;x<q.size;x++){if(q.x+x<o.bx||q.x+x>=o.bx+32||q.y+y<o.by||q.y+y>=o.by+48)continue;int dx=bx+q.x+x,dy=by+q.y+y;if(dx<bx||dx>=bx+CELL||dy<by||dy>=by+CELL)continue;auto z=&img[(dy*W+dx)*4];unsigned c=z[3]<128?0:near(z,pal);int sx=(q.flip&1)?q.size-1-x:x,sy=(q.flip&2)?q.size-1-y:y;put(p,adr(p,q,sx,sy),sx,sy,c);}}}
}
extern "C" void Cv4SimonSpritesheetBeginFrame(Ppu*p,uint8_t*r){if(!p||!r||r[0x32]!=4)return;if(!init)load(),init=true;if(++tick%30==0)reload();Pose o{};if(!pose(p,o))return;int n=-1;for(size_t i=0;i<poses.size();i++)if(same(poses[i],o)){n=(int)i;break;}if(n<0&&poses.size()<MAX){n=(int)poses.size();capture(p,o,n);poses.push_back(o);save();loaded=true;}if(n>=0&&loaded)apply(p,poses[n],n);}
