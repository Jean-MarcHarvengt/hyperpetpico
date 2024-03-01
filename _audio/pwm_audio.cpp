#include "pwm_audio.h"

#define SAMPLE_REPEAT_SHIFT 2  // shift 2 is x4

static void (*fillsamples)(uint8_t * stream, int len) = nullptr;
static uint8_t * snd_tx_buffer;        // 8bits samples buffer
static uint16_t snd_nb_samples;        // in 8bits mono samples
static uint16_t snd_sample_ptr = 0;    // in 8bits samples pointer
static bool snd_last_position;         // playback was first half at last buffer fill

/********************************
 * Processing
********************************/ 
// process a single sample (from audio interrupt or timer or ..)
// should be called at SOUNDRATE
void pwm_audio_handle_sample(void)
{
  pwm_set_gpio_level(AUDIO_PIN, snd_tx_buffer[snd_sample_ptr >> 2]);
//  snd_sample_ptr = (snd_sample_ptr + 1) % (snd_nb_samples << SAMPLE_REPEAT_SHIFT);  

  if (snd_sample_ptr < (snd_nb_samples << SAMPLE_REPEAT_SHIFT) - 1) {
      ++snd_sample_ptr;
  } else {
      snd_sample_ptr = 0;
  }  
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
  bool current_pos = ((snd_sample_ptr >> SAMPLE_REPEAT_SHIFT) >= snd_nb_samples/2)?true:false;
  if (current_pos != snd_last_position) {
    if (current_pos ) {
      fillsamples(snd_tx_buffer, snd_nb_samples/2);
    }  
    else { 
      fillsamples(&snd_tx_buffer[snd_nb_samples/2], snd_nb_samples/2);
    }  
  }
  snd_last_position = current_pos;
}

void pwm_audio_reset(void)
{
  memset((void*)snd_tx_buffer,0, snd_nb_samples*sizeof(uint8_t));
}

/*
static int pwm_dma_chan;

void dma_irh() {
    dma_hw->ch[pwm_dma_chan].al3_read_addr_trig = (io_rw_32)snd_tx_buffer16;
    dma_hw->ints0 = (1u << pwm_dma_chan);
}
*/
/********************************
 * Initialization
********************************/ 
void pwm_audio_init(int buffersize, void (*callback)(uint8_t * stream, int len))
{
  fillsamples = callback;
  snd_nb_samples = buffersize;
  snd_sample_ptr = 0;
  snd_tx_buffer =  (uint8_t*)malloc(snd_nb_samples*sizeof(uint8_t));
  if (snd_tx_buffer == NULL) {
    //printf("sound buffer could not be allocated!!!!!\n");
    return;  
  }
  memset((void*)snd_tx_buffer,0, snd_nb_samples*sizeof(uint8_t));

/*
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


  //pwm_config cfg = pwm_get_default_config();
  //pwm_config_set_clkdiv(&cfg, 50.0f);
  //pwm_config_set_wrap(&cfg, 254);

  // start PWM
  pwm_init(PWMSND_SLICE, &cfg, True);
  pwm_set_gpio_level(AUDIO_PIN, 0);
*/
/*

// Setup DMA channel to drive the PWM
pwm_dma_chan = 4; //dma_claim_unused_channel(true);

dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(pwm_dma_chan);
// Transfer 16 bits at once, increment read address to go through sample
// buffer, always write to the same address (PWM slice CC register).
channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_16);
channel_config_set_read_increment(&pwm_dma_chan_config, true);
channel_config_set_write_increment(&pwm_dma_chan_config, false);
// Transfer on PWM cycle end
channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + PWMSND_SLICE);

// Setup the channel and set it going
dma_channel_configure(
    pwm_dma_chan,
    &pwm_dma_chan_config,
    &pwm_hw->slice[PWMSND_SLICE].cc, // Write to PWM counter compare
    snd_tx_buffer16, // Read values from audio buffer
    snd_nb_samples,
    true // Start immediately.
);

// Setup interrupt handler to fire when PWM DMA channel has gone through the
// whole audio buffer
dma_channel_set_irq1_enabled(pwm_dma_chan, true);
irq_set_exclusive_handler(DMA_IRQ_1, dma_irh);
irq_set_enabled(DMA_IRQ_1, true);
*/
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

#ifdef AUDIO_IRQ
    // Setup PWM interrupt to fire when PWM cycle is complete
    pwm_clear_irq(audio_pin_slice);
    pwm_set_irq_enabled(audio_pin_slice, true);    
    irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
    irq_set_enabled(PWM_IRQ_WRAP, true);
#endif
    
    // Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 5.5f);
    pwm_config_set_wrap(&config, 254);
    pwm_init(audio_pin_slice, &config, true);

    pwm_set_gpio_level(AUDIO_PIN, 0);
}



