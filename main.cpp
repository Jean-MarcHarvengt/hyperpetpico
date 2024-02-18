// ****************************************************************************
//
//                                 Main code
//
// ****************************************************************************
#include "include.h"
#include "petfont.h"
#include "petbus.pio.h"
#include "reSID.h"
#include "mos6502.h"
#include "basic4_b000.h"
#include "basic4_c000.h"
#include "basic4_d000.h"
#include "edit4.h"
#include "edit480.h"
#include "kernal4.h"

#ifndef HAS_PETIO
#ifdef HAS_NETWORK
#include "pico/cyw43_arch.h"
#include "lwip/apps/tftp_server.h"
#include "dhcpserver.h"
#endif
#include "usb_kbd.h"
#include "kbd.h"
#endif
#include "decrunch.h"

#ifdef HAS_PETIO
// Petbus PIO config
#define CONFIG_PIN_PETBUS_DATA_BASE 0 /* 8+1(RW) pins */
#define CONFIG_PIN_PETBUS_RW (CONFIG_PIN_PETBUS_DATA_BASE + 8)
#define CONFIG_PIN_PETBUS_CONTROL_BASE (CONFIG_PIN_PETBUS_DATA_BASE + 9) //CE DATA,ADDRLO,ADDRHI
#define CONFIG_PIN_PETBUS_PHI2  26
#define CONFIG_PIN_PETBUS_RESET 22
const PIO pio = pio1;
const uint sm = 0;
#endif

// Graphics
#define VGA_RGB(r,g,b)   ( (((r>>5)&0x07)<<5) | (((g>>5)&0x07)<<2) | (((b>>6)&0x3)<<0) )

// Video mode
#ifdef SIXTYHZ
#define VideoMod VideoVGA
#else
#define VideoMod VideoPALp
#endif
//#define VideoMod VideoEGA

#define VMODE_HIRES    0
#define VMODE_LORES    1
#define VMODE_GAMERES  2
#ifndef HAS_PETIO
#ifdef EIGHTYCOL
#define VMODE_DEFAULT  VMODE_HIRES
#else
#define VMODE_DEFAULT  VMODE_LORES
#endif
#else
#define VMODE_DEFAULT  VMODE_HIRES
#endif

// screen resolution
#define HI_XRES        640
#define LO_XRES        320
#define GAME_XRES      256 

#ifndef HAS_PETIO
#ifdef EIGHTYCOL
#define MAXWIDTH       HI_XRES        // Max screen width
#else
#define MAXWIDTH       LO_XRES        // Max screen width
#endif
#else
#define MAXWIDTH       HI_XRES        // Max screen width
#endif
#define MAXHEIGHT      200            // Max screen height

// Sprite definition
#define SPRITEW        16             // Sprite width
#define SPRITEH        24 //16        // Sprite height
#define SPRITE_SIZE    (SPRITEW*SPRITEH)
#define SPRITE_NUM     16             // Max Sprite NR on same row
#define SPRITE_NUM_MAX 96             // Max reusable sprites NR 
#define SPRITE_NBTILES 64             // Sprite NB of tiles definition

// Tile definition
#define TILEW          8              // Tile width
#define TILEH          8              // Tile height
#define TILE_SIZE      (TILEW*TILEH)

#define TILE_NB_BANKS  2              // Number of banks (hack form bitmap unpack)
// Tile data max size
#define TILE_MAXDATA   (8*8*256*TILE_NB_BANKS)  // (or 16*16*64)
#define TILEMAP_SIZE   0x800

// Font
#define FONTW 8                       // Font width
#define FONTH 8                       // Font height
#define FONT_SIZE (FONTW*FONTH)

// Video configurations
static sVmode Vmode0; // videomode setup
static sVgaCfg Cfg0;  // required configuration
static sVmode Vmode1;
static sVgaCfg Cfg1;
static sVmode Vmode2;
static sVgaCfg Cfg2;

// Screen resolution
static u8 video_mode = VMODE_DEFAULT;
static u16 screen_height = 200;
static u16 screen_width;

// Frame buffer
static ALIGNED u8 Bitmap[LO_XRES*MAXHEIGHT];
// Sprites
struct Sprite {
   u8  y;
   u8  id;
   u8  flipH;
   u8  flipV;
   u32 x;
};

struct SprYSortedItem {
   u8 count;
   u8 first;
};  

static Sprite SpriteParams[SPRITE_NUM_MAX];
static SprYSortedItem SpriteYSorted[256];
static u8 SprYSortedIndexes[256];
static u8 SprYSortedIds[SPRITE_NUM_MAX+1];


// Sprite definition
static ALIGNED u8 SpriteData[SPRITE_SIZE*SPRITE_NBTILES];

// Font definition
static bool font_lowercase = false;

// Tile definition
static ALIGNED u8 TileData[TILE_MAXDATA];

// PET shadow memory 8000-9fff
static unsigned char mem[0x2000];
static bool pet_reset = false;
//
// 8000-9fff: memory map GFX+SOUND expansion
//
// RESOLUTION:
// 640x200 HI_RES   (PET8032)
// 320x200 LO_RES   (PET4032)
// 256x200 GAME_RES (New resolution suitable for 80's ARCADE games)
//
// LAYERING:
// - background color
// - L0 = bitmap (1) or tile 8x8/16x16 (scollable) 
// - L1 = petfont 8x8 or tile 8x8/16x16 (scollable)
// - L2 = sprites (with H/V flip)
// (1) 320x200 also in HIRES

//
// 8000-87ff: videomem standard petfont text map L1 (fmap1)
//            (83ff for 4032, 87ff for 8032) 
// 8800-8fff: videomem expanded tiles map L1 (tmap1)
// 9000-97ff: videomem expanded tiles map L0 (tmap0)
//
// 9800-99ff: MAX 128 sprites registers (id,xhi,xlo,y)
//            id:    0-5
//            hflip: 6
//            vflip: 7
// 9a00-9aff: transfer lookup (if used as color palette, 256 RGB332 colors)
// 9b00: video mode 
//       0-1: resolution (0=640x200,1=320x200,2=256x200)
// 9b01: background color (RGB332)
// 9b02: layers config
//       0: L0 on/off
//       1: L1 on/off                   (off if HIRES and bitmap in L0!)
//       2: L2 on/off
//       3: L2 sprites between L0 and L1
//       4: bitmap/tile in L0
//       5: petfont/tile in L1
//       6: enable scroll area in L0
//       7: enable scroll area in L1
// 9b03: lines config 2
//       0: single/perline background color
//       1: single/perline L0 xscroll
//       2: single/perline L1 xscroll
// 9b04: xscroll hi registers
//       3-0: L0 xscroll hi
//       7-4: L1 xscroll hi
// 9b05: L0 xscroll lo
// 9b06: L1 xscroll lo
// 9b07: L0 yscroll
// 9b08: L1 yscroll
// 9b09: L0 scroll area's line start (0-24)
//       4-0
// 9b0a: L0 scroll area's line end
//       4-0
// 9b0b: L1 scroll area's line start (0-24)
//       4-0
// 9b0c: L1 scroll area's line end
//       4-0
// 9b0d: foreground color (RGB332)
//
// 9b0e: tiles config
//       0: L0: 0=8x8, 1=16x16
//       1: L1: 0=8x8, 1=16x16
//       2-4: xcurtain
//        0: on/off
//        1: 8/16 pixels left
//       5-7: ycurtain
//        0: on/off
//        1: 8/16 pixels top
// 9b0f: spare!!!
// 9b10: 3-0: transfer mode 
//       1/2/4/8 bits per pixel (using indexed CLUT)
//       9 = 8 bits RGB332 no CLUT
//       0 = compressed
// 9b11: transfer command
//       0: idle
//       1: transfer tiles data      (data=tilenr,w,h,packet pixels)
//       2: transfer sprites data    (data=spritenr,w,h,packet pixels)
//       3: transfer bitmap data     (data=xh,xl,y,wh,wl,h,w*h/packet pixels) 
//       4: transfer t/fmap col data (data=layer,col,row,size,size/packet tiles)
//       5: transfer t/fmap row data (data=layer,col,row,size,size/packet tiles)
//       6: transfer all tile 8bits data compressed (data=sizeh,sizel,pixels)
//       7: transfer all sprite 8bits data compressed (data=sizeh,sizel,pixels)
//       8: transfer bitmap 8bits data compressed (data=sizeh,sizel,pixels)

