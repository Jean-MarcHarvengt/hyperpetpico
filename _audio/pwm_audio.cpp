#include "pwm_audio.h"

#ifdef AUDIO_IRQ
#ifndef AUDIO_DMA
#define SAMPLE_REPEAT_SHIFT 2    // shift 2 is REPETITION_RATE=4
#else
#define SAMPLE_REPEAT_SHIFT 0
#endif
#else
#define SAMPLE_REPEAT_SHIFT 0
#endif
#define REPETITION_RATE     (1<<SAMPLE_REPEAT_SHIFT) 

static void (*fillsamples)(audio_sample * stream, int len) = nullptr;
static audio_sample * snd_tx_buffer;   // samples buffer
static uint16_t snd_nb_samples;        // nb samples (mono)
static uint16_t snd_sample_ptr = 0;    // samples index
static bool snd_last_position;         // playback was first half at last buffer fill

static int pwm_dma_chan;

/********************************
 * Processing
********************************/ 
// process a single sample (from audio interrupt or timer or ..)
// should be called at SOUNDRATE
void pwm_audio_handle_sample(void)
{
  pwm_set_gpio_level(AUDIO_PIN, snd_tx_buffer[snd_sample_ptr >> SAMPLE_REPEAT_SHIFT]);
//  snd_sample_ptr = (snd_sample_ptr + 1) % (snd_nb_samples << SAMPLE_REPEAT_SHIFT);  
  if (snd_sample_ptr < (snd_nb_samples << SAMPLE_REPEAT_SHIFT) - 1) {
      ++snd_sample_ptr;
  } else {
      snd_sample_ptr = 0;
  }   
}

#ifdef AUDIO_IRQ
#ifndef AUDIO_DMA
static void AUDIO_isr() {
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  pwm_audio_handle_sample();
}
#else
static void AUDIO_isr() {
  dma_hw->ch[pwm_dma_chan].al3_read_addr_trig = (io_rw_32)snd_tx_buffer;
  dma_hw->ints0 = (1u << pwm_dma_chan);   
}
#endif
#endif

// fill half buffer depending on current position
void pwm_audio_handle_buffer(void)
{
#ifdef AUDIO_IRQ
#ifndef AUDIO_DMA
  bool current_pos = ((snd_sample_ptr >> SAMPLE_REPEAT_SHIFT) >= snd_nb_samples/2)?true:false;
#else
  bool current_pos = (dma_hw->ch[pwm_dma_chan].transfer_count >= snd_nb_samples/2)?true:false;
#endif
#else  
  bool current_pos = (snd_sample_ptr >= snd_nb_samples/2)?true:false;
#endif
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


/********************************
 * Initialization
********************************/ 
void pwm_audio_init(int buffersize, void (*callback)(audio_sample * stream, int len))
{
  fillsamples = callback;
  snd_nb_samples = buffersize;
  snd_sample_ptr = 0;
  snd_tx_buffer =  (audio_sample*)malloc(snd_nb_samples*sizeof(audio_sample));
  if (snd_tx_buffer == NULL) {
    //printf("sound buffer could not be allocated!!!!!\n");
    return;  
  }
  memset((void*)snd_tx_buffer,0, snd_nb_samples*sizeof(audio_sample));

  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
  pwm_set_gpio_level(AUDIO_PIN, 0);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, (((float)SOUNDRATE)/1000) / REPETITION_RATE);
  pwm_config_set_wrap(&config, 254);
  pwm_init(audio_pin_slice, &config, true);

#ifdef AUDIO_IRQ
#ifndef AUDIO_DMA
  // Each sample played from IRQ
  // Setup PWM interrupt to fire when PWM cycle is complete
  pwm_clear_irq(audio_pin_slice);
  pwm_set_irq_enabled(audio_pin_slice, true);    
  irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
  irq_set_enabled(PWM_IRQ_WRAP, true);
#else
  // Each sample played from DMA
  // Setup DMA channel to drive the PWM
  pwm_dma_chan = AUD_DMA_CHANNEL;
  dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(pwm_dma_chan);
  // Transfer 16 bits at once, increment read address to go through sample
  // buffer, always write to the same address (PWM slice CC register).
  channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_16);
  channel_config_set_read_increment(&pwm_dma_chan_config, true);
  channel_config_set_write_increment(&pwm_dma_chan_config, false);
  // Transfer on PWM cycle end
  channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);

  // Setup the channel and set it going
  dma_channel_configure(
      pwm_dma_chan,
      &pwm_dma_chan_config,
      &pwm_hw->slice[audio_pin_slice].cc, // Write to PWM counter compare
      snd_tx_buffer, // Read values from audio buffer
      snd_nb_samples,
      false // Start immediately if true.
  );

  // Setup interrupt handler to fire when PWM DMA channel has gone through the
  // whole audio buffer
  dma_channel_set_irq1_enabled(pwm_dma_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_1, AUDIO_isr);
  irq_set_enabled(DMA_IRQ_1, true);
  dma_channel_start(pwm_dma_chan);
#endif
#endif
}



