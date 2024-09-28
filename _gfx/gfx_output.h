#ifndef GFX_OUTPUT_H
#define GFX_OUTPUT_H

#include "stdint.h"

extern uint8_t video_mode;
extern uint16_t screen_height;
extern uint16_t screen_width;

extern void GfxCoreWithSound(void);
extern void GfxVideoSetup(void);
extern void GfxVideoInit(uint8_t mode, bool firstTime);
extern void GfxWaitVSync(void);
extern bool GfxIsVSync(void);

#endif
