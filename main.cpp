// ****************************************************************************
//
//                                 Main code
//
// ****************************************************************************
#include "include.h"
#include "petfont.h"
#include "reSID.h"

#include "pwm_audio.h"
#include "petbus.h"
#ifndef HAS_PETIO
#include "mos6502.h"
#include "basic4_b000.h"
#include "basic4_c000.h"
#include "basic4_d000.h"
#include "edit4.h"
#include "edit480.h"
#include "kernal4.h"
#ifdef HAS_NETWORK
#include "lwip/apps/tftp_server.h"
#include "network.h"
#endif
#include "bsp/board.h"
#include "tusb.h"
#include "kbd.h"
extern "C" void cdc_task(void);
extern "C" void hid_app_task(void);
#endif
#include "decrunch.h"

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


// ****************************************
// Audio code
// ****************************************
static AudioPlaySID playSID;
static u8 prev_sid_reg[26];

void __not_in_flash("AudioRender") AudioRender(void)
{
#ifdef AUDIO_CBACK
  pwm_audio_handle_sample();
#endif
}

static void audio_fill_buffer( audio_sample * stream, int len )
{
  playSID.update(SOUNDRATE, (void *)stream, len);
}


static void sid_dump( void )
{
  //memcpy((void *)&buffer[0], (void *)&mem[REG_SID_BASE], 26);
  for(int i=0;i<25;i++) 
  {
    u8 reg = mem[REG_SID_BASE+i];
    if(reg != prev_sid_reg[i]) {       
        playSID.setreg(i, reg);
        prev_sid_reg[i] = reg;                  
    } 
  }
 
}

// ****************************************
// Setup Video mode
// ****************************************
static void VgaCoreWithSound()
{
  VgaCore();
}

