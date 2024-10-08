#ifndef PETBUS_H
#define PETBUS_H

#include "global.h"
#include "stdint.h"

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
// 8000-87ff: videomem standard petfont text map in L1 (fmap1)
//            (83ff for 4032, 87ff for 8032) 
// 8800-8fff: videomem expanded tiles map in L1 (tmap1)
// 9000-97ff: videomem expanded tiles map in L0 (tmap0)
//            tile id 0-255 in 8x8   tile mode
//            tile id 0-63  in 16x16 tile mode
//
// 9800-99ff: MAX 128 sprites registers (id,xhi,xlo,y)
//       id:    0-5 (max 63)
//       hflip: 6
//       vflip: 7
//
// 9a00-9aff: transfer lookup (if used as color palette, 256 RGB332 colors)
//       RGB332
//       R: 5-7, 0x20 -> 0xe0   xxx --- --
//       G: 2-4, 0x04 -> 0x1c   --- xxx -- 
//       B: 0-1, 0x00 -> 0x03   --- --- xx    
//
// 9b00: video mode 
//       0-1: resolution (0=640x200,1=320x200,2=256x200)
//
// 9b01: background color (RGB332)
//
// 9b02: layers config
//       0: L0 on/off (1=on)
//       1: L1 on/off (1=on)
//       2: L2 on/off (1=on)
//       3: L2 sprites between L0 and L1 (0 = sprites top)
//       4: bitmap/tile in L0 (0=bitmap)
//       5: petfont/tile in L1 (0=font)
//       6: enable scroll area in L0
//       7: enable scroll area in L1
//
// 9b03: lines config 2
//       0: single/perline background color
//       1: single/perline L0 xscroll
//       2: single/perline L1 xscroll
//
// 9b04: xscroll HI registers
//       3-0: L0 xscroll HI
//       7-4: L1 xscroll HI
// 9b05: L0 xscroll LO
// 9b06: L1 xscroll LO
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
//
// 9b0d: foreground color (RGB332)
//
// 9b0e: tiles config
//       0: L0: 0=8x8, 1=16x16
//       1: L1: 0=8x8, 1=16x16
//       2-4: xcurtain
//            0: on/off
//            1: 8/16 pixels left
//       5-7: ycurtain
//            0: on/off
//            1: 8/16 pixels top
//
// 9b0f: vsync line (0-200, 200 is overscan)
//
// 9b10: 3-0: transfer mode (WR)
//       1/2/4/8 bits per pixel (using indexed CLUT)
//       9 = 8 bits RGB332 no CLUT
//       0 = compressed
// 9b11: transfer command (WR)
//       0: idle
//       1: transfer tiles data      (data=tilenr,w,h,packet pixels)
//       2: transfer sprites data    (data=spritenr,w,h,packet pixels)
//       3: transfer bitmap data     (data=xh,xl,y,wh,wl,h,w*h/packet pixels) 
//       4: transfer t/fmap col data (data=layer,col,row,size,size/packet tiles)
//       5: transfer t/fmap row data (data=layer,col,row,size,size/packet tiles)
//       6: transfer all tile 8bits data compressed (data=sizeh,sizel,pixels)
//       7: transfer all sprite 8bits data compressed (data=sizeh,sizel,pixels)
//       8: transfer bitmap 8bits data compressed (data=sizeh,sizel,pixels)
//       9: transfer font 1bit data, 8bits a time (data=sizeh,sizel,pixels)  
//
// 9b12: transfer params (WR)
// 9b13: transfer data   (RD/WR)
// 9b14: transfer status (RD) 1=ready
//
// Redefining tiles/sprite sequence
// 1. write lookup palette entries needed
// 2. write transfer mode (1/2/4/8/9)
// 3. write command 1/2
// 4. write params tile/sprite id,w,h
// 5. write data sequence (8bytes*plane for tiles, (h*2)bytes*plane for sprites)
// (any new command to reset)
//
// Transfer bitmap sequence
// 1. write lookup palette entries needed
// 2. write transfer mode (1/2/4/8/9)
// 3. write command 3
// 4. write params XH,XL,Y,WH,WL,H
// 5. write data sequence (bytes*plane /packed_bits)
// (any new command to reset)
//
// 9b15: PAL/NTSC (for PAL machine only)
// 0 = PAL
// 1 = NTSC
//
// 9b18-9b37: 32 (re)SID registers (d400 on C64)

