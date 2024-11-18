#include "global.h"
#include "hdmi.h"
#include "petbus.h"

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/pwm.h"

#include <string.h>
#include <cstdlib>
// ----------------------------------------------------------------------------
// DVI constants

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

#define MODE_H_SYNC_POLARITY 0
#define MODE_H_FRONT_PORCH   16
#define MODE_H_SYNC_WIDTH    96
#define MODE_H_BACK_PORCH    48
#define MODE_H_ACTIVE_PIXELS 640

#define MODE_V_SYNC_POLARITY 0
#define MODE_V_FRONT_PORCH   10
#define MODE_V_SYNC_WIDTH    2
#define MODE_V_BACK_PORCH    33
#define MODE_V_ACTIVE_LINES  480

#define MODE_H_TOTAL_PIXELS ( \
    MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + \
    MODE_H_BACK_PORCH  + MODE_H_ACTIVE_PIXELS \
)
#define MODE_V_TOTAL_LINES  ( \
    MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + \
    MODE_V_BACK_PORCH  + MODE_V_ACTIVE_LINES \
)

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)


#define TRA16 1

// ----------------------------------------------------------------------------
// HSTX command lists

// Lists are padded with NOPs to be >= HSTX FIFO size, to avoid DMA rapidly
// pingponging and tripping up the IRQs.

static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V1_H1,
    HSTX_CMD_NOP
};

static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V0_H1,
    HSTX_CMD_NOP
};

static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS       | MODE_H_ACTIVE_PIXELS
};

// ----------------------------------------------------------------------------
// DMA logic

#define DMACH_PING (VGA_DMA_CHANNEL)
#define DMACH_PONG (VGA_DMA_CHANNEL+1)

// First we ping. Then we pong. Then... we ping again.
static bool dma_pong = false;

// A ping and a pong are cued up initially, so the first time we enter this
// handler it is to cue up the second ping after the first ping has completed.
// This is the third scanline overall (-> =2 because zero-based).
static uint v_scanline = 2;

// During the vertical active period, we take two IRQs per scanline: one to
// post the command list, and another to post the pixels.
static bool vactive_cmdlist_posted = false;


// gfx mode
//0: 640x200
//1: 320x200 (pixel doubling)
//2: 256x200 (pixel doubling, centered)
static int hdmi_mode;

volatile Bool VSync;    // current scan line is vsync or dark
//volatile int VCount;    // current vsync count

// line buffers
static ALIGNED u8 LineBuf1[640];
static ALIGNED u8 LineBuf2[640];
static ALIGNED u8 LineBlack[640];
volatile int BufInx;  // current buffer set (0..1)
static u8* rend_dbuf=&LineBuf1[0]; 

//0x00680011
//#define DMA_CTRL_32 (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (DMA_CH0_CTRL_TRIG_INCR_READ_BITS) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) )
//#define DMA_CTRL_16 (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (DMA_CH0_CTRL_TRIG_INCR_READ_BITS) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_16 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) )
//#define DMA_CTRL_8  (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (DMA_CH0_CTRL_TRIG_INCR_READ_BITS) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_8  << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB ))
#define DMA_CTRL_32 (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (0x10) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) )
#define DMA_CTRL_16 (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (0x10) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_16 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) )
#define DMA_CTRL_8  (0x00000000 | DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB | (0x10) | (DMA_CH0_CTRL_TRIG_EN_BITS) | (DMA_SIZE_8  << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB ))


static uint32_t dma_ctrl_32[2];
static uint32_t dma_ctrl_hi[2];
static uint32_t dma_ctrl_lo[2];
static uint32_t dma_count;