static void ResetGFXMem(void) 
{
  memset((void*)&Bitmap[0],0, sizeof(Bitmap));
  memset((void*)&TileData[0],0, sizeof(TileData));
  memset((void*)&SpriteData[0],0, sizeof(SpriteData));
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

    // sprite collision for first 8 sprites
    for (int i = 0; i < SPRITE_NUM_MAX; i++)
    {
      uint16_t x1 = (mem[REG_SPRITE_XHI+i]<<8)+mem[REG_SPRITE_XLO+i];    
      uint8_t y1 = mem[REG_SPRITE_Y+i]; 
      uint8_t colbits = 0;      
      for (int k = 0; k < 8; k++)
      {
        uint16_t x = (mem[REG_SPRITE_XHI+k]<<8)+mem[REG_SPRITE_XLO+k];    
        uint8_t y = mem[REG_SPRITE_Y+k];
        if ( (x >= x1) && (x < (x1+SPRITEW)) && (y >= y1) && (y < (y1+SPRITEH)) )
        {
          colbits += (1<<k);
        }  
      }
      mem[REG_SPRITE_COLI+i] = colbits; 
    }
#ifndef HAS_PETIO
//  multicore_fifo_push_blocking(1);
#endif
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
#ifndef HAS_PETIO
//  multicore_fifo_push_blocking(0);  
#endif
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
  SET_LAYER_MODE( LAYER_L0_TILE | LAYER_L1_PETFONT | LAYER_L2_SPRITE | LAYER_L2_INBETW );
//  SET_LAYER_MODE( LAYER_L0_BITMAP | LAYER_L1_PETFONT | LAYER_L2_SPRITE );
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

#define PET_CYCLES 16600 //9000

static void pet_step(void) 
{
  mos.Run(PET_CYCLES);
  mos.IRQ();
}

static void pet_line(void) 
{
  mos.Run(PET_CYCLES/320);
}

static void pet_remaining(void) 
{
  mos.Run((PET_CYCLES/320)*120);
  mos.IRQ();
}

static uint8_t petcol = 0;
static void core0_sio_irq() {
  irq_clear(SIO_IRQ_PROC0);
  while(multicore_fifo_rvalid()) {
    uint16_t raw = multicore_fifo_pop_blocking();
    if (pet_running)
    {    
      if (raw == 0) {
        petcol = petcol + 1;
        pet_line();
      }  
      else {
        petcol = 0;
        pet_remaining();
      }  
      //mem[REG_BG_COL] = petcol;
    }  
  } 
  multicore_fifo_clear_irq();
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
#endif


#ifndef HAS_PETIO
// ****************************************
// USB keyboard
// ****************************************
char kbd_to_ascii (int code, int flags)
{
  // TODO -- I'm not sure how we handle 8-bit key codes, if there 
  //   are any. I'm sure some keyboards must generate them (or, at least,
  //   there scancode-to-keycode mappings do. At present, we only
  //   support US keyboards. Virtual keys like 'up' and 'F1' also
  //   generate codes > 127, and these also cannot meaningfully be
  //   converted.
  if (code > 127) return 0;
  
  // We have more work to do here. What about shift-ctrl, shift-alt, etc?
  if (flags & KBD_FLAG_CONTROL)
     return (code & ~0x60);

  return code; 
}

void kbd_signal_raw_key (int code, int flags, int pressed)
{
  if (pressed == KEY_PRESSED)
  {
      pet_kdown(toupper(kbd_to_ascii (code, flags)));
      //printf("kdown %c\r\n", kbd_to_ascii (code, flags));
  }
  else 
  {
      pet_kup(toupper(kbd_to_ascii (code, flags)));
      //printf("kup %c\r\n", kbd_to_ascii (code, flags));
  }
}
#endif

#ifdef HAS_PETIO
static void __not_in_flash("pet_mem_write") pet_mem_write(uint16_t address, uint8_t value) {
//static void pet_mem_write(uint16_t address, uint8_t value) {
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
#endif

#ifndef HAS_PETIO
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
#endif
#endif



// ****************************************
// Main
// ****************************************
int main()
{
#ifndef HAS_PETIO
  uint8_t slowdown=0;
#endif

  stdio_init_all();

#ifdef HAS_PETIO
  petbus_init(pet_mem_write);  
#else
#ifdef HAS_NETWORK 
  printf("Init Wifi...\n");
  uint32_t ip = wifi_init();
  if (ip)
  {
    tftp_init_common(LWIP_TFTP_MODE_SERVER, &tftp);
  }   
#endif
  printf("Init USB...\n");
  
  board_init();
  printf("TinyUSB Host HID Controller Example\r\n");
  printf("Note: Events only displayed for explicit supported controllers\r\n");
  // init host stack on configured roothub port
  tuh_init(BOARD_TUH_RHPORT);
#endif

  memset((void*)&Bitmap[0],0, sizeof(Bitmap));

#ifndef HAS_PETIO
  printf("Init Video...\n");
#endif
  multicore_launch_core1(VgaCoreWithSound);  
  VideoSetup();
  //screen_width = 640;
  VideoRenderInit();

#ifndef HAS_PETIO
  printf("Init Audio...\n");
#endif
  //31500/60=525,31500/50=630 samples per frame
  //22050/60=367,22050/50=441 samples per frame
  pwm_audio_init(2048, audio_fill_buffer);
  playSID.begin(SOUNDRATE, 1024);

#ifndef HAS_PETIO
  printf("Init Emu...\n");
  pet_start();
  multicore_fifo_clear_irq();
//  irq_set_exclusive_handler(SIO_IRQ_PROC0,core0_sio_irq);   
//  irq_set_enabled(SIO_IRQ_PROC0,true);  
#endif

  // main loop
  while (true)
  {
#ifdef HAS_PETIO  
    if (petbus_poll_reset())
    {
#else
    if (pet_reset)
    {
      pet_reset = false;
      mos.Reset();
      pet_running = true;
#endif
      VideoRenderInit();
      pwm_audio_reset();
    }

#ifdef HAS_PETIO  
    WaitVSync();
#else
    WaitVSync();
    // tinyusb host task
    tuh_task();

#if CFG_TUH_CDC
    cdc_task();
#endif

#if CFG_TUH_HID
    hid_app_task();
#endif
    
    if (pet_running)
    {
      pet_step();
//mem[REG_BG_COL] = 0x00;
      if (prg_start) 
      {
        if (input_delay != 0 ) 
        {
          input_delay--;
        }  
        else 
        {
          if (slowdown == 0)
          {
            input_chr = input_cmd[input_pt] & 0x7f;
            if (input_chr != 0) 
            {
              pet_kdown( input_chr);
              //printf("d %d\n", input_chr);
            }  
          }
          else if (slowdown == 2)
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
          if (slowdown == 0)
          {
            if (input_chr != 0) 
            {
              pet_kdown( input_chr);
            }  
          }
          else if (slowdown == 2)
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
    slowdown+=1;
    slowdown&=7;
#endif
//mem[REG_BG_COL] = 0xff;
    sid_dump();
    pwm_audio_handle_buffer();
    handleCmdQueue();
//mem[REG_BG_COL] = 0x00;
//    __wfi();    
  }
}



