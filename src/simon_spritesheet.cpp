/* Editable ROM-derived Simon graphics. The first gameplay frame exports the
 * decompressed WRAM sheet. Edited PNG pixels are converted back to SNES 4bpp
 * before the retail animation DMA reads that graphics-only buffer. */
#include "simon_spritesheet.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include "snes/ppu.h"
#include "third_party/stb_image.h"
#include "third_party/stb_image_write.h"

namespace {
constexpr unsigned kRamOffset=0x10000, kSheetBytes=0xe000, kTileBytes=32;
constexpr unsigned kTiles=kSheetBytes/kTileBytes, kColumns=32;
constexpr unsigned kWidth=kColumns*8, kHeight=(kTiles/kColumns)*8;
constexpr char kFilename[]="simon_spritesheet.png";
std::array<uint8_t,kSheetBytes> original{}, edited{};
bool captured=false, loaded=false; time_t modified=0; unsigned clock30=0;

uint8_t GetPixel(const uint8_t*t,unsigned x,unsigned y){unsigned b=7-x;return((t[y*2]>>b)&1)|(((t[y*2+1]>>b)&1)<<1)|(((t[16+y*2]>>b)&1)<<2)|(((t[17+y*2]>>b)&1)<<3);}
void SetPixel(uint8_t*t,unsigned x,unsigned y,uint8_t c){unsigned m=1u<<(7-x);for(unsigned p=0;p<4;++p){unsigned q=(p>=2?16:0)+y*2+(p&1);t[q]=(uint8_t)((t[q]&~m)|((c&(1u<<p))?m:0));}}
void Color(uint16_t c,uint8_t*p){p[0]=(uint8_t)(((c&31)*255+15)/31);p[1]=(uint8_t)((((c>>5)&31)*255+15)/31);p[2]=(uint8_t)((((c>>10)&31)*255+15)/31);p[3]=255;}
unsigned Nearest(const uint8_t*p,const uint16_t*pal){unsigned best=1,bd=~0u;for(unsigned i=1;i<16;++i){uint8_t q[4];Color(pal[i],q);int r=p[0]-q[0],g=p[1]-q[1],b=p[2]-q[2];unsigned d=(unsigned)(r*r+g*g+b*b);if(d<bd)bd=d,best=i;}return best;}

bool Export(const Ppu*ppu){std::array<uint8_t,kWidth*kHeight*4> px{};const uint16_t*pal=&ppu->cgram[128];for(unsigned ti=0;ti<kTiles;++ti){unsigned ox=(ti%kColumns)*8,oy=(ti/kColumns)*8;const uint8_t*t=&original[ti*kTileBytes];for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){unsigned c=GetPixel(t,x,y);uint8_t*out=&px[((oy+y)*kWidth+ox+x)*4];Color(pal[c],out);if(!c)out[3]=0;}}return stbi_write_png(kFilename,kWidth,kHeight,4,px.data(),kWidth*4)!=0;}
bool Load(const Ppu*ppu){int w=0,h=0,n=0;uint8_t*px=stbi_load(kFilename,&w,&h,&n,4);if(!px||w!=(int)kWidth||h!=(int)kHeight){if(px)stbi_image_free(px);std::fprintf(stderr,"[Simon sheet] %s must remain %ux%u RGBA\n",kFilename,kWidth,kHeight);return false;}edited.fill(0);const uint16_t*pal=&ppu->cgram[128];for(unsigned ti=0;ti<kTiles;++ti){unsigned ox=(ti%kColumns)*8,oy=(ti/kColumns)*8;uint8_t*t=&edited[ti*kTileBytes];for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const uint8_t*in=&px[((oy+y)*kWidth+ox+x)*4];SetPixel(t,x,y,in[3]<128?0:(uint8_t)Nearest(in,pal));}}stbi_image_free(px);std::fprintf(stderr,"[Simon sheet] loaded %s; watching for edits\n",kFilename);return true;}
}

extern "C" void Cv4SimonSpritesheetBeginFrame(Ppu*ppu,uint8_t*ram){if(!ppu||!ram||ram[0x32]!=4)return;if(!captured){std::memcpy(original.data(),ram+kRamOffset,kSheetBytes);edited=original;captured=true;struct stat st{};if(stat(kFilename,&st)!=0&&Export(ppu))std::fprintf(stderr,"[Simon sheet] exported %s (%ux%u)\n",kFilename,kWidth,kHeight);}if(++clock30%30==0){struct stat st{};if(stat(kFilename,&st)==0&&(!loaded||st.st_mtime!=modified)){modified=st.st_mtime;loaded=Load(ppu);}}if(loaded)std::memcpy(ram+kRamOffset,edited.data(),kSheetBytes);}
