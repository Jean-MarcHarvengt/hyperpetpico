#ifndef HDMI_H
#define HDMI_H

#include "stdint.h"
#include "global.h"


#ifdef HAS_AUDIO

#ifdef AUDIO_8BIT
typedef uint8_t  audio_sample;
#else
typedef uint16_t  audio_sample;
#endif

#define SOUNDRATE 31500			// sound rate = VGALINE [Hz] 525x60
#endif

extern void HdmiCore(void);
extern void HdmiVSync(void);
extern void Core1Exec(void (*fnc)());
extern void HdmiInit(int mode);
extern bool HdmiIsVSync(void);
//extern int HdmiVCount(void);
#ifdef HAS_AUDIO
extern void HdmiInitAudio(int samplesize, void (*callback)(audio_sample * stream, int len));
extern void HdmiHandleAudio(void);
extern void HdmiResetAudio(void);
#endif
#endif
