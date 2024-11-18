#include "pwm_audio.h"

#ifdef HAS_AUDIO
#ifndef AUDIO_CB

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/float.h"
#include <string.h>
#include <stdio.h>

#ifdef AUDIO_IRQ
#define SAMPLE_REPEAT_SHIFT 2    // shift 2 is REPETITION_RATE=4
#endif
#ifdef AUDIO_1DMA
#define SAMPLE_REPEAT_SHIFT 0    // not possible to repeat samples with single DMA!! 
#endif
#ifdef AUDIO_3DMA
#define SAMPLE_REPEAT_SHIFT 2    // shift 2 is REPETITION_RATE=4
#endif
#ifndef SAMPLE_REPEAT_SHIFT
#define SAMPLE_REPEAT_SHIFT 0    // not possible to repeat samples CBACK!!
#endif

#define REPETITION_RATE     (1<<SAMPLE_REPEAT_SHIFT) 

static void (*fillsamples)(audio_sample * stream, int len) = nullptr;
static audio_sample * snd_buffer;       // samples buffer (1 malloc for 2 buffers)
static uint16_t snd_nb_samples;         // total nb samples (mono) later divided by 2
static uint16_t snd_sample_ptr = 0;     // sample index
static audio_sample * audio_buffers[2]; // pointers to 2 samples buffers 
static volatile int cur_audio_buffer;
static volatile int last_audio_buffer;
#ifdef AUDIO_3DMA
static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_chan, trigger_dma_chan, sample_dma_chan;
#endif
#ifdef AUDIO_1DMA
static int pwm_dma_chan;
#endif

/********************************
 * Processing
********************************/ 
#ifdef AUDIO_IRQ
static void __isr __time_critical_func(AUDIO_isr)()
{
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  pwm_set_gpio_level(AUDIO_PIN, snd_buffer[snd_sample_ptr >> SAMPLE_REPEAT_SHIFT]);
  if (snd_sample_ptr < (snd_nb_samples << SAMPLE_REPEAT_SHIFT) - 1) {
      ++snd_sample_ptr;
  } else {
      cur_audio_buffer = 1 - cur_audio_buffer;
      snd_buffer = audio_buffers[cur_audio_buffer];
      snd_sample_ptr = 0;
  }  
}
#endif

#ifdef AUDIO_1DMA
static void __isr __time_critical_func(AUDIO_isr)()
{
  cur_audio_buffer = 1 - cur_audio_buffer;  
  dma_hw->ch[pwm_dma_chan].al3_read_addr_trig = (intptr_t)audio_buffers[cur_audio_buffer];
  dma_hw->ints1 = (1u << pwm_dma_chan);   
}
#endif

#ifdef AUDIO_3DMA
static void __isr __time_critical_func(AUDIO_isr)()
{
  cur_audio_buffer = 1 - cur_audio_buffer;  
  dma_hw->ch[sample_dma_chan].al1_read_addr = (intptr_t)audio_buffers[cur_audio_buffer];
  dma_hw->ch[trigger_dma_chan].al3_read_addr_trig = (intptr_t)&single_sample_ptr;
  dma_hw->ints1 = (1u << trigger_dma_chan);
}
#endif

// fill half buffer depending on current position
void pwm_audio_handle_buffer(void)
{
  if (last_audio_buffer == cur_audio_buffer) {
    return;
  }
  audio_sample *buf = audio_buffers[last_audio_buffer];
  last_audio_buffer = cur_audio_buffer;
  if (fillsamples != NULL) fillsamples(buf, snd_nb_samples);
}

void pwm_audio_reset(void)
{
  memset((void*)snd_buffer,0, snd_nb_samples*sizeof(uint8_t));
}


/********************************
 * Initialization
********************************/ 
void pwm_audio_init(int buffersize, void (*callback)(audio_sample * stream, int len))
{
  fillsamples = callback;
  snd_nb_samples = buffersize;
  snd_sample_ptr = 0;
  snd_buffer =  (audio_sample*)malloc(snd_nb_samples*sizeof(audio_sample));
  if (snd_buffer == NULL) {
    printf("sound buffer could not be allocated!!!!!\n");
    return;  
  }
  memset((void*)snd_buffer,128, snd_nb_samples*sizeof(audio_sample));

  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
  pwm_set_gpio_level(AUDIO_PIN, 0);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, (((float)SOUNDRATE)/1000) / REPETITION_RATE);
  pwm_config_set_wrap(&config, 254);
  pwm_init(audio_pin_slice, &config, true);

  snd_nb_samples = snd_nb_samples/2;
  audio_buffers[0] = &snd_buffer[0];
  audio_buffers[1] = &snd_buffer[snd_nb_samples];