//        dma_ctrl =    0x00680011; //  0x00686019
//        //dma_ctrl &= ~(0x0001e00c );
//        dma_ctrl |= DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB;
//        dma_ctrl &= ~(0x0000000c ); // CHAIN_TO 4bits, DMA_SIZE 2bits
//        dma_ctrl |= (DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB);
/*
        dma_ctrl  = DREQ_HSTX << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB |
                    DMA_CH0_CTRL_TRIG_INCR_READ_BITS |
                    DMA_CH0_CTRL_TRIG_BUSY_BITS |
                    //DMA_CH0_CTRL_TRIG_IRQ_QUIET_BITS |
                    DMA_SIZE_32 << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB |
                    dma_pong << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB |
                    DMA_CH0_CTRL_TRIG_EN_BITS;
*/
extern void VideoRenderLineBG(u8 * linebuffer, int scanline);
extern void VideoRenderLineL0(u8 * linebuffer, int scanline);
extern void VideoRenderLineL1(u8 * linebuffer, int scanline);
extern void VideoRenderUpdate(void);
extern void LineCall(void);
extern void Core1Call(void);

#ifdef HAS_AUDIO
static audio_sample * snd_buffer = NULL; // samples buffer
static uint16_t snd_nb_samples;      // total nb samples (mono) later divided by 2
static uint16_t snd_sample_ptr = 0;  // sample index
static int last_audio_buffer = 0;
static int cur_audio_buffer = 0;
static void (*fillsamples)(audio_sample * stream, int len) = nullptr;
#endif

void __scratch_x("") dma_irq_handler() {
    // dma_pong indicates the channel that just finished, which is the one
    // we're about to reload.
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;

#define DMACTRLREG al3_ctrl //ctrl_trig

    if (v_scanline >= MODE_V_FRONT_PORCH && v_scanline < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
        ch->DMACTRLREG = dma_ctrl_32[dma_pong];
        VSync = True;
#ifdef HAS_AUDIO
        pwm_set_gpio_level(AUDIO_PIN, snd_buffer[snd_sample_ptr++]);
#endif
        if (v_scanline & 1) LineCall();
        if (v_scanline == MODE_V_FRONT_PORCH) {
          VideoRenderUpdate();
          //VCount++;
        }  
    } else if (v_scanline < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
        ch->DMACTRLREG = dma_ctrl_32[dma_pong];
        VSync = False;
#ifdef HAS_AUDIO
        pwm_set_gpio_level(AUDIO_PIN, snd_buffer[snd_sample_ptr++]);
#endif
        if (v_scanline & 1) LineCall();
    } else if (!vactive_cmdlist_posted) {
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        ch->DMACTRLREG = dma_ctrl_32[dma_pong];
        vactive_cmdlist_posted = true;
#ifdef HAS_AUDIO
        pwm_set_gpio_level(AUDIO_PIN, snd_buffer[snd_sample_ptr++]);
#endif
        if (v_scanline & 1) LineCall();
    } else {
        int y0 = (v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES));
        vactive_cmdlist_posted = false;
        if ( (y0>=40) && (y0<442) ) {
          ch->read_addr = (uintptr_t)rend_dbuf;
          ch->transfer_count = dma_count;
          ch->DMACTRLREG = (hdmi_mode?dma_ctrl_lo[dma_pong]:dma_ctrl_hi[dma_pong]) ;
          // prepare buffers to be processed next
          u8* dbuf; // data buffer
          if (BufInx == 0)
          {
            dbuf = LineBuf1;
          }
          else
          {
            dbuf = LineBuf2;
          }
          y0 -= 40;
          u8 BufMod = y0 & 1;
          if (BufMod)
            BufInx = BufInx ^ 1;
          
          if (!BufMod) {  
            y0 >>= 1;
            if (hdmi_mode==2) dbuf+=32; // 256 => 64/2 border
            VideoRenderLineBG(dbuf, y0);
            VideoRenderLineL0(dbuf, y0);
          }
          else {
            y0 >>= 1;
            if (hdmi_mode==2)
            {
              uint32_t * dbuf32=(uint32_t *)dbuf;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32=0;
              VideoRenderLineL1(dbuf+32, y0);
              dbuf32=(uint32_t *)(dbuf+32+256);
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32++=0;
              *dbuf32=0;
            }
            else { 
              VideoRenderLineL1(dbuf, y0);
            }
            rend_dbuf = (y0 >= 200)?LineBlack:dbuf;
          }
        }
        else {
          ch->read_addr = (uintptr_t)LineBlack;
          ch->transfer_count = dma_count;
          ch->DMACTRLREG = (hdmi_mode?dma_ctrl_lo[dma_pong]:dma_ctrl_hi[dma_pong]) ;
        }

    }

    if (!vactive_cmdlist_posted) {
        v_scanline = (v_scanline + 1) % MODE_V_TOTAL_LINES;
    }
