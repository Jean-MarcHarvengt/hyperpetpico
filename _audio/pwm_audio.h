#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include "stdint.h"
#include "global.h"

#ifdef AUDIO_8BIT
typedef uint8_t  audio_sample;
#else
typedef uint16_t  audio_sample;
#endif

#ifdef AUDIO_CBACK
#define SOUNDRATE 15750                           // sound rate = VGALINE [Hz] 525x60/2
#else
#define SOUNDRATE 22050                           // sound rate [Hz]
#endif

extern void pwm_audio_init(int buffersize, void (*callback)(audio_sample * stream, int len));
extern void pwm_audio_handle_buffer(void);
extern void pwm_audio_reset(void);

#endif
