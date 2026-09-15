/* Pyxel-compatible, assembled Simon frame sheet.
 * The 608x1024 canvas is the MainSimon layer from SimonSheetWRAMFullFrames.
 * Its 38x64 layout maps repeated cells onto 448 source 16x16 tiles, which
 * exactly cover the game's 0xe000-byte decompressed Simon graphics bank. */
#include "simon_spritesheet.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>
#include "snes/ppu.h"
#include "third_party/stb_image.h"
#include "third_party/stb_image_write.h"
namespace {
constexpr unsigned RAM=0x10000,BYTES=0xe000,TILES=448,CELL=16,COLS=38,ROWS=64,W=608,H=1024;
constexpr char PNG[]="simon_full_frames.png",REFERENCE[]="simon_full_frames.reference.png",LAYOUT[]="assets/simon_full_frames.layout";
std::array<uint8_t,BYTES> original{},edited{};std::array<int16_t,COLS*ROWS> layout{};
std::array<uint16_t,16> palette{};bool captured=false,loaded=false,mapped=false;time_t modified=0;unsigned clock30=0;
std::array<uint8_t,TILES*CELL*CELL> reference_pixels{};std::array<int16_t,TILES*4> source_to_piece{};
uint8_t get(const uint8_t*t,unsigned x,unsigned y){unsigned b=7-x;return((t[y*2]>>b)&1)|(((t[y*2+1]>>b)&1)<<1)|(((t[16+y*2]>>b)&1)<<2)|(((t[17+y*2]>>b)&1)<<3);}
void put(uint8_t*t,unsigned x,unsigned y,uint8_t c){unsigned m=1u<<(7-x);for(unsigned p=0;p<4;p++){unsigned q=(p>=2?16:0)+y*2+(p&1);t[q]=(uint8_t)((t[q]&~m)|((c&(1u<<p))?m:0));}}
void color(uint16_t c,uint8_t*p){p[0]=(uint8_t)(((c&31)*255+15)/31);p[1]=(uint8_t)((((c>>5)&31)*255+15)/31);p[2]=(uint8_t)((((c>>10)&31)*255+15)/31);p[3]=255;}
unsigned nearest(const uint8_t*p){unsigned best=1,bd=~0u;for(unsigned i=1;i<16;i++){uint8_t q[4];color(palette[i],q);int r=p[0]-q[0],g=p[1]-q[1],b=p[2]-q[2];unsigned d=(unsigned)(r*r+g*g+b*b);if(d<bd)bd=d,best=i;}return best;}
unsigned source_8x8(unsigned tile,unsigned x,unsigned y){unsigned local=tile&63,base=(tile>>6)*256+(local&7)*8+(local>>3)*2;return base+(x>=8?1:0)+(y>=8?64:0);}
uint8_t source_pixel(unsigned tile,unsigned x,unsigned y){return get(&original[source_8x8(tile,x,y)*32],x&7,y&7);}
void target_pixel(unsigned tile,unsigned x,unsigned y,uint8_t c){put(&edited[source_8x8(tile,x,y)*32],x&7,y&7,c);}
bool load_layout(){FILE*f=std::fopen(LAYOUT,"rb");if(!f)return false;bool ok=std::fread(layout.data(),sizeof(int16_t),layout.size(),f)==layout.size();std::fclose(f);if(ok)for(int16_t v:layout)if(v<-1||v>=(int)TILES)return false;return ok;}
bool build_reference(){int w=0,h=0,n=0;uint8_t*px=stbi_load(REFERENCE,&w,&h,&n,4);if(!px||w!=(int)W||h!=(int)H){if(px)stbi_image_free(px);std::fprintf(stderr,"[Simon frames] import the Pyxel file first; missing %s\n",REFERENCE);return false;}std::array<int,TILES> first{};first.fill(-1);for(unsigned c=0;c<layout.size();c++)if(layout[c]>=0&&first[layout[c]]<0)first[layout[c]]=(int)c;for(unsigned a=0;a<TILES;a++){int c=first[a];if(c<0)continue;unsigned ox=(c%COLS)*CELL,oy=(c/COLS)*CELL;for(unsigned y=0;y<CELL;y++)for(unsigned x=0;x<CELL;x++){const uint8_t*q=&px[((oy+y)*W+ox+x)*4];reference_pixels[(a*CELL+y)*CELL+x]=(q[3]<128||(q[2]>q[0]*2&&q[2]>q[1]*2))?0:(uint8_t)nearest(q);}}stbi_image_free(px);source_to_piece.fill(-1);unsigned unmatched=0,variants=0;for(unsigned s=0;s<TILES*4;s++){unsigned best=0,bd=~0u;const uint8_t*src=&original[s*32];for(unsigned candidate=0;candidate<TILES*4;candidate++){unsigned art=candidate>>2,q=candidate&3,dx=(q&1)*8,dy=(q>>1)*8,d=0;for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++)d+=get(src,x,y)!=reference_pixels[(art*CELL+dy+y)*CELL+dx+x];if(d<bd)bd=d,best=candidate;if(!d)break;}if(bd<=32){source_to_piece[s]=(int16_t)best;variants+=bd>8;}else unmatched++;}std::fprintf(stderr,"[Simon frames] 8x8 DMA matching: %u palette variants accepted, %u/%u unmatched left native\n",variants,unmatched,TILES*4);return true;}
bool export_png(){std::vector<uint8_t> px(W*H*4);for(unsigned cell=0;cell<layout.size();cell++){int tile=layout[cell];if(tile<0)continue;unsigned ox=(cell%COLS)*CELL,oy=(cell/COLS)*CELL;for(unsigned y=0;y<CELL;y++)for(unsigned x=0;x<CELL;x++){unsigned c=source_pixel((unsigned)tile,x,y);uint8_t*out=&px[((oy+y)*W+ox+x)*4];color(palette[c],out);if(!c)out[3]=0;}}return stbi_write_png(PNG,W,H,4,px.data(),W*4)!=0;}
bool load_png(){int w=0,h=0,n=0;uint8_t*px=stbi_load(PNG,&w,&h,&n,4);if(!px||w!=(int)W||h!=(int)H){if(px)stbi_image_free(px);std::fprintf(stderr,"[Simon frames] %s must remain %ux%u RGBA\n",PNG,W,H);return false;}edited=original;const bool keyed=px[3]>=128;const uint8_t kr=px[0],kg=px[1],kb=px[2];std::array<int,TILES> first{};first.fill(-1);for(unsigned cell=0;cell<layout.size();cell++)if(layout[cell]>=0&&first[layout[cell]]<0)first[layout[cell]]=(int)cell;for(unsigned source=0;source<TILES*4;source++){int candidate=source_to_piece[source];if(candidate<0)continue;unsigned art=(unsigned)candidate>>2,q=(unsigned)candidate&3;int cell=first[art];if(cell<0)continue;unsigned ox=(cell%COLS)*CELL+(q&1)*8,oy=(cell/COLS)*CELL+(q>>1)*8;uint8_t*t=&edited[source*32];for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){const uint8_t*in=&px[((oy+y)*W+ox+x)*4];bool key=keyed&&in[0]==kr&&in[1]==kg&&in[2]==kb;put(t,x,y,(uint8_t)((in[3]<128||key)?0:nearest(in)));}}stbi_image_free(px);size_t changed=0;for(size_t i=0;i<BYTES;i++)changed+=edited[i]!=original[i];std::fprintf(stderr,"[Simon frames] loaded %s through exact 8x8 matches (%zu source bytes changed)\n",PNG,changed);return true;}
bool ready(const Ppu*p,unsigned*pal){unsigned visible=0;for(unsigned s=2;s<=11;s++){uint16_t pos=p->oam[s*2],tile=p->oam[s*2+1];unsigned y=pos>>8;if(y>16&&y<224){*pal=(tile>>9)&7;visible++;}}return visible>=4;}
}
extern "C" void Cv4SimonSpritesheetBeginFrame(Ppu*p,uint8_t*r){if(!p||!r||r[0x32]!=4)return;if(!mapped){mapped=load_layout();if(!mapped){std::fprintf(stderr,"[Simon frames] missing or invalid %s\n",LAYOUT);return;}}if(!captured){unsigned pal=0;if(!ready(p,&pal))return;memcpy(original.data(),r+RAM,BYTES);edited=original;std::copy_n(&p->cgram[128+pal*16],16,palette.begin());if(!build_reference())return;captured=true;struct stat st{};if(stat(PNG,&st)!=0&&export_png())std::fprintf(stderr,"[Simon frames] exported assembled %s (%ux%u)\n",PNG,W,H);}if(++clock30%30==0){struct stat st{};if(stat(PNG,&st)==0&&(!loaded||st.st_mtime!=modified)){modified=st.st_mtime;loaded=load_png();}}if(loaded)memcpy(r+RAM,edited.data(),BYTES);}
