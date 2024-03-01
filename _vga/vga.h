
// ****************************************************************************
//
//                                 VGA output
//
// ****************************************************************************

#ifndef _VGA_H
#define _VGA_H

// fill memory buffer with u32 words
//  buf ... data buffer, must be 32-bit aligned
//  data ... data word to store
//  num ... number of 32-bit words (= number of bytes/4)
// Returns new destination address.
extern "C" u32* RenderColor(u32* buf, u32 data, int num);

// Blit scanline using font + color
//  dst:		destination buffer
//  src:		source buffer (char buffer)
//  w:			width in chars
//  color:		fgcolor
//  fontdef:	font definition
//  scroll:		scrollx
extern "C" void TextBlitKey32(u8* dst, u8* src, int w, u8 fgcolor, u8* fontdef, int scroll, int offset);
extern "C" void TextBlitKey40(u8* dst, u8* src, int w, u8 fgcolor, u8* fontdef, int scroll, int offset);
extern "C" void TextBlitKey80(u8* dst, u8* src, int w, u8 fgcolor, u8* fontdef, int scroll, int offset);

// Blit scanline using font + color
//  dst:		destination buffer
//  src:		source buffer (char buffer)
//  w:			width in chars
//  fgcolorlut:	fgcolor 4bits lookup table
//  fontdef:	font definition
//  scroll:		scrollx
extern "C" void TextBlit32(u8* dst, u8* src, int w, u8 * fgcolorlut, u8* fontdef, int scroll, int offset);
extern "C" void TextBlit40(u8* dst, u8* src, int w, u8 * fgcolorlut, u8* fontdef, int scroll, int offset);
extern "C" void TextBlit80(u8* dst, u8* src, int w, u8 * fgcolorlut, u8* fontdef, int scroll, int offset);


// Blit scanline using 8x8 or 16x16 tile (+ bgcolor)
//  dst:		destination buffer
//  src:		source buffer (tile buffer)
//  w:			width in tiles
//  bgcolor:	background color
//  tiledef:	tile definition
//  scroll:		scrollx
//  offset:		offset
extern "C" void TileBlitKey8_32(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlitKey8_40(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlitKey8_80(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);

extern "C" void TileBlit8_32(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlit8_40(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlit8_80(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);

extern "C" void TileBlitKey16_16(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlitKey16_20(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);
extern "C" void TileBlitKey16_40(u8* dst, u8* src, int w, u8 bgcolor, u8* tiledef, int scroll, int offset);

// Blit scanmine sprites (16x24)
//  sprnb:		number of sprites
//  w:			width in pixels
//  scanline:	current scanline
//  sprdata:	source buffer (sprite data buffer)
//  sprdef:	    sprite definition base
//  dst:		destination buffer
extern "C" void Sprite16(int sprnb, int w, int scanline, u8* sprdata, u8* sprdef, u8* dst);

// key blit scanline from source
//  dst: 		destination buffer
//  src:		source buffer
//  w:			width in pixels
//  scroll:     scrollx
//  offset:     offset  
extern "C" void LineBlitKey32(u8* dst, u8* src, int w, int scroll, int offset);
extern "C" void LineBlitKey40(u8* dst, u8* src, int w, int scroll, int offset);
extern "C" void LineBlit80(u8* dst, u8* src, int w, int scroll, int offset);


// initialize videomode (returns False on bad configuration)
// - All layer modes must use same layer program (LAYERMODE_BASE = overlapped layers are OFF)
void VgaInit(const sVmode* vmode); //, u8 layer1mode=LAYERMODE_BASE, u8 layer2mode=LAYERMODE_BASE, u8 layer3mode=LAYERMODE_BASE);

// VGA core
void VgaCore();

// request to initialize VGA videomode, NULL=only stop driver (wait to initialization completes)
void VgaInitReq(const sVmode* vmode);

// execute core 1 remote function
void Core1Exec(void (*fnc)());

// check if core 1 is busy (executing remote function)
Bool Core1Busy();

// wait if core 1 is busy (executing remote function)
void Core1Wait();

// wait for VSync scanline
void WaitVSync();
void WaitScanline(int scanline);

// get scanline
int GetScanline();

#endif // _VGA_H