// 9b12: transfer params
// 9b13: transfer data
//
// Redefining tiles/sprite sequence
// 1. write lookup palette entries needed
// 2. write transfer mode
// 3. write command 1/2
// 4. write params tile/sprite NR,w,h
// 5. write data sequence (8bytes*plane for tiles, 24*2*plane for sprites)
// 6. write new command to reset
//
// Transfer bitmap sequence
// 1. write lookup palette entries needed
// 2. write transfer mode
// 3. write command 3
// 4. write params XH,XL,Y,WH,WL,H
// 5. write data sequence (N bytes/packed_bits)
// 6. write transfer=0 to reset
//
// 9b38-9bff: lines background color (RGB332)
// 9c00-9cc7: 7-4:  lines L1 xscroll hi, 3-0: L0 xscroll hi
// 9cc8-9d8F: lines L0 xscroll lo
// 9d90-9e58: lines L1 xscroll lo
//
// 9f00-9f1d: SID registers (d400 on C64)
//
// 9f80-9fff: Sprite collision
// 
#define REG_TEXTMAP_L1    (0x8000 - 0x8000) // 32768
#define REG_TILEMAP_L1    (0x8800 - 0x8000) // 34816
#define REG_TILEMAP_L0    (0x9000 - 0x8000) // 36864
#define REG_SPRITE_IND    (0x9800 - 0x8000) // 38912
#define REG_SPRITE_XHI    (0x9880 - 0x8000) // 39040
#define REG_SPRITE_XLO    (0x9900 - 0x8000) // 39168
#define REG_SPRITE_Y      (0x9980 - 0x8000) // 39296
#define REG_TLOOKUP       (0x9a00 - 0x8000) // 39424
#define REG_VIDEO_MODE    (0x9b00 - 0x8000) // 39680
#define REG_BG_COL        (0x9b01 - 0x8000) // 39681
#define REG_LAYERS_CFG    (0x9b02 - 0x8000) // 39682
#define REG_LINES_CFG     (0x9b03 - 0x8000) // 39683
#define REG_XSCROLL_HI    (0x9b04 - 0x8000) // 39684
#define REG_XSCROLL_L0    (0x9b05 - 0x8000) // 39685
#define REG_XSCROLL_L1    (0x9b06 - 0x8000) // 39686
#define REG_YSCROLL_L0    (0x9b07 - 0x8000) // 39687
#define REG_YSCROLL_L1    (0x9b08 - 0x8000) // 39688
#define REG_SC_START_L0   (0x9b09 - 0x8000) // 39689
#define REG_SC_END_L0     (0x9b0a - 0x8000) // 39690
#define REG_SC_START_L1   (0x9b0b - 0x8000) // 39691
#define REG_SC_END_L1     (0x9b0c - 0x8000) // 39692
#define REG_FG_COL        (0x9b0d - 0x8000) // 39693
#define REG_TILES_CFG     (0x9b0e - 0x8000) // 39694

#define REG_TDEPTH        (0x9b10 - 0x8000) // 39696
#define REG_TCOMMAND      (0x9b11 - 0x8000) // 39697
#define REG_TPARAMS       (0x9b12 - 0x8000) // 39698
#define REG_TDATA         (0x9b13 - 0x8000) // 39699

#define REG_LINES_BG_COL  (0x9b38 - 0x8000) // 39736
#define REG_LINES_XSCR_HI (0x9c00 - 0x8000) // 39936
#define REG_LINES_L0_XSCR (0x9cc8 - 0x8000) // 40136
#define REG_LINES_L1_XSCR (0x9d90 - 0x8000) // 40336

#define REG_SID_BASE      (0x9f00 - 0x8000) // 40192

#define GET_VIDEO_MODE    ( mem[REG_VIDEO_MODE] )
#define GET_BG_COL        ( mem[REG_BG_COL] )
#define GET_FG_COL        ( mem[REG_FG_COL] )
#define GET_XSCROLL_L0    ( mem[REG_XSCROLL_L0] | ((mem[REG_XSCROLL_HI] & 0x0f)<<8) )
#define GET_YSCROLL_L0    ( mem[REG_YSCROLL_L0] )
#define GET_XSCROLL_L1    ( mem[REG_XSCROLL_L1] | ((mem[REG_XSCROLL_HI] & 0xf0)<<4) )
#define GET_YSCROLL_L1    ( mem[REG_YSCROLL_L1] )
#define GET_SC_START_L0   ( mem[REG_SC_START_L0] & 31 )
#define GET_SC_END_L0     ( mem[REG_SC_END_L0] & 31 )
#define GET_SC_START_L1   ( mem[REG_SC_START_L1] & 31 )
#define GET_SC_END_L1     ( mem[REG_SC_END_L1] & 31 )

#define SET_VIDEO_MODE(x) { mem[REG_VIDEO_MODE] = x; }
#define SET_BG_COL(x)     { mem[REG_BG_COL] = x; }
#define SET_FG_COL(x)     { mem[REG_FG_COL] = x; }
#define SET_XSCROLL_L0(x) { mem[REG_XSCROLL_L0] = x & 0xff; mem[REG_XSCROLL_HI] &= 0xf0; mem[REG_XSCROLL_HI] |= (x>>8); }
#define SET_XSCROLL_L1(x) { mem[REG_XSCROLL_L1] = x & 0xff; mem[REG_XSCROLL_HI] &= 0x0f; mem[REG_XSCROLL_HI] |= ((x>>4)&0xf0); }
#define SET_YSCROLL_L0(x) { mem[REG_YSCROLL_L0] = x; }
#define SET_YSCROLL_L1(x) { mem[REG_YSCROLL_L1] = x; }
#define SET_SC_START_L0(x) { mem[REG_SC_START_L0] = x & 31; }
#define SET_SC_END_L0(x)  { mem[REG_SC_END_L0] = x & 31; }
#define SET_SC_START_L1(x) { mem[REG_SC_START_L1] = x & 31; }
#define SET_SC_END_L1(x)  { mem[REG_SC_END_L1] = x & 31; }

#define SET_LAYER_MODE(x) { mem[REG_LAYERS_CFG] = x; }
#define SET_LINE_MODE(x)  { mem[REG_LINES_CFG] = x; }
#define SET_TILE_MODE(x)  { mem[REG_TILES_CFG] = x; }


#define VIDEO_MODE_HIRES  ( mem[REG_VIDEO_MODE] == 0 )  
#define LAYER_L0_ENA      ( mem[REG_LAYERS_CFG] & 0x1  )  
#define LAYER_L1_ENA      ( mem[REG_LAYERS_CFG] & 0x2  )  
#define LAYER_L2_ENA      ( mem[REG_LAYERS_CFG] & 0x4  )
#define L2_BETWEEN_ENA    ( mem[REG_LAYERS_CFG] & 0x8  )
#define L0_TILE_ENA       ( mem[REG_LAYERS_CFG] & 0x10 )  
#define L1_TILE_ENA       ( mem[REG_LAYERS_CFG] & 0x20 )
#define L0_AREA_ENA       ( mem[REG_LAYERS_CFG] & 0x40 )   
#define L1_AREA_ENA       ( mem[REG_LAYERS_CFG] & 0x80 )   

#define BG_COL_LINE_ENA   ( mem[REG_LINES_CFG]  & 0x01 )
#define L0_XSCR_LINE_ENA  ( mem[REG_LINES_CFG]  & 0x02 )
#define L1_XSCR_LINE_ENA  ( mem[REG_LINES_CFG]  & 0x04 )

#define L0_TILE_16_ENA     ( mem[REG_TILES_CFG]  & 0x01 )
#define L1_TILE_16_ENA     ( mem[REG_TILES_CFG]  & 0x02 )
#define HCURTAIN8_ENA      ( (mem[REG_TILES_CFG] & (0x04+0x08)) == (0x04) )
#define HCURTAIN16_ENA     ( (mem[REG_TILES_CFG] & (0x04+0x08)) == (0x04+0x08) )
#define VCURTAIN8_ENA      ( (mem[REG_TILES_CFG] & (0x20+0x40)) == (0x20) )
#define VCURTAIN16_ENA     ( (mem[REG_TILES_CFG] & (0x20+0x40)) == (0x20+0x40) )

#define LAYER_L0_BITMAP   ( 0x1 )  
#define LAYER_L0_TILE     ( 0x1 | 0x10 )  
#define LAYER_L1_PETFONT  ( 0x2 )  
#define LAYER_L1_TILE     ( 0x2 | 0x20)
#define LAYER_L2_SPRITE   ( 0x4 )
#define LAYER_L2_INBETW   ( 0x8 )
#define LAYER_L0_AREA     ( 0x40)
#define LAYER_L1_AREA     ( 0x80)

#define LINE_BG_COL       ( 0x1 )
#define LINE_L0_XSCR      ( 0x2 )
#define LINE_L1_XSCR      ( 0x4 )

#define L0_TILE_16        ( 0x1 )  
#define L1_TILE_16        ( 0x2 )  
#define HCURTAIN_8        ( 0x4 )  
#define HCURTAIN_16       ( 0x4 + 0x8 ) 
#define VCURTAIN_8        ( 0x20 )  
#define VCURTAIN_16       ( 0x20 + 0x40 )  

// ****************************************
// Audio code
// ****************************************
static AudioPlaySID playSID;

static bool fillfirsthalf = true;
static uint16_t cnt = 0;
static uint16_t sampleBufferSize = 0;
static void (*fillsamples)(short * stream, int len) = nullptr;
static uint32_t * snd_tx_buffer;
static short * snd_tx_buffer16;

static void AUDIO_isr() {
  pwm_clear_irq(pwm_gpio_to_slice_num(AUDIO_PIN));
  long s = snd_tx_buffer16[cnt++]; 
  s += snd_tx_buffer16[cnt++];
  s = s/2 + 32767; 
  pwm_set_gpio_level(AUDIO_PIN, s >> 8);
  cnt = cnt & (sampleBufferSize*2-1);

  if (cnt == 0) 
  {
    fillfirsthalf = false;
    //irq_set_pending(RTC_IRQ+1);
    multicore_fifo_push_blocking(0);
  } 
  else if (cnt == sampleBufferSize) {
    fillfirsthalf = true;
    //irq_set_pending(RTC_IRQ+1);
    multicore_fifo_push_blocking(0);
  }
}

