#ifndef SCIV_SIMON_SPRITESHEET_H
#define SCIV_SIMON_SPRITESHEET_H
#include <stdint.h>
struct Ppu;
#ifdef __cplusplus
extern "C" {
#endif
void Cv4SimonSpritesheetBeginFrame(struct Ppu *ppu, uint8_t *ram);
#ifdef __cplusplus
}
#endif
#endif