#ifdef AUDIO_IRQ
  // Each sample played from IRQ
  // Setup PWM interrupt to fire when PWM cycle is complete
  pwm_clear_irq(audio_pin_slice);
  pwm_set_irq_enabled(audio_pin_slice, true);    
  irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
  irq_set_priority (DMA_IRQ_1, PICO_DEFAULT_IRQ_PRIORITY-8);
  irq_set_enabled(PWM_IRQ_WRAP, true);
#endif
#ifdef AUDIO_3DMA
  int audio_pin_chan = pwm_gpio_to_channel(AUDIO_PIN);
  // DMA chain of 3 DMA channels
  sample_dma_chan = AUD_DMA_CHANNEL;
  pwm_dma_chan = AUD_DMA_CHANNEL+1;
  trigger_dma_chan = AUD_DMA_CHANNEL+2;

  // setup PWM DMA channel
  dma_channel_config pwm_dma_chan_config = dma_channel_get_default_config(pwm_dma_chan);
  channel_config_set_transfer_data_size(&pwm_dma_chan_config, DMA_SIZE_32);              // transfer 32 bits at a time
  channel_config_set_read_increment(&pwm_dma_chan_config, false);                        // always read from the same address
  channel_config_set_write_increment(&pwm_dma_chan_config, false);                       // always write to the same address
  channel_config_set_chain_to(&pwm_dma_chan_config, sample_dma_chan);                    // trigger sample DMA channel when done
  channel_config_set_dreq(&pwm_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);       // transfer on PWM cycle end
  dma_channel_configure(pwm_dma_chan,
                        &pwm_dma_chan_config,
                        &pwm_hw->slice[audio_pin_slice].cc,   // write to PWM slice CC register
                        &single_sample,                       // read from single_sample
                        REPETITION_RATE,                      // transfer once per desired sample repetition
                        false                                 // don't start yet
                        );


  // setup trigger DMA channel
  dma_channel_config trigger_dma_chan_config = dma_channel_get_default_config(trigger_dma_chan);
  channel_config_set_transfer_data_size(&trigger_dma_chan_config, DMA_SIZE_32);          // transfer 32-bits at a time
  channel_config_set_read_increment(&trigger_dma_chan_config, false);                    // always read from the same address
  channel_config_set_write_increment(&trigger_dma_chan_config, false);                   // always write to the same address
  channel_config_set_dreq(&trigger_dma_chan_config, DREQ_PWM_WRAP0 + audio_pin_slice);   // transfer on PWM cycle end
  dma_channel_configure(trigger_dma_chan,
                        &trigger_dma_chan_config,
                        &dma_hw->ch[pwm_dma_chan].al3_read_addr_trig,     // write to PWM DMA channel read address trigger
                        &single_sample_ptr,                               // read from location containing the address of single_sample
                        REPETITION_RATE * snd_nb_samples,              // trigger once per audio sample per repetition rate
                        false                                             // don't start yet
                        );
  dma_channel_set_irq1_enabled(trigger_dma_chan, true);    // fire interrupt when trigger DMA channel is done
  irq_set_exclusive_handler(DMA_IRQ_1, AUDIO_isr);
  irq_set_priority (DMA_IRQ_1, PICO_DEFAULT_IRQ_PRIORITY-8);
  irq_set_enabled(DMA_IRQ_1, true);

  // setup sample DMA channel
  dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(sample_dma_chan);
  channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8);  // transfer 8-bits at a time
  channel_config_set_read_increment(&sample_dma_chan_config, true);            // increment read address to go through audio buffer
  channel_config_set_write_increment(&sample_dma_chan_config, false);          // always write to the same address
  dma_channel_configure(sample_dma_chan,
                        &sample_dma_chan_config,
                        (char*)&single_sample + 2*audio_pin_chan,  // write to single_sample
                        snd_buffer,                      // read from audio buffer
                        1,                                         // only do one transfer (once per PWM DMA completion due to chaining)
                        false                                      // don't start yet
                        );

    // Kick things off with the trigger DMA channel
    dma_channel_start(trigger_dma_chan);
#endif
#ifdef AUDIO_1DMA
  // Each sample played from a single DMA channel
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
      snd_buffer, // Read values from audio buffer
      snd_nb_samples,
      false // Start immediately if true.
  );

  // Setup interrupt handler to fire when PWM DMA channel has gone through the
  // whole audio buffer
  dma_channel_set_irq1_enabled(pwm_dma_chan, true);
  irq_set_exclusive_handler(DMA_IRQ_1, AUDIO_isr);
  irq_set_priority (DMA_IRQ_1, PICO_DEFAULT_IRQ_PRIORITY-8);
  irq_set_enabled(DMA_IRQ_1, true);
  dma_channel_start(pwm_dma_chan);
#endif
}
#endif
#endif