static void core1_sio_irq() {
  irq_clear(SIO_IRQ_PROC1);
  while(multicore_fifo_rvalid()) {
    uint16_t raw = multicore_fifo_pop_blocking();
    if (fillfirsthalf) {
      fillsamples((short *)snd_tx_buffer, sampleBufferSize);
    }  
    else { 
      fillsamples((short *)&snd_tx_buffer[sampleBufferSize/2], sampleBufferSize);
    }
  } 
  multicore_fifo_clear_irq();
}


#define AUDIO_BUFFER_SIZE 1024
#define REPETITION_RATE 4

static uint32_t single_sample = 0;
static uint32_t *single_sample_ptr = &single_sample;
static int pwm_dma_chan, trigger_dma_chan, sample_dma_chan;

static uint8_t audio_buffers[2][AUDIO_BUFFER_SIZE];
static volatile int cur_audio_buffer;
static volatile int last_audio_buffer;

static void __isr __time_critical_func(dma_handler)()
{
  cur_audio_buffer = 1 - cur_audio_buffer;
  dma_hw->ch[sample_dma_chan].al1_read_addr       = (intptr_t) &audio_buffers[cur_audio_buffer][0];
  dma_hw->ch[trigger_dma_chan].al3_read_addr_trig = (intptr_t) &single_sample_ptr;

  dma_hw->ints1 = 1u << trigger_dma_chan;
}

static void AudioInit(int samplesize, void (*callback)(short * stream, int len))
{
  fillsamples = callback;
  snd_tx_buffer =  (uint32_t*)malloc(samplesize*sizeof(uint32_t));

  if (snd_tx_buffer == NULL) {
    printf("sound buffer could not be allocated!!!!!\n");
    return;  
  }
  memset((void*)snd_tx_buffer,0, samplesize*sizeof(uint32_t));


  snd_tx_buffer16 = (short*)snd_tx_buffer;
  sampleBufferSize = samplesize;

  gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
  
  int audio_pin_slice = pwm_gpio_to_slice_num(AUDIO_PIN);

  int audio_pin_chan = pwm_gpio_to_channel(AUDIO_PIN);


  // Setup PWM interrupt to fire when PWM cycle is complete
  pwm_clear_irq(audio_pin_slice);
  pwm_set_irq_enabled(audio_pin_slice, true);
  irq_set_exclusive_handler(PWM_IRQ_WRAP, AUDIO_isr);
  irq_set_priority (PWM_IRQ_WRAP, 128);
  irq_set_enabled(PWM_IRQ_WRAP, true);

  //irq_set_exclusive_handler(RTC_IRQ+1,SOFTWARE_isr);
  //irq_set_priority (RTC_IRQ+1, 120);
  //irq_set_enabled(RTC_IRQ+1,true);

  // Setup PWM for audio output
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 50.0f);
  pwm_config_set_wrap(&config, 254);
  pwm_init(audio_pin_slice, &config, true);

  pwm_set_gpio_level(AUDIO_PIN, 0);

/*

  pwm_dma_chan     = 4; //dma_claim_unused_channel(true);
  trigger_dma_chan = 4; //dma_claim_unused_channel(true);
  sample_dma_chan  = 4; //dma_claim_unused_channel(true);

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
                        REPETITION_RATE * AUDIO_BUFFER_SIZE,              // trigger once per audio sample per repetition rate
                        false                                             // don't start yet
                        );
  dma_channel_set_irq1_enabled(trigger_dma_chan, true);    // fire interrupt when trigger DMA channel is done
  irq_set_exclusive_handler(DMA_IRQ_1, dma_handler);
  irq_set_enabled(DMA_IRQ_1, true);

  // setup sample DMA channel
  dma_channel_config sample_dma_chan_config = dma_channel_get_default_config(sample_dma_chan);
  channel_config_set_transfer_data_size(&sample_dma_chan_config, DMA_SIZE_8);  // transfer 8-bits at a time
  channel_config_set_read_increment(&sample_dma_chan_config, true);            // increment read address to go through audio buffer
  channel_config_set_write_increment(&sample_dma_chan_config, false);          // always write to the same address
  dma_channel_configure(sample_dma_chan,
                        &sample_dma_chan_config,
                        (char*)&single_sample + 2*audio_pin_chan,  // write to single_sample
                        &audio_buffers[0][0],                      // read from audio buffer
                        1,                                         // only do one transfer (once per PWM DMA completion due to chaining)
                        false                                      // don't start yet
                        );


  // clear audio buffers
  memset(audio_buffers[0], 128, AUDIO_BUFFER_SIZE);
  memset(audio_buffers[1], 128, AUDIO_BUFFER_SIZE);

  // kick things off with the trigger DMA channel
  dma_channel_start(trigger_dma_chan);
*/  




}

static void  SND_Process( short * stream, int len )
{
    playSID.update((void *)stream, len);
}

static char buffer[26];
static char oldbuffer[26];

static void sid_dump( void )
{
  for(int i=0;i<25;i++) 
  {
    buffer[i] = mem[REG_SID_BASE+i];
    if(buffer[i] != oldbuffer[i]) {       
        playSID.setreg(i, buffer[i]);
        oldbuffer[i] = buffer[i];                  
    } 
  }
}

// ****************************************
// Setup Video mode
// ****************************************
static void VgaCoreWithSound()
{
  multicore_fifo_clear_irq();
  irq_set_exclusive_handler(SIO_IRQ_PROC1,core1_sio_irq);
  irq_set_enabled(SIO_IRQ_PROC1,true);
  VgaCore();
}


static void ResetGFXMem(void) 
{
  memset((void*)&Bitmap[0],0, sizeof(Bitmap));
  memset((void*)&TileData[0],0, sizeof(TileData));
  memset((void*)&SpriteData[0],0, sizeof(SpriteData));
  //sleep_ms(10);
}

static void VideoInit(u8 mode, bool firstTime)
{
  // initialize base layer 0
  switch (mode) {
    case 0: // 640x200
      screen_width = 640;
      // initialize system clock
      set_sys_clock_pll(Vmode0.vco*1000, Vmode0.pd1, Vmode0.pd2);
      // initialize videomode
      if (firstTime) VgaInitReq(&Vmode0);
      else VgaInit(&Vmode0);
      break;
    case 1: // 320x200
      screen_width = 320;
      // initialize system clock
      set_sys_clock_pll(Vmode1.vco*1000, Vmode1.pd1, Vmode1.pd2);
      // initialize videomode
      if (firstTime) VgaInitReq(&Vmode1);
      else VgaInit(&Vmode1);
      break;
    case 2: // 256x200
      screen_width = 256;
      // draw box
      // initialize system clock
      set_sys_clock_pll(Vmode2.vco*1000, Vmode2.pd1, Vmode2.pd2);
      // initialize videomode
      if (firstTime) VgaInitReq(&Vmode2);
      else VgaInit(&Vmode2);
    default:
      break; 
  }
  video_mode = mode;
  SET_XSCROLL_L0(0);
  SET_XSCROLL_L1(0);
//  if (video_mode == VMODE_HIRES) memset(Bitmap, 0 , MAXWIDTH*MAXHEIGHT/2);  
}


static void VideoInit0(void) {
  VideoInit(0, false);
} 

static void VideoInit1(void) {
  VideoInit(1, false);
} 

static void VideoInit2(void) {
  VideoInit(2, false);
} 

static void VideoInitNew(u8 mode)
{
  if (mode == 0) Core1Exec(VideoInit0);  
  else if (mode == 1) Core1Exec(VideoInit1); 
  else Core1Exec(VideoInit2);  
} 

static void VideoSetup(void)
{
  // setup videomode
  VgaCfgDef(&Cfg0);
  Cfg0.video = &VideoMod;
  Cfg0.width = 640;
  Cfg0.height = screen_height;
#ifdef SIXTYHZ
  Cfg0.dbly = true;  
#endif
  Cfg0.freq = 250000;
  VgaCfg(&Cfg0, &Vmode0);

  VgaCfgDef(&Cfg1);
  Cfg1.video = &VideoMod;
  Cfg1.width = 320;
  Cfg1.height = screen_height;
#ifdef SIXTYHZ
  Cfg1.dbly = true;  
#endif
  Cfg1.freq = 250000;
  VgaCfg(&Cfg1, &Vmode1);

  VgaCfgDef(&Cfg2);
  Cfg2.video = &VideoMod;
  Cfg2.width = 256;
  Cfg2.height = screen_height;
#ifdef SIXTYHZ
  Cfg2.dbly = true;  
#endif
  Cfg2.freq = 250000;
  VgaCfg(&Cfg2, &Vmode2);
  VideoInit(video_mode, true);
  SET_VIDEO_MODE(video_mode);
}

