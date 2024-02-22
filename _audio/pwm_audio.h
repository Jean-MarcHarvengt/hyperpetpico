#ifndef PWM_AUDIO_H
#define PWM_AUDIO_H

#include "global.h"

#define SOUNDRATE 31500                           // sound rate [Hz]

#define PWMSND_TOP  255                           // PRM top (period = PWMSND_TOP + 1 = 256)
#define PWMSND_CLOCK  (SOUNDRATE*(PWMSND_TOP+1))  // PWM clock (= 22050*256 = 5644800)
#define PWMSND_GPIO AUDIO_PIN                     // PWM output GPIO pin
#define PWMSND_SLICE  ((PWMSND_GPIO>>1)&7)        // PWM slice index (=1)
#define PWMSND_CHAN (PWMSND_GPIO&1)               // PWM channel index (=1)

extern void pwm_audio_init(int buffersize, void (*callback)(short * stream, int len));
extern void pwm_audio_handle_sample(void);
extern void pwm_audio_handle_buffer(void);

#endif
