#include "pwm_audio.h"

static void (*fillsamples)(short * stream, int len) = nullptr;
static short * snd_tx_buffer16;        // 16bits samples buffer
static uint16_t snd_nb_samples;        // in 16bits mono samples
static uint16_t snd_sample_ptr = 0;    // in 16bits samples pointer
static bool snd_last_position;         // playback was first half at last buffer fill


/********************************
 * Processing
********************************/ 
// process a single sample (from audio interrupt or timer or ..)
// should be called at SOUNDRATE
void pwm_audio_handle_sample(void)
{
  pwm_set_gpio_level(AUDIO_PIN, ((long)snd_tx_buffer16[snd_sample_ptr++]+32767)>>8);
  snd_sample_ptr %= snd_nb_samples;  
}

#ifdef AUDIO_IRQ
static void AUDIO_isr() {
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  pwm_audio_handle_sample();
}
#endif

// fill half buffer depending on current position
void pwm_audio_handle_buffer(void)
{
    bool current_pos = (snd_sample_ptr >= (snd_nb_samples/2))?true:false;
    if (current_pos != snd_last_position) {
      if (current_pos ) {
        fillsamples(snd_tx_buffer16, snd_nb_samples/2);
      }  
      else { 
        fillsamples(&snd_tx_buffer16[snd_nb_samples/2], snd_nb_samples/2);
      }  
    }
    snd_last_position = current_pos;
}

void pwm_audio_reset(void)
{
  if (snd_tx_buffer16 != NULL) {
    memset((void*)snd_tx_buffer16,0, snd_nb_samples*sizeof(short));
  }    
}

/********************************
 * Initialization
********************************/ 
void pwm_audio_init(int buffersize, void (*callback)(short * stream, int len))
{
  fillsamples = callback;
  snd_nb_samples = buffersize;
  snd_tx_buffer16 =  (short*)malloc(buffersize*sizeof(short));
  if (snd_tx_buffer16 == NULL) {
    //printf("sound buffer could not be allocated!!!!!\n");
    return;  
  }
  memset((void*)snd_tx_buffer16,0, snd_nb_samples*sizeof(short));

  // set GPIO function to PWM
  gpio_set_function(PWMSND_GPIO, GPIO_FUNC_PWM);

  // Setup PWM interrupt to fire when PWM cycle is complete
#ifdef AUDIO_IRQ
  pwm_clear_irq(PWMSND_SLICE);
  pwm_set_irq_enabled(PWMSND_SLICE, true);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
  irq_set_priority (PWM_IRQ_WRAP, 3);
  irq_set_enabled(PWM_IRQ_WRAP, true);
#endif

  pwm_config cfg = pwm_get_default_config();
  // set clock divider (INT = 0..255, FRAC = 1/16..15/16)
  //  125 MHz: 125000000/5644800 = 22.144, INT=22, FRAC=2,
  //     real sample rate = 125000000/(22+2/16)/256 = 22069Hz
  pwm_config_set_clkdiv(&cfg, (float)clock_get_hz(clk_sys)/PWMSND_CLOCK + 0.03f); // 0.03f = rounding 0.5/16
  // set period to 256 cycles
  pwm_config_set_wrap(&cfg, PWMSND_TOP);

  // start PWM
  pwm_init(PWMSND_SLICE, &cfg, True);
  pwm_set_gpio_level(AUDIO_PIN, 0);
}