void __not_in_flash("VideoRenderUpdate") VideoRenderUpdate(void)
{
    int vmode = GET_VIDEO_MODE;
    if (video_mode != vmode) {
      if (vmode == 0) VideoInit(0, false);  
      else if (vmode == 1) VideoInit(1, false); 
      else VideoInit(2, false);
      //VideoInitNew(vmode);
    }

    // sort sprites by Y, max amount sprites per scanline
    memset((void*)&SpriteYSorted[0],0, sizeof(SpriteYSorted));
    for (int i = 0; i < SPRITE_NUM_MAX; i++)
    { 
      if (mem[REG_SPRITE_Y+i] < MAXHEIGHT)
      {  
        SpriteYSorted[mem[REG_SPRITE_Y+i]].count++;
      }
      else {
        SpriteParams[i].y = MAXHEIGHT;  
      }   
    }  
    // Sort sprites by Y
    int nextid = 1;
    for (int i = 0; i < SPRITE_NUM_MAX; i++)
    { 
      uint8_t y = mem[REG_SPRITE_Y+i];
      if (y < MAXHEIGHT) {      
        if ( (SpriteYSorted[y].count) && (SpriteYSorted[y].first == 0) ) {
          SpriteYSorted[y].first = nextid;
          nextid += SpriteYSorted[y].count;
          SprYSortedIndexes[y] = 0;
        }
        if (SpriteYSorted[y].count) 
        {
          SprYSortedIds[SpriteYSorted[y].first+SprYSortedIndexes[y]] = i;
          SprYSortedIndexes[y]++;
        }
      }
    } 
    // update sprites params
    int cur = 0;
    for (int j = 0; j < MAXHEIGHT; j++)
    {
      for (int i=0;i<SpriteYSorted[j].count; i++) 
      {
        if (i<SPRITE_NUM)
        {
          if (cur < SPRITE_NUM_MAX) {
            int ind = SprYSortedIds[SpriteYSorted[j].first+i];
            uint8_t id = mem[REG_SPRITE_IND+ind];
            uint16_t x = (mem[REG_SPRITE_XHI+ind]<<8)+mem[REG_SPRITE_XLO+ind];    
            uint8_t y = mem[REG_SPRITE_Y+ind];
            SpriteParams[cur].x = x;
            SpriteParams[cur].id = id & 0x3f;
            SpriteParams[cur].y = y;
            SpriteParams[cur].flipH = (id & 0x40);
            SpriteParams[cur].flipV = (id & 0x80);
            cur++;       
          }  
        }
        else 
        {
          break;
        }
      }  
    }
    for (int i=cur; i < SPRITE_NUM_MAX; i++)
    {
      SpriteParams[cur].y = MAXHEIGHT;
    }
}


void __not_in_flash("VideoRenderLineBG") VideoRenderLineBG(u8 * linebuffer, int scanline)
{
  // Background color
  uint32_t bgcolor32 = (mem[REG_BG_COL]<<24)+(mem[REG_BG_COL]<<16)+(mem[REG_BG_COL]<<8)+(mem[REG_BG_COL]);
  uint32_t * dst32 = (uint32_t *)linebuffer;      
  if ( BG_COL_LINE_ENA ) 
  {
    u8 bgcol = mem[REG_LINES_BG_COL+scanline];
    bgcolor32 = (bgcol<<24)+(bgcol<<16)+(bgcol<<8)+(bgcol);
  }
  RenderColor(dst32, bgcolor32, (screen_width/4));
}

void __not_in_flash("VideoRenderLineL0") VideoRenderLineL0(u8 * linebuffer, int scanline)
{
  // Layer 0
  if (LAYER_L0_ENA)
  {
    scanline = (scanline + GET_YSCROLL_L0) % MAXHEIGHT;
    int scroll = L0_XSCR_LINE_ENA?GET_XSCROLL_L0+( mem[REG_LINES_L0_XSCR+scanline] | ((mem[REG_LINES_XSCR_HI+scanline] & 0x0f)<<8) ):GET_XSCROLL_L0;
    if ( L0_TILE_ENA )
    {
      u8 bgcolor = GET_BG_COL;
      if (L0_TILE_16_ENA) 
      {
        unsigned char * tilept = &mem[(scanline >> 4)*(screen_width >> 4)+REG_TILEMAP_L0];
        u8 * src = &TileData[(scanline&15) << 4];
        int screen_width_in_tiles = screen_width >> 4;
        if (screen_width_in_tiles == 16) { 
          TileBlitKey16_16(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&15, (scroll>>3)%16);
        }
        else if (screen_width_in_tiles == 20) { 
          TileBlitKey16_20(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&15, (scroll>>3)%20);
        }
        else {   
          TileBlitKey16_40(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&15, (scroll>>3)%40);
        }
      }
      else
      {
        unsigned char * tilept = &mem[(scanline >> 3)*(screen_width >> 3)+REG_TILEMAP_L0];
        u8 * src = &TileData[(scanline&7) << 3];
        int screen_width_in_tiles = screen_width >> 3;
        if (screen_width_in_tiles == 32) { 
          TileBlitKey8_32(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&7, (scroll>>3)%32);
        }
        else if (screen_width_in_tiles == 40) { 
          TileBlitKey8_40(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&7, (scroll>>3)%40);
        }
        else { 
          TileBlitKey8_80(linebuffer, tilept, screen_width_in_tiles, bgcolor, src, scroll&7, (scroll>>3)%80);
        }
      }
    }
    else
    {
      // Bitmap mode
      if (screen_width == GAME_XRES) {
        LineBlitKey32(linebuffer, &Bitmap[scanline*screen_width],screen_width, scroll&7, (scroll>>3)%32);
      }
      else if (screen_width == LO_XRES) {
        LineBlitKey40(linebuffer, &Bitmap[scanline*screen_width],screen_width, scroll&7, (scroll>>3)%40);
      }
      else { 
//        LineBlitKey80(linebuffer, &Bitmap[scanline*screen_width/2],screen_width/2, scroll&7, (scroll>>3)%40);
        LineBlit80(linebuffer, &Bitmap[scanline*screen_width/2],screen_width/2, scroll&7, (scroll>>3)%40);
      }
    }
  }

  // Sprites if in between L0 and L1
  if ( (LAYER_L2_ENA) && (L2_BETWEEN_ENA) ) 
  {
    Sprite16((SPRITE_NUM << 8) + SPRITE_NUM_MAX, screen_width, scanline, (u8 *)&SpriteParams[0], (u8 *)&SpriteData[0], linebuffer);    
  }  
}

void __not_in_flash("VideoRenderLineL1") VideoRenderLineL1(u8 * linebuffer, int scanline)
{
  // Curtain V
  if ( ( (scanline < 8) && ( VCURTAIN8_ENA ) ) || ( (scanline < 16) && ( VCURTAIN16_ENA ) ) ) 
  {
    uint32_t * dst32 = (uint32_t *)linebuffer;      
    RenderColor(dst32, 0, (screen_width/4));
  }
  else
  {
    if (LAYER_L1_ENA)
    {
      scanline = (scanline + GET_YSCROLL_L1) % MAXHEIGHT;    
      if ( L1_TILE_ENA )
      {
        int scroll = L1_XSCR_LINE_ENA?GET_XSCROLL_L1+( mem[REG_LINES_L1_XSCR+scanline] | ((mem[REG_LINES_XSCR_HI+scanline] & 0x0f)<<8) ):GET_XSCROLL_L1;
        if (L1_TILE_16_ENA) 
        {
          unsigned char * tilept = &mem[(scanline >> 4)*(screen_width >> 4)+REG_TILEMAP_L1];
          u8 * src = &TileData[(scanline&15) << 4];
          int screen_width_in_tiles = screen_width >> 4;
          if (screen_width_in_tiles == 16) {
            TileBlitKey16_16(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&15, (scroll>>3)%16);
          }
          else if (screen_width_in_tiles == 20) {
            TileBlitKey16_20(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&15, (scroll>>3)%20);
          }
          else {  
            TileBlitKey16_40(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&15, (scroll>>3)%40);
          }
        }
        else
        {
          unsigned char * tilept = &mem[(scanline >> 3)*(screen_width >> 3)+REG_TILEMAP_L1];
          u8 * src = &TileData[(scanline&7) << 3];
          int screen_width_in_tiles = screen_width >> 3;
          if (screen_width_in_tiles == 32) {
            TileBlitKey8_32(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&7, (scroll>>3)%32);
          }
          else if (screen_width_in_tiles == 40) { 
            TileBlitKey8_40(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&7, (scroll>>3)%40);
          }
          else { 
            TileBlitKey8_80(linebuffer, tilept, screen_width_in_tiles, 0, src, scroll&7, (scroll>>3)%80);
          }
        }
      }
      else
      {
        u8 fgcolor = GET_FG_COL;
        unsigned char * charpt = &mem[(scanline>>3)*(screen_width/FONTW)+REG_TEXTMAP_L1];    
        unsigned char * fontpt = &petfont[(scanline&0x7)+(font_lowercase?0x800:0x000)];
        int scroll = L1_XSCR_LINE_ENA?GET_XSCROLL_L1+( mem[REG_LINES_L1_XSCR+scanline] | ((mem[REG_LINES_XSCR_HI+scanline] & 0xf0)<<4) ):GET_XSCROLL_L1;
        int screen_width_in_chars = screen_width >> 3;
        if (screen_width_in_chars == 32) { 
          TextBlitKey32(linebuffer, charpt, screen_width_in_chars, fgcolor, fontpt, scroll&7, (scroll>>3)%40);          
        }
        else if (screen_width_in_chars == 40) { 
          TextBlitKey40(linebuffer, charpt, screen_width_in_chars, fgcolor, fontpt, scroll&7, (scroll>>3)%40);          
        }
        else { 
  //        TextBlit80(linebuffer, charpt, screen_width_in_chars/2, &fgcolorlut[0], fontpt, scroll&7, (scroll>>3)%40);          
          TextBlitKey80(linebuffer, charpt, screen_width_in_chars, fgcolor, fontpt, scroll&7, (scroll>>3)%40);          
        }
      }
    } 

    // Layer 2
    if ( (LAYER_L2_ENA) && (!L2_BETWEEN_ENA) )
    {
      Sprite16((SPRITE_NUM << 8) + SPRITE_NUM_MAX, screen_width, scanline, (u8 *)&SpriteParams[0], (u8 *)&SpriteData[0], linebuffer); 
    }

    // Curtain H
    uint32_t color32 = 0x00000000;
    uint32_t * dst32 = (uint32_t *)linebuffer;      
    if ( HCURTAIN8_ENA )
    {
      *dst32++=color32;
      *dst32=color32;
    }
    else if ( HCURTAIN16_ENA ) 
    {
      *dst32++=color32;
      *dst32++=color32;
      *dst32++=color32;
      *dst32=color32;
    }
  }
}