#ifdef HAS_AUDIO
    if (snd_sample_ptr >= snd_nb_samples) {
        snd_sample_ptr = 0;
    }
    cur_audio_buffer = (snd_sample_ptr >= (snd_nb_samples/2))?0:1;
#endif    
}

#ifdef HAS_AUDIO
void  HdmiHandleAudio(void)
{
    if (last_audio_buffer == cur_audio_buffer)
        return;

    audio_sample *buf = &snd_buffer[cur_audio_buffer*(snd_nb_samples/2)];
    if (fillsamples != NULL) fillsamples((audio_sample*)buf, snd_nb_samples/2);
    last_audio_buffer = cur_audio_buffer;
}

void HdmiResetAudio(void) 
{
  memset((void*)snd_buffer,0, snd_nb_samples*sizeof(audio_sample));
}
#endif


void (* volatile Core1Fnc)() = NULL; // core 1 remote function

void HdmiCore(void)
{
//  set_sys_clock_khz(125000, true);    
//  vreg_set_voltage(VREG_VOLTAGE_1_05);
  set_sys_clock_khz(250000, true);
//  set_sys_clock_khz(240000, true);
  //CLK_HSTX_DIV = 2 << 16; // HSTX clock/2
  *((uint32_t *)(0x40010000+0x58)) = 2 << 16;

  // Configure HSTX's TMDS encoder for RGB332
  hstx_ctrl_hw->expand_tmds =
      2  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB |
      0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   |
      2  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
      29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   |
      1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
      26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;

  // Pixels (TMDS) come in 2 8-bit chunks. Control symbols (RAW) are an
  // entire 32-bit word.
  hstx_ctrl_hw->expand_shift =
#ifdef TRA16
      2 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
#else
      4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
#endif      
      8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
      1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
      0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

  // Serial output config: clock period of 5 cycles, pop from command
  // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
  hstx_ctrl_hw->csr = 0;
  hstx_ctrl_hw->csr =
      HSTX_CTRL_CSR_EXPAND_EN_BITS |
      5u << HSTX_CTRL_CSR_CLKDIV_LSB |
      5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
      2u << HSTX_CTRL_CSR_SHIFT_LSB |
      HSTX_CTRL_CSR_EN_BITS;


  // Note we are leaving the HSTX clock at the SDK default of 125 MHz; since
  // we shift out two bits per HSTX clock cycle, this gives us an output of
  // 250 Mbps, which is very close to the bit clock for 480p 60Hz (252 MHz).
  // If we want the exact rate then we'll have to reconfigure PLLs.

  // HSTX outputs 0 through 7 appear on GPIO 12 through 19.
  // Pinout on Pico DVI sock:
  //
  //   GP12 D0+  GP13 D0-
  //   GP14 CK+  GP15 CK-
  //   GP16 D2+  GP17 D2-
  //   GP18 D1+  GP19 D1-

  // Assign clock pair to two neighbouring pins:
  hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
  hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
  for (uint lane = 0; lane < 3; ++lane) {
      // For each TMDS lane, assign it to the correct GPIO pair based on the
      // desired pinout:
      static const int lane_to_output_bit[3] = {0, 6, 4};
      int bit = lane_to_output_bit[lane];
      // Output even bits during first half of each HSTX cycle, and odd bits
      // during second half. The shifter advances by two bits each cycle.
      uint32_t lane_data_sel_bits =
          (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
          (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
      // The two halves of each pair get identical data, but one pin is inverted.
      hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
      hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
  }

  for (int i = 12; i <= 19; ++i) {
      gpio_set_function(i, (gpio_function_t)0); // HSTX
  }

  // Both channels are set up identically, to transfer a whole scanline and
  // then chain to the opposite channel. Each time a channel finishes, we
  // reconfigure the one that just finished, meanwhile the opposite channel
  // is already making progress.
  dma_channel_config c;
  c = dma_channel_get_default_config(DMACH_PING);
  channel_config_set_chain_to(&c, DMACH_PONG);
  channel_config_set_dreq(&c, DREQ_HSTX);
  dma_channel_configure(
      DMACH_PING,
      &c,
      &hstx_fifo_hw->fifo,
      vblank_line_vsync_off,
      count_of(vblank_line_vsync_off),
      false
  );
  c = dma_channel_get_default_config(DMACH_PONG);
  channel_config_set_chain_to(&c, DMACH_PING);
  channel_config_set_dreq(&c, DREQ_HSTX);
  dma_channel_configure(
      DMACH_PONG,
      &c,
      &hstx_fifo_hw->fifo,
      vblank_line_vsync_off,
      count_of(vblank_line_vsync_off),
      false
  );

  dma_ctrl_32[0] = DMA_CTRL_32 | (DMACH_PING << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_32[1] = DMA_CTRL_32 | (DMACH_PONG << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
#ifdef TRA16
  dma_ctrl_hi[0] = DMA_CTRL_16 | (DMACH_PING << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_hi[1] = DMA_CTRL_16 | (DMACH_PONG << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_lo[0] = DMA_CTRL_8 | (DMACH_PING << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_lo[1] = DMA_CTRL_8 | (DMACH_PONG << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  // transfer are 16bits (2 times more as if they were 32bits)
  dma_count = (MODE_H_ACTIVE_PIXELS) / (sizeof(uint16_t));
#else
  dma_ctrl_hi[0] = DMA_CTRL_32 | (DMACH_PING << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_hi[1] = DMA_CTRL_32 | (DMACH_PONG << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_lo[0] = DMA_CTRL_16 | (DMACH_PING << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  dma_ctrl_lo[1] = DMA_CTRL_16 | (DMACH_PONG << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
  // transfer are 32bits
  dma_count = (MODE_H_ACTIVE_PIXELS) / (sizeof(uint32_t));
#endif


  dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
  dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
  irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
  irq_set_enabled(DMA_IRQ_0, true);

  bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;



  for (int i=0; i<640;i++) {
    LineBuf1[i]=0xff;
    LineBuf2[i]=0x57;
    LineBlack[i]=0x00;
  }

#ifdef HAS_AUDIO
  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);
  pwm_set_gpio_level(AUDIO_PIN, 0);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, (((float)SOUNDRATE)/1000));
  pwm_config_set_wrap(&config, 254);
  pwm_init(audio_pin_slice, &config, true);
#endif

  dma_channel_start(DMACH_PING);

  void (*fnc)();
  while (1)
  {
    __dmb();
    // execute remote function
    fnc = Core1Fnc;
    if (fnc != NULL)
    {
      fnc();
      __dmb();
      Core1Fnc = NULL;
    }
    Core1Call();    
  }
}

void HdmiVSync(void)
{
  // wait for end of VSync
  while (VSync) { __dmb(); }

  // wait for start of VSync
  while (!VSync) { __dmb(); }
}

bool HdmiIsVSync(void)
{
  return VSync;
}

/*
int HdmiVCount(void)
{
  return VCount;
}
*/

void HdmiInit(int mode)
{
  hdmi_mode = mode;
}

#ifdef HAS_AUDIO
void HdmiInitAudio(int samplesize, void (*callback)(audio_sample * stream, int len))
{
  if ( snd_buffer == NULL) {
      snd_buffer =  (audio_sample*)malloc(samplesize*sizeof(audio_sample));
      if (snd_buffer == NULL) {
        return;  
      }  
      fillsamples = callback;    
      snd_nb_samples = samplesize;
      snd_sample_ptr = 0;
      memset((void*)snd_buffer,0, snd_nb_samples*sizeof(audio_sample));   
  }  
}
#endif

// execute core 1 remote function
void Core1Exec(void (*fnc)())
{
  __dmb();
  Core1Fnc = fnc;
  __dmb();
}



