#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include "global.h"

#ifdef AUDIO_8BIT
typedef uint8_t  audio_sample;
#else
typedef uint16_t  audio_sample;
#endif

#ifdef AUDIO_IRQ
#define SOUNDRATE 22050                           // sound rate [Hz]
#else
#define SOUNDRATE 31500                           // sound rate = VGALINE [Hz]
#endif

extern void pwm_audio_init(int buffersize, void (*callback)(audio_sample * stream, int len));
extern void pwm_audio_handle_sample(void);
extern void pwm_audio_handle_buffer(void);
extern void pwm_audio_reset(void);

#endif