static void VideoRenderInit(void)
{
  // initialize shared memory
  memset((void*)&mem[0x0000], 0, 0x1800); // text/tilemap
  memset((void*)&mem[0x1800], 0, 0x0800);  // the rest
  // initialize GFX memory
  ResetGFXMem();  

  // prepare sprites
  for (int i = 0; i < SPRITE_NUM_MAX; i++)
  {
    SpriteParams[i].x = 0;
    SpriteParams[i].id = 0;
    SpriteParams[i].y = MAXHEIGHT;
    SpriteParams[i].flipH = 0;
    SpriteParams[i].flipV = 0;
  }

  // Init tile map with something
  /*
  for (int i=0;i<TILEMAP_SIZE;i++) 
  {
    mem[REG_TILEMAP_L0+i] = i&255;  // L0 tiles
    mem[REG_TEXTMAP_L1+i] = i&255;  // L1 text
    mem[REG_TILEMAP_L1+i] = i&255;  // L1 tiles
  }
  */
  /*
  // init raster colors
  for (int i=0;i<MAXHEIGHT;i++) 
  {
    mem[REG_LINES_BG_COL+i] = VGA_RGB(i&7*32,0,0); // Lines BG colors
  }
  */
  SET_BG_COL(VGA_RGB(0x00,0x00,0x00));
  SET_FG_COL(VGA_RGB(0x00,0xff,0x00));
  // 0: L0 tiles + L1 petfont
  // 1: L0 tiles + L1 tiles
  // 2: L0 tiles
  // 3: L0 bitmap  
//  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L1_TILE | LAYER_L2_SPRITE );
//  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L1_PETFONT | LAYER_L2_SPRITE );
//  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L1_PETFONT | LAYER_L2_SPRITE | LAYER_L2_INBETW );
  SET_LAYER_MODE( LAYER_L0_BITMAP | LAYER_L1_PETFONT | LAYER_L2_SPRITE );
//  SET_LAYER_MODE( LAYER_L0_BITMAP | LAYER_L1_PETFONT | LAYER_L2_SPRITE | LAYER_L2_INBETW );
//  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L1_PETFONT | LAYER_L0_AREA | LAYER_L1_AREA | LAYER_L2_SPRITE );
//  SET_LAYER_MODE( LAYER_L1_PETFONT | LAYER_L1_AREA );
//  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L0_AREA );
//  SET_LAYER_MODE( LAYER_L0_BITMAP | LAYER_L1_TILE );
//  SET_LINE_MODE( LINE_BG_COL | LINE_L1_XSCR );
//  SET_LINE_MODE( LINE_L1_XSCR);
//  SET_SC_START_L0(13);
//  SET_SC_END_L0(24);
//  SET_SC_START_L1(0);
//  SET_SC_END_L1(11);
//  SET_XSCROLL_L0(0);
//  SET_XSCROLL_L1(0);
//  for (int i=0;i<MAXHEIGHT;i++) 
//  {
//    mem[REG_LINES_XSCR_HI+i] = 0;
//    mem[REG_LINES_L0_XSCR+i] = 0;
//    mem[REG_LINES_L1_XSCR+i] = 0;
//  }

#ifndef HAS_PETIO
#ifdef EIGHTYCOL
  SET_VIDEO_MODE(0);
#else
  SET_VIDEO_MODE(1);
#endif  
#endif
}

// ****************************************
// PET memory access IRQ
// ****************************************
#define MAX_CMD 16
#define MAX_PAR 8

static uint8_t tra_params[MAX_PAR];
static int cmd_nb_params;
static int param_ind;
static void (*traParamFunc)(void);

static uint8_t * tra_address;
static int tra_x;
static int tra_w;
static int tra_h;
static int tra_stride;
static uint8_t tra_spr_id;
static void (*traDataFunc)(uint8_t);
static uint8_t * tra_pal = &mem[REG_TLOOKUP];

#define CMD_QUEUE_SIZE 256
typedef struct {
   uint8_t  id;
   uint8_t  p8_1;
   uint8_t  p8_2;
   uint8_t  p8_3;
   uint16_t p16_1;
   uint16_t p16_2;
} QueueItem;

static QueueItem cmd_queue[CMD_QUEUE_SIZE];
static uint8_t cmd_queue_rd=0;
static uint8_t cmd_queue_wr=0;
static uint8_t cmd_queue_cnt=0;

typedef enum {
  cmd_undef=0,
  cmd_transfer_tile_data=1,
  cmd_transfer_sprite_data=2,
  cmd_transfer_bitmap_data=3,
  cmd_transfer_tilemap_col=4,
  cmd_transfer_tilemap_row=5,
  cmd_transfer_packed_tile_data=6,
  cmd_transfer_packed_sprite_data=7,
  cmd_transfer_packed_bitmap_data=8,
  cmd_unpack_tiles=9,
  cmd_unpack_sprites=10,
  cmd_unpack_bitmap=11,
  cmd_bitmap_clr=12,
  cmd_bitmap_point=13,
  cmd_bitmap_rect=14, 
  cmd_bitmap_tri=15,
} Cmd;

static void handleCmdQueue(void) {
  while (cmd_queue_cnt)
  {
    QueueItem cmd = cmd_queue[cmd_queue_rd];
    cmd_queue_rd = (cmd_queue_rd + 1)&(CMD_QUEUE_SIZE-1);
    cmd_queue_cnt--;
    switch (cmd.id) {
      case cmd_unpack_tiles:
        UnPack(0, &Bitmap[0], &TileData[0], sizeof(TileData));
        break;
      case cmd_unpack_sprites:
        UnPack(0, &Bitmap[0], &SpriteData[0], sizeof(SpriteData));
        break;
      case cmd_unpack_bitmap:
        UnPack(0, &TileData[0], &Bitmap[0], sizeof(Bitmap));
        break;
      case cmd_bitmap_clr:
        memset((void*)&Bitmap[0],0, sizeof(Bitmap));
        break;
      default:
        break;
    }  
  }  
}

static void pushCmdQueue(QueueItem cmd ) {
  if (cmd_queue_cnt != 256)
  {
    cmd_queue[cmd_queue_wr] = cmd;
    cmd_queue_wr = (cmd_queue_wr + 1)&(CMD_QUEUE_SIZE-1);
    cmd_queue_cnt++;
  }  
}

static const uint8_t cmd_params_len[MAX_CMD]={ 
//       0:  idle
//       1:  transfer tiles data      (data=tilenr,w,h,packet pixels)
//       2:  transfer sprites data    (data=spritenr,w,h,packet pixels)
//       3:  transfer bitmap data     (data=xh,xl,y,wh,wl,h,w*h/packet pixels) 
//       4:  transfer t/fmap col data (data=layer,col,row,size,size/packet tiles)
//       5:  transfer t/fmap row data (data=layer,col,row,size,size/packet tiles)
//       6:  transfer all tile 8bits data compressed (data=sizeh,sizel,pixels)
//       7:  transfer all sprite 8bits data compressed (data=sizeh,sizel,pixels)
//       8:  transfer bitmap 8bits data compressed (data=sizeh,sizel,pixels)  
//       9:  unpack tiles   
//       10: unpack sprites   
//       11: unpack bitmap
//       12: bitmap clr
//       13: bitmap point
//       14: bitmap tri
//       15: bitmap rect
  0,3,3,6,4,4,2,2,2, 0,0,0, 0,0,0,0
}; 

static void traParamFuncDummy(void){
}

static void traParamFuncTile(void){
  tra_spr_id = SPRITE_NBTILES; // not a sprite!
  tra_x = 0;
  tra_w = tra_params[1];
  tra_h = tra_params[2];
  tra_stride = tra_w;
  tra_address = &TileData[tra_w*tra_stride*(tra_params[0])];
}

static void traParamFuncSprite(void){
  tra_spr_id = tra_params[0] & (SPRITE_NBTILES-1);
  tra_x = 0;
  tra_w = tra_params[1];
  tra_h = tra_params[2]; 
  tra_stride = tra_w;
  tra_address = &SpriteData[tra_w*tra_stride*tra_spr_id];
}

