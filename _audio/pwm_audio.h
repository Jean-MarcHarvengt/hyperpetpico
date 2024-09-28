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
#define SOUNDRATE 31500                           // sound rate = VGALINE [Hz]
#else
#define SOUNDRATE 22050                           // sound rate [Hz]
#endif

extern void pwm_audio_init(int buffersize, void (*callback)(audio_sample * stream, int len));
#ifdef AUDIO_CBACK
extern void pwm_audio_handle_sample(void);
#endif
extern void pwm_audio_handle_buffer(void);
extern void pwm_audio_reset(void);

#endif
