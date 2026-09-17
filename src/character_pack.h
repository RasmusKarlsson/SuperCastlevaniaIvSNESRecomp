#pragma once
#include <stdint.h>
struct Ppu;
#ifdef __cplusplus
extern "C" {
#endif
void Cv4CharacterInit(const uint8_t *rom, unsigned size);
void Cv4CharacterBeforeFrame(void);
void Cv4CharacterBegin(struct Ppu *ppu, const uint8_t *ram);
void Cv4CharacterEnd(struct Ppu *ppu);
void Cv4CharacterEditor(void);
#ifdef __cplusplus
}
#endif