static void traParamFuncBitmap(void){
  tra_stride = screen_width==HI_XRES?screen_width/2:screen_width;  
  tra_address = &Bitmap[tra_stride*tra_params[2]+((tra_params[0]<<8)+tra_params[1])];
  tra_x = 0; 
  tra_w = (tra_params[3]<<8)+tra_params[4];
  tra_h = tra_params[5];  
}

static void traParamFuncTmapcol(void){
  tra_stride = screen_width/8; 
  tra_address = &mem[REG_TILEMAP_L0-TILEMAP_SIZE*tra_params[0]+tra_stride*tra_params[2]+tra_params[1]];
  tra_x = 0; 
  tra_w = 1;
  tra_h = tra_params[3];
}

static void traParamFuncTmaprow(void){
  tra_stride = screen_width/8; 
  tra_address = &mem[REG_TILEMAP_L0-TILEMAP_SIZE*tra_params[0]+tra_stride*tra_params[2]+tra_params[1]];
  tra_x = 0; 
  tra_w = tra_params[3];
  tra_h = 1; 
}

static void traParamFuncPackedTiles(void){
  tra_h = (tra_params[0]<<8)+tra_params[1];
  tra_x = 0; //sizeof(TileData)-tra_h;
  tra_w = tra_h;
  tra_address = &Bitmap[0];
}

static void traParamFuncPackedSprites(void){
  tra_h = (tra_params[0]<<8)+tra_params[1];
  tra_x = 0; //sizeof(SpriteData)-tra_h;
  tra_w = tra_h;
  tra_address = &Bitmap[0];
}

static void traParamFuncPackedBitmap(void){
  tra_h = (tra_params[0]<<8)+tra_params[1];
  tra_x = 0; //sizeof(Bitmap)-tra_h;
  tra_w = tra_h;
  tra_address = &TileData[0];
}

static void traParamFuncExecuteCommand(void){
  switch (mem[REG_TCOMMAND]) 
  {
    case cmd_transfer_packed_tile_data:
      pushCmdQueue({cmd_unpack_tiles});
      break;
    case cmd_transfer_packed_sprite_data:
      pushCmdQueue({cmd_unpack_sprites});
      break;
    case cmd_transfer_packed_bitmap_data:
      pushCmdQueue({cmd_unpack_bitmap});
      break;
  }
}


static void (*traParamFuncPtr[MAX_CMD])(void) = {
  traParamFuncDummy,
  traParamFuncTile,
  traParamFuncSprite,
  traParamFuncBitmap,
  traParamFuncTmapcol,
  traParamFuncTmaprow,
  traParamFuncPackedTiles,
  traParamFuncPackedSprites,
  traParamFuncPackedBitmap,
  traParamFuncDummy,
  traParamFuncDummy,
  traParamFuncDummy,
  
  traParamFuncExecuteCommand,
  traParamFuncExecuteCommand,
  traParamFuncExecuteCommand,
  traParamFuncExecuteCommand
};

static void traDataFunc8nolut(uint8_t val) {
  tra_address[tra_x++]=val; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
}

static void traDataFunc8(uint8_t val) {
  tra_address[tra_x++]=tra_pal[val]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
}

static void traDataFunc4(uint8_t val) {
  tra_address[tra_x++]=tra_pal[val>>4]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
  if (tra_h) {
    tra_address[tra_x++]=tra_pal[val&0xf]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
  }  
}

static void traDataFunc2(uint8_t val) {
  tra_address[tra_x++]=tra_pal[(val>>6)&0x3]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
  if (tra_h) {
    tra_address[tra_x++]=tra_pal[(val>>4)&0x3]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
    if (tra_h) {
      tra_address[tra_x++]=tra_pal[(val>>2)&0x3]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
      if (tra_h) {
        tra_address[tra_x++]=tra_pal[(val)&0x3]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
      }
    }
  }
}