// 9b38-9bff: lines background color (RGB332)
// 9c00-9cc7: 7-4:  lines L1 xscroll HI, 3-0: L0 xscroll HI
// 9cc8-9d8F: lines L0 xscroll LO
// 9d90-9e58: lines L1 xscroll LO
//
// sprite collision only for first 16 sprites against all
// 8bits x 128 entries
// 9f00-9f7f: Sprite collision LO (first 8 sprites, bit0 = sprite 0)
// 9f80-9fff: Sprite collision HI (last  8 sprites, bit0 = sprite 8)
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
#define REG_VSYNC         (0x9b0f - 0x8000) // 39695

#define REG_TDEPTH        (0x9b10 - 0x8000) // 39696
#define REG_TCOMMAND      (0x9b11 - 0x8000) // 39697
#define REG_TPARAMS       (0x9b12 - 0x8000) // 39698
#define REG_TDATA         (0x9b13 - 0x8000) // 39699
#define REG_TSTATUS       (0x9b14 - 0x8000) // 39700
#define REG_PALNTSC       (0x9b15 - 0x8000) // 39700

#define REG_SID_BASE      (0x9b18 - 0x8000) // 39704

#define REG_LINES_BG_COL  (0x9b38 - 0x8000) // 39736
#define REG_LINES_XSCR_HI (0x9c00 - 0x8000) // 39936
#define REG_LINES_L0_XSCR (0x9cc8 - 0x8000) // 40136
#define REG_LINES_L1_XSCR (0x9d90 - 0x8000) // 40336
#define REG_SPRITE_COL_LO (0x9f00 - 0x8000) // 40704
#define REG_SPRITE_COL_HI (0x9f80 - 0x8000) // 40832

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
#define GET_TSTATUS       ( mem[REG_TSTATUS] )

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

#define L0_TILE_16_ENA    ( mem[REG_TILES_CFG]  & 0x01 )
#define L1_TILE_16_ENA    ( mem[REG_TILES_CFG]  & 0x02 )
#define HCURTAIN8_ENA     ( (mem[REG_TILES_CFG] & (0x04+0x08)) == (0x04) )
#define HCURTAIN16_ENA    ( (mem[REG_TILES_CFG] & (0x04+0x08)) == (0x04+0x08) )
#define VCURTAIN8_ENA     ( (mem[REG_TILES_CFG] & (0x20+0x40)) == (0x20) )
#define VCURTAIN16_ENA    ( (mem[REG_TILES_CFG] & (0x20+0x40)) == (0x20+0x40) )

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

extern unsigned char mem[0x2000];
#ifdef PETIO_A000
extern unsigned char mem_a000[0x1000];
#endif
#ifdef PETIO_EDIT
extern unsigned char mem_e000[0x0800];
#endif
extern bool font_lowercase;

#define CMD_QUEUE_SIZE 256
typedef struct {
   uint8_t  id;
   uint8_t  p8_1;
   uint16_t p16_1;
} QueueItem;

#define MAX_CMD 32
#define MAX_PAR 32
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
  cmd_transfer_font=9,

  cmd_tiles_clr=11,
  cmd_bitmap_clr=12,
  cmd_bitmap_point=13,
  cmd_bitmap_rect=14,
  cmd_bitmap_tri=15,
  
  cmd_openfile=27,
  cmd_readfile=28,
  cmd_opendir=29,
  cmd_readdir=30,
  cmd_unused=31,
} Cmd;


#ifdef HAS_PETIO
extern void petbus_init(void);
extern void petbus_loop(void);
extern bool petbus_poll_reset(void);
#endif

#endif