static void traDataFunc1(uint8_t val) {
  tra_address[tra_x++]=tra_pal[(val>>7)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
  if (tra_h) {
    tra_address[tra_x++]=tra_pal[(val>>6)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
    if (tra_h) {
      tra_address[tra_x++]=tra_pal[(val>>5)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
      if (tra_h) {
        tra_address[tra_x++]=tra_pal[(val>>4)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
        if (tra_h) {
          tra_address[tra_x++]=tra_pal[(val>>3)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
          if (tra_h) {
            tra_address[tra_x++]=tra_pal[(val>>2)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
            if (tra_h) {
              tra_address[tra_x++]=tra_pal[(val>>1)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
              if (tra_h) {
                tra_address[tra_x++]=tra_pal[(val)&0x1]; if (tra_x == tra_w) {tra_x=0; tra_address+=tra_stride; tra_h--; };
              }
            }
          }
        }
      }
    }
  }
}

static void traDataFunc8nolutPacked(uint8_t val) {
  if (tra_h > 0) tra_address[tra_x++]=val; tra_h--;
}

static void (*traDataFuncPtr[])(uint8_t) = {
  traDataFunc8nolutPacked, // 0
  traDataFunc1, // 1
  traDataFunc2, // 2 
  traDataFunc2, // 3
  traDataFunc4, // 4
  traDataFunc4, // 5
  traDataFunc4, // 6
  traDataFunc4, // 7
  traDataFunc8, // 8
  traDataFunc8nolut, // 9
  traDataFunc8nolut, // 10
  traDataFunc8nolut, // 11
  traDataFunc8nolut, // 12
  traDataFunc8nolut, // 13
  traDataFunc8nolut, // 14
  traDataFunc8nolut, // 15
};

static void handle_custom_registers(uint16_t address, uint8_t value) 
{
  switch (address-0x8000) 
  {  
    case REG_TDEPTH:
      traDataFunc = traDataFuncPtr[value&0x0f];
      break;
    case REG_TCOMMAND:
      param_ind = 0;
      cmd_nb_params = cmd_params_len[value&(MAX_CMD-1)];
      traParamFunc = traParamFuncPtr[value&(MAX_CMD-1)];
      break;
    case REG_TPARAMS:
      tra_params[param_ind++]=value;
      if (param_ind == cmd_nb_params) traParamFunc();
      break;
    case REG_TDATA:
      if (tra_h)
      {
        traDataFunc(value);
        if (!tra_h) {
          switch (mem[REG_TCOMMAND]) 
          {
            case cmd_transfer_packed_tile_data:
              pushCmdQueue({cmd_unpack_tiles});
              break;
            case cmd_transfer_packed_sprite_data:
              pushCmdQueue({cmd_unpack_sprites});
              break;
            case cmd_transfer_packed_bitmap_data:
              pushCmdQueue({cmd_unpack_bitmap});
              break;
          }
        }  
      }   
      break;
    default:
      break;
  } 
}

#ifdef HAS_PETIO
static void rx_irq(void) {
  while(!pio_sm_is_rx_fifo_empty(pio, sm)) {
    uint32_t value = pio_sm_get(pio, sm);
    const bool is_write = ((value & (1u << (CONFIG_PIN_PETBUS_RW - CONFIG_PIN_PETBUS_DATA_BASE))) == 0);
    uint_fast16_t address = (value >> 9) & 0xffff;
    if ( (is_write) && (gpio_get(CONFIG_PIN_PETBUS_RESET)) )
    {
      if ( address >= 0x8000) 
      {
        value &= 0xff;
        if (address == 0xe84C) 
        {
            // e84C 12=LO, 14=HI
            if (value & 0x02) 
            {
              font_lowercase = true;
            }
            else 
            {
              font_lowercase = false;
            }
        }    
        if ( address < 0xa000) {
          mem[address-0x8000] = value;
          handle_custom_registers(address, value);
        } 
      }
    }  
  }
}

static void init_petpio(void)
{
  // Init PETBUS
  uint offset = pio_add_program(pio, &petbus_program);
  pio_sm_claim(pio, sm);
  pio_sm_config c = petbus_program_get_default_config(offset);
  // set the bus R/W pin as the jump pin
  sm_config_set_jmp_pin(&c, CONFIG_PIN_PETBUS_RW);
  // map the IN pin group to the data signals
  sm_config_set_in_pins(&c, CONFIG_PIN_PETBUS_DATA_BASE);
  // map the SET pin group to the bus transceiver enable signals
  sm_config_set_set_pins(&c, CONFIG_PIN_PETBUS_CONTROL_BASE, 3);
  // configure left shift into ISR & autopush every 25 bits
  sm_config_set_in_shift(&c, false, true, 24+1);
  pio_sm_init(pio, sm, offset, &c);
  // configure the GPIOs
  // Ensure all transceivers will start disabled
  pio_sm_set_pins_with_mask(
      pio, sm, (uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE, (uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE);
  pio_sm_set_pindirs_with_mask(pio, sm, (0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE),
      (1 << CONFIG_PIN_PETBUS_PHI2) | (1 << CONFIG_PIN_PETBUS_RESET) | (0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | (0x1ff << CONFIG_PIN_PETBUS_DATA_BASE));

  // Disable input synchronization on input pins that are sampled at known stable times
  // to shave off two clock cycles of input latency
  pio->input_sync_bypass |= (0x1ff << CONFIG_PIN_PETBUS_DATA_BASE);
  
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_PHI2);
  gpio_set_pulls(CONFIG_PIN_PETBUS_PHI2, false, false);
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_RESET);
  gpio_set_pulls(CONFIG_PIN_PETBUS_RESET, false, false);

  for(int pin = CONFIG_PIN_PETBUS_CONTROL_BASE; pin < CONFIG_PIN_PETBUS_CONTROL_BASE + 3; pin++) {
      pio_gpio_init(pio, pin);
  }
  for(int pin = CONFIG_PIN_PETBUS_DATA_BASE; pin < CONFIG_PIN_PETBUS_DATA_BASE + 9; pin++) {
      pio_gpio_init(pio, pin);
      gpio_set_pulls(pin, false, false);
  }

  // Find a free irq
  static int8_t pio_irq;
  static_assert(PIO0_IRQ_1 == PIO0_IRQ_0 + 1 && PIO1_IRQ_1 == PIO1_IRQ_0 + 1, "");
  pio_irq = (pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
  if (irq_get_exclusive_handler(pio_irq)) {
      pio_irq++;
      if (irq_get_exclusive_handler(pio_irq)) {
          panic("All IRQs are in use");
      }
  }

  // Enable interrupt
  irq_add_shared_handler(pio_irq, rx_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
  irq_set_enabled(pio_irq, true); // Enable the IRQ
  const uint irq_index = pio_irq - ((pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0); // Get index of the IRQ
  pio_set_irqn_source_enabled(pio, irq_index, (pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + sm), true); // Set pio to tell us when the FIFO is NOT empty

  pio_sm_set_enabled(pio, sm, true);
  pio_enable_sm_mask_in_sync(pio, (1 << sm));
}
#endif


#ifndef HAS_PETIO 
// ****************************************
// 6502/pet emu
// ****************************************
#define PET_MEMORY_SIZE 0x8000 // for 32k

// 6502 emu
static mos6502 mos;
static uint8_t* petram = NULL;
//static uint8_t petram[PET_MEMORY_SIZE];
static bool pet_running = true;
static bool prg_start = false;
#define INPUT_DELAY 50
static uint8_t input_delay = INPUT_DELAY;
static uint8_t input_pt = 0;
static const uint8_t input_cmd[] = {'L', 'I', 'S', 'T', 0x0d, 'R', 'U', 'N', 0x0d, 0}; // LIST + RUN
//static const uint8_t input_cmd[] = {'L', 'I', 'S', 'T', '1', 'R', 'U', 'N', '2', 0}; // LIST + RUN
static uint8_t input_chr;
static uint8_t _rows[0x10];
static uint8_t _row;


/*
Professionnal keyboard map
----+------------------------
row |  7  6  5  4  3  2  1  0
----+------------------------
 9  | 16 04 3A 03 39 36 33 DF
    | ^V --  : ^C  9  6  3 <-   ^V = TAB + <- + DEL, ^C = STOP,
    |                            <- = left arrow
 8  | B1 2F 15 13 4D 20 58 12
    | k1  / ^U ^S  m sp  x ^R   k9 = keypad 9, ^U = RVS + A + L,
    |                           ^S = HOME, sp = space, ^R = RVS
 7  | B2 10 0F B0 2C 4E 56 5A   ^O = Z + A + L, rp = repeat
    | k2 rp ^O k0  ,  n  v  z
    |
 6  | B3 00 19 AE 2E 42 43 00
    | k3 rs ^Y k.  .  b  c ls   ^Y = left shift + TAB + I, k. = keypad .
    |                           ls = left shift, rs = right shift
 5  | B4 DB 4F 11 55 54 45 51   ^Q = cursor down
    | k4  [  o ^Q  u  t  e  q
    |    5D]
 4  | 14 50 49 DC 59 52 57 09
    | ^T  p  i  \  y  r  w ^I   ^T = DEL, ^I = TAB
    |          C0@
 3  | B6 C0 4C 0D 4A 47 44 41
    | k6  @  l ^M  j  g  d  a   ^M = return
    |    5B[
 2  | B5 3B 4B DD 48 46 53 9B
    | k5  ;  k  ]  h  f  s ^[   ^[ = ESC
    |    5C\   3B;
 1  | B9 06 DE B7 B0 37 34 31
    | k9 --  ^ k7  0  7  4  1
    |
 0  | 05 0E 1D B8 2D 38 35 32
    |  . ^N ^] k8  -  8  5  2   ^N = both shifts + 2, ^] = cursor right
*/

static const uint8_t asciimap[8*10] = {
/*----+-----------------------------------------------*/
/*row |   7     6     5     4     3     2     1     0 */
/*----+-----------------------------------------------*/
/* 9  |*/ 0x16, 0x04, 0x3A, 0x03, 0x39, 0x36, 0x33, 0xDF,
/* 8  |*/ 0xB1, 0x2F, 0x15, 0x13, 0x4D, 0x20, 0x58, 0x12,
/* 7  |*/ 0xB2, 0x10, 0x0F, 0xB0, 0x2C, 0x4E, 0x56, 0x5A,
/* 6  |*/ 0xB3, 0x00, 0x19, 0xAE, 0x2E, 0x42, 0x43, 0x00,
/* 5  |*/ 0xB4, 0xDB, 0x4F, 0x11, 0x55, 0x54, 0x45, 0x51,
/* 4  |*/ 0x14, 0x50, 0x49, 0xDC, 0x59, 0x52, 0x57, 0x09,
/* 3  |*/ 0xB6, 0xC0, 0x4C, 0x0D, 0x4A, 0x47, 0x44, 0x41,
/* 2  |*/ 0xB5, 0x3B, 0x4B, 0xDD, 0x48, 0x46, 0x53, 0x9B,
/* 1  |*/ 0xB9, 0x06, 0xDE, 0xB7, 0xB0, 0x37, 0x34, 0x31,
/* 0  |*/ 0x05, 0x0E, 0x1D, 0xB8, 0x2D, 0x38, 0x35, 0x32
};

static uint8_t ascii2rowcol(uint8_t chr) 
{
  uint8_t rowcol = 0;
  for (int i=0;i<sizeof(asciimap); i++) {
    if (asciimap[i] == chr) {
      int col = 7-(i&7);
      int row = 9-(i>>3);
      rowcol = (row<<4)+col;
    }  
  } 
  return rowcol; 
}  

uint8_t readWord( uint16_t location)
{
  if (location < 0x8000)  {
    if (location < PET_MEMORY_SIZE) {
      return petram[location];
    }
    else {
      return 0xff;
    }  
  }
  else if (location < 0xa000) {
    return mem[location-0x8000];
  }  
  else if (location < 0xb000) {
    return 0;
  }  
  else if (location < 0xc000) {
    return basic4_b000[location-0xb000];
  }  
  else if (location < 0xd000) {
    return basic4_c000[location-0xc000];
  }  
  else if (location < 0xe000) {
    return basic4_d000[location-0xd000];
  } 
  else if (location < 0xe800) {
#ifdef EIGHTYCOL    
    return edit480[location-0xe000];
#else 
    return edit4[location-0xe000];
#endif    
  } 
  else if ( (location > 0xe800) && (location < 0xf000) ) {
    if (location == 0xe812)         // PORT B
      return (_rows[_row] ^ 0xff);    
    else if (location == 0xe810)    // PORT A
      return (_row | 0x80); 
    else
      return 0x00;
  }  
  else {
    return kernal4[location-0xf000];
  }
}

void writeWord( uint16_t location, uint8_t value)
{
  if (location < 0x8000) { 
    if (location < PET_MEMORY_SIZE) {
      petram[location] = value;
    }
  }
  else if (location < 0xa000) {
    mem[location-0x8000] = value;
    handle_custom_registers(location, value);
  }
  else if ( (location > 0xe800) && (location < 0xf000) ) {
    if (location == 0xe812)       // PORT B
    {
    } 
    else if (location == 0xe810)  // PORT A
    {
      _row = (value & 0x0f);
    }          
    else if (location == 0xe84C) {
      if (value & 0x02) 
      {
        font_lowercase = true;
      }
      else 
      {
        font_lowercase = false;
      }
    }  
  }  
}

static void pet_start(void) 
{
  petram = (uint8_t*)malloc(PET_MEMORY_SIZE);
  for (int i=0;i<PET_MEMORY_SIZE;i++) 
  {
    petram[i] = 0;
  }
 
  mos.Reset();
  for (int i = sizeof(_rows); i--; )
    _rows[i] = 0;
  pet_running = true;
  prg_start = false;
  input_delay = INPUT_DELAY;
}

static void pet_step(void) 
{
  mos.Run(9000);
  mos.IRQ();
}

static void _set(uint8_t k) {
  _rows[(k & 0xf0) >> 4] |= 1 << (k & 0x0f);
}

static void _reset(uint8_t k) {
  _rows[(k & 0xf0) >> 4] &= ~(1 << (k & 0x0f));
}

static void pet_kdown(uint8_t asciicode) {
  if (asciicode < 0x80)
    _set(ascii2rowcol(asciicode));
}

static void pet_kup(uint8_t asciicode) {
  if (asciicode < 0x80)
    _reset(ascii2rowcol(asciicode));  
}


#ifdef HAS_NETWORK
// ****************************************
// TFTP server
// ****************************************
static int dummy_handle;
static uint16_t prg_add_start;
static uint16_t prg_add_cur;
static uint16_t prg_wr = 0;
static uint16_t prg_size = 0;
static bool prg_skip = false;

static void prg_write(uint8_t * src, int length )
{
  while (1)
  {
    if (prg_wr == 0)
    {
      prg_wr++;
      prg_add_start = *src++;
    }   
    else if (prg_wr == 1)
    {
      prg_wr++;
      prg_add_start = prg_add_start + (*src++ << 8);
      prg_add_cur = prg_add_start;
      printf("loading at %04x\n",prg_add_start);
    }   
    else
    {
      //printf("%02x\n",*src);
      petram[prg_add_cur++] = *src++;
    } 
    length  = length - 1;
    if ( length == 0) return;
  }
}

static void* tftp_open(const char* fname, const char* mode, u8_t is_write)
{
  printf("TFTP open: %s\n", fname);
  LWIP_UNUSED_ARG(mode);
  pet_running = false;  
  prg_start = false;
  prg_wr = 0;
  prg_size = 0;
  if (!strcmp(fname,"reset") ) 
  {
    pet_reset = true;
    prg_skip = true;
  }
  else if (!strcmp(fname,"key") ) 
  {
    pet_running = true;  
    prg_skip = true;
    input_chr = '1';
    //input_delay = INPUT_DELAY;
  }
  else 
  {
    prg_skip = false;
  }
  return (void*)&dummy_handle;
}

static void tftp_close(void* handle)
{
  printf("TFTP close\n");
  if (!prg_skip) 
  {
    uint8_t lo,hi;
    petram[0xc7] = petram[0x28];
    petram[0xc8] = petram[0x29];

    lo = (uint8_t)(prg_add_cur & 0xff);
    hi = (uint8_t)(prg_add_cur >> 8);
    petram[0x2a] = lo;
    petram[0x2c] = lo;
    petram[0x2e] = lo;
    petram[0xc9] = lo;
    petram[0x2b] = hi;
    petram[0x2d] = hi;
    petram[0x2f] = hi;
    petram[0xca] = hi;

    pet_running = true;
    prg_start = true;
    input_delay = INPUT_DELAY;
    input_pt = 0; 
    printf("prg size %d\n",prg_size);  
  }
  else 
  {
    prg_skip = false;
  }
}

static int tftp_read(void* handle, void* buf, int bytes)
{
  return 0;
}

static int tftp_write(void* handle, struct pbuf* p)
{
  if (!prg_skip) 
  {
    while (p != NULL) {
      prg_size += p->len;
      printf("TFTP write %d\n",p->len);
      prg_write((uint8_t *)p->payload,p->len);  
      p = p->next;
    }
  }
  return 0;
}

/* For TFTP client only */
static void tftp_error(void* handle, int err, const char* msg, int size)
{
  char message[100];

  LWIP_UNUSED_ARG(handle);

  memset(message, 0, sizeof(message));
  MEMCPY(message, msg, LWIP_MIN(sizeof(message)-1, (size_t)size));

  printf("TFTP error: %d (%s)", err, message);
}

static const struct tftp_context tftp = {
  tftp_open,
  tftp_close,
  tftp_read,
  tftp_write,
  tftp_error
};

static uint32_t init_server(void) 
{
  uint32_t ip = 0;
  if (cyw43_arch_init()) {
      printf("failed to initialise\n");
      return ip;
  }
#ifdef WIFI_AP
  cyw43_arch_enable_ap_mode("hyperpetpico", "picopet123", CYW43_AUTH_WPA2_MIXED_PSK );
  //CYW43_AUTH_WPA2_MIXED_PSK
  //CYW43_AUTH_OPEN
  printf("Connecting to WiFi...\n");
  struct netif *netif = netif_default;
  ip4_addr_t addr = { .addr = 0x017BA8C0 }, mask = { .addr = 0x00FFFFFF };
  ip = 0x017BA8C0; // 192.168.123.1
  printf("IP Address: %lu.%lu.%lu.%lu\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
  netif_set_addr(netif, &addr, &mask, &addr);
  //set_secondary_ip_address(0x006433c6);

  // Start the dhcp server
  static dhcp_server_t dhcp_server;
  dhcp_server_init(&dhcp_server, &netif->ip_addr, &netif->netmask, "picodomain");
#else
  cyw43_arch_enable_sta_mode();
  // this seems to be the best be can do using the predefined `cyw43_pm_value` macro:
  // cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
  // however it doesn't use the `CYW43_NO_POWERSAVE_MODE` value, so we do this instead:
  cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));
  printf("Connecting to WiFi...\n");

  int retry = 10;
  while (retry-- > 0) { 
    if (cyw43_arch_wifi_connect_timeout_ms("yourap", "yourpasswd", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect, retrying %d...\n",retry);
        sleep_ms(500);
    } else 
    {
        printf("Connected.\n");
        extern cyw43_t cyw43_state;
        auto ip_addr = cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr;
        printf("IP Address: %lu.%lu.%lu.%lu\n", ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, ip_addr >> 24);
        ip = ip_addr;
        break;
    }
  }  
#endif

  if (ip)
  {
    tftp_init_common(LWIP_TFTP_MODE_SERVER, &tftp);
  }  

  return ip; 
}
#endif
#endif


#ifdef HAS_PETIO
static bool resprev = true;

static bool repeating_timer_callback(struct repeating_timer *t) 
{
  bool resnext = gpio_get(CONFIG_PIN_PETBUS_RESET) != 0;
  // REST was high and becomes low
  if ( (resprev == true) && (!resnext) )
  {
    resprev = false;
    pet_reset = true;
  }
  else {
    resprev = resnext;
  }
  return true;
}
#endif


#ifndef HAS_PETIO
// ****************************************
// USB keyboard
// ****************************************
void kbd_raw_key_down (int code, int flags)
{
  switch (code)
  {
    case KBD_KEY_UP:
      break;
    case KBD_KEY_DOWN:
      break;
    default:
      char c = kbd_to_ascii (code, flags);
      break;
  }
}
#endif

// ****************************************
// Main
// ****************************************
#define VID_CNT 200

int main()
{
#ifdef HAS_PETIO
  struct repeating_timer timer;
#endif
  uint8_t xscrollslowdown=0;
  int vidchange_cnt = VID_CNT;
  uint8_t vid_mode = 0;

  stdio_init_all();
#ifndef HAS_PETIO
  usb_kbd_init();
#endif

#ifdef HAS_PETIO
  init_petpio();  
#else 
#ifdef HAS_NETWORK 
  init_server();
#endif
#endif

  playSID.begin();
  AudioInit(256, SND_Process);

  memset((void*)&Bitmap[0],0, sizeof(Bitmap));

  multicore_launch_core1(VgaCoreWithSound);  
  VideoSetup();

  VideoRenderInit();

#ifdef HAS_PETIO
  add_repeating_timer_ms(1, repeating_timer_callback, NULL, &timer); 
#else
  pet_start();
#endif

  // main loop
  while (true)
  {
    if (pet_reset)
    {
      pet_reset = false;
#ifndef HAS_PETIO 
      mos.Reset();
      pet_running = true;
#endif      
      VideoRenderInit();
    }

    WaitVSync();

#ifndef HAS_PETIO  
    if (pet_running)
    {
      pet_step();
      if (prg_start) 
      {
        if (input_delay != 0 ) 
        {
          input_delay--;
        }  
        else 
        {
          if (xscrollslowdown == 0)
          {
            input_chr = input_cmd[input_pt] & 0x7f;
            if (input_chr != 0) 
            {
              pet_kdown( input_chr);
              //printf("d %d\n", input_chr);
            }  
          }
          else if (xscrollslowdown == 2)
          {
            if (input_chr != 0) 
            {
              input_pt++;
              pet_kup( input_chr);
              input_chr = 0;
              //printf("u %d\n", input_chr);
            }  
          }
        }
      }
      else 
      {
        /*
        if (input_delay != 0 ) 
        {
          input_delay--;
        }  
        else
        */  
        {
          if (xscrollslowdown == 0)
          {
            if (input_chr != 0) 
            {
              pet_kdown( input_chr);
            }  
          }
          else if (xscrollslowdown == 2)
          {
            if (input_chr != 0) 
            {
              pet_kup( input_chr);
              input_chr = 0;
            }  
          }       
        }                 
      }  
    }
#endif
    xscrollslowdown+=1;
    xscrollslowdown&=7;

    //sleep_ms(500);
    sid_dump();    
    
    handleCmdQueue();

/*
    mem[REG_SPRITE_XLO] += 1;

    if ( LAYER_L0_ENA )
    {
      uint16_t scroll = GET_XSCROLL_L0;
      if (scroll++ == screen_width) scroll = 0;
      SET_XSCROLL_L0(scroll);      
    }
*/

/*
    if ( LAYER_L1_ENA )
    {
      if (xscrollslowdown == 0) 
      {
        uint16_t scroll = GET_XSCROLL_L1;
        if (scroll++ == screen_width) scroll = 0;
        SET_XSCROLL_L1(scroll);
      }
    }
*/
#ifndef HAS_PETIO
    usb_kbd_scan();  
#endif
  }
}



