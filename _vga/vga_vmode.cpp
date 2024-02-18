
// ****************************************************************************
//
//                                 VGA videomodes
//
// ****************************************************************************

#include "include.h"
// align to multiple of 4
#define ALIGN4(x) ((x) & ~3)

/*
http://martin.hinner.info/vga/pal.html

VGA system (525 lines total):
time 0:
- line 1, 2: (2) vertical sync
- line 3..35: (33) dark
- line 36..515: (480) image lines 0..479
- line 516..525: (10) dark
*/


// TV PAL progressive 5:4 360x288 (4:3 384x288, 16:9 512x288)
const sVideo VideoPALp = {
	// horizontal (horizontal frequency 15625 Hz)
	.htot=   64.00000f,	// total scanline in [us]
	.hfront=  1.65000f,	// H front porch (after image, before HSYNC) in [us]
	.hsync=   4.70000f,	// H sync pulse in [us]
	.hback=   5.70000f,	// H back porch (after HSYNC, before image) in [us]
	.hfull=  47.36000f,	// H full visible in [us] (formally should be 51.95 us) 

	// vertical (vertical frequency 50 Hz)
	.vtot=312,		// total scanlines (both subframes)
	.vmax=288,		// maximal height

	// subframe 1
	.vsync=2,		// V sync (half-)pulses
	.vback=18+23+2,	// V back porch (after VSYNC, before image)
	.vact=240,		// active visible scanlines, subframe 1 (formally should be 288, 576 total)
	.vfront=24+3,		// V front porch (after image, before VSYNC)

	// flags
	.psync=False,		// positive synchronization
//	.odd=True 		// first sub-frame is odd lines 1, 3, 5,... (PAL)
};


// EGA 8:5 640x400 (5:4 500x400, 4:3 528x400, 16:9 704x400), vert. 70 Hz, hor. 31.4685 kHz, pixel clock 25.175 MHz
const sVideo VideoEGA = {
	// horizontal
	.htot=   31.77781f,	// total scanline in [us]
	.hfront=  0.63556f,	// H front porch (after image, before HSYNC) in [us]
	.hsync=   3.81334f,	// H sync pulse in [us]
	.hback=   1.90667f,	// H back porch (after HSYNC, before image) in [us]
	.hfull=  25.42224f,	// H full visible in [us]

	// vertical
	.vtot=449,		// total scanlines (both subframes)
	.vmax=400,		// maximal height

	// subframe
	.vsync=2,		// V sync (half-)pulses
	.vback=35,		// V back porch (after VSYNC, before image)
	.vact=400,		// active visible scanlines
	.vfront=12,		// V front porch (after image, before VSYNC)

	// flags
	.psync=False,		// positive synchronization
};

// VGA 4:3 640x480 (16:9 848x480), vert. 60 Hz, hor. 31.4685 kHz, pixel clock 25.175 MHz
const sVideo VideoVGA = {
	// horizontal
	.htot=   31.77781f,	// total scanline in [us] (800 pixels)
	.hfront=  0.63556f,	// H front porch (after image, before HSYNC) in [us] (16 pixels)
	.hsync=   3.81334f,	// H sync pulse in [us] (96 pixels)
	.hback=   1.90667f,	// H back porch (after HSYNC, before image) in [us] (48 pixels)
	.hfull=  25.42224f,	// H full visible in [us] (640 pixels)

	// vertical
	.vtot=525,		// total scanlines (both subframes)
	.vmax=480,		// maximal height

	// subframe
	.vsync=2,		// V sync (half-)pulses
	.vback=33,		// V back porch (after VSYNC, before image)
	.vact=480,		// active visible scanlines
	.vfront=10,		// V front porch (after image, before VSYNC)

	// flags
	.psync=False,		// positive synchronization
};




// initialize default VGA configuration
void VgaCfgDef(sVgaCfg* cfg)
{
	cfg->width = 640;		// width in pixels
	cfg->height = 480;		// height in lines
	cfg->wfull = 0;			// width of full screen, corresponding to 'hfull' time (0=use 'width' parameter)
	cfg->video = &VideoVGA;		// used video timings
	cfg->freq = 120000;		// required minimal system frequency in kHz (real frequency can be higher)
	cfg->fmax = 270000;		// maximal system frequency in kHz (limit resolution if needed)
	cfg->mode[0] = LAYERMODE_BASE;	// modes of overlapped layers 0..3 LAYERMODE_* (LAYERMODE_BASE = layer is off)
	cfg->dbly = False;		// double in Y direction
	cfg->lockfreq = False;		// lock required frequency, do not change it
}


// calculate videomode setup
//   cfg ... required configuration
//   vmode ... destination videomode setup for driver
void VgaCfg(const sVgaCfg* cfg, sVmode* vmode)
{
	int i;

	// prepare layer program, copy layer modes
	u8 prog = LAYERMODE_BASE;
	vmode->mode[0] = prog;
	vmode->prog = prog;

	// prepare minimal and maximal clocks per pixel
	int mincpp = 2; //LayerMode[LAYERMODE_BASE].mincpp;
	int maxcpp = 17; //LayerMode[LAYERMODE_BASE].maxcpp;
	int cpp;

	// prepare full width
	int w = cfg->width; // required width
	int wfull = cfg->wfull;	// full width
	if (wfull == 0) wfull = w; // use required width as 100% width

	// prepare maximal active time and maximal pixels
	const sVideo* v = cfg->video;
	float hmax = v->htot - v->hfront - v->hsync - v->hback;
	float hfull = v->hfull;
	int wmax = (int)(wfull*hmax/hfull + 0.001f);

	// calculate cpp from required frequency (rounded down), limit minimal cpp
	u32 freq = cfg->freq;
	cpp = (int)(freq*hfull/1000/wfull + 0.1f);
	if (cpp < mincpp) cpp = mincpp;

	// recalculate frequency if not locked
	if (!cfg->lockfreq)
	{
		int freq2 = (int)(cpp*wfull*1000/hfull + 0.5f) + 200;
		if (freq2 < freq)
		{
			cpp++;
			freq2 = (int)(cpp*wfull*1000/hfull + 0.5f) + 200;
		}
		if (freq2 >= freq) freq = freq2;
		if (freq > cfg->fmax) freq = cfg->fmax;
	}

	// find sysclock setup (use set_sys_clock_pll to set sysclock)
	u32 vco;
	u16 fbdiv;
	u8 pd1, pd2;
	FindSysClock(freq, &freq, &vco, &fbdiv, &pd1, &pd2);

	vmode->freq = freq;
	vmode->vco = vco;
	vmode->fbdiv = fbdiv;
	vmode->pd1 = pd1;
	vmode->pd2 = pd2;

	// calculate divisor
	cpp = (int)(freq*hfull/1000/wfull + 0.2f);
	int div = 1;
	while (cpp > maxcpp)
	{
		div++;
		cpp = (int)(freq*hfull/1000/wfull/div + 0.2f);
	}

	vmode->div = div;
	vmode->cpp = cpp;

	// calculate new full resolution and max resolution
	wfull = (int)(freq*hfull/1000/cpp/div + 0.4f);
	wmax = (int)(freq*hmax/1000/cpp/div + 0.4f);

	// limit resolution
	if (w > wmax) w = wmax;
	w = ALIGN4(w);
	vmode->width = w; // active width
	vmode->wfull = wfull; // width of full screen (image should be full visible)
	vmode->wmax = wmax; // maximal width (can be > wfull)

	// horizontal timings
	int hwidth = w*cpp; // active width in state machine clocks
	int htot = (int)(freq*v->htot/1000/div + 0.5f);  // total state machine clocks per line
	int hsync = (int)(freq*v->hsync/1000/div + 0.5f); // H sync pulse in state machine clocks (min. 4)

	if (hsync < 4)
	{
		htot -= 4 - hsync;
		hsync = 4;
	}

	int hfront = (int)(freq*v->hfront/1000/div + 0.5f); // H front porch in state machine clocks (min. 2)
	int hback = (int)(freq*v->hback/1000/div + 0.5f); // H back porch in state machine clocks (min. 13)
	int d = htot - hfront - hsync - hback - hwidth; // difference
	hfront += d/2;
	hback += (d < 0) ? (d-1)/2 : (d+1)/2;

	if (hfront < 4)
	{
		hback -= 4 - hfront;
		hfront = 4;
	}

	if (hback < 13)
	{
		hfront -= 13 - hback;
		hback = 13;

		if (hfront < 2) hfront = 2;
	}

	htot = hfront + hsync + hback + hwidth; // total state machine clocks per line

	vmode->htot = (u16)htot; // total state machine clocks per line
	vmode->hfront = (u16)hfront; // H front porch in state machine clocks (min. 2)
	vmode->hsync = (u16)hsync; // H sync pulse in state machine clocks (min. 4)
	vmode->hback = (u16)hback; // H back porch in state machine clocks (min. 13)

	// vertical timings
	int h = cfg->height; // required height
	if (cfg->dbly) h *= 2; // use double lines
	vmode->vmax = v->vmax; // maximal height
	if (h > v->vmax) h = v->vmax; // limit height
	if (cfg->dbly) h &= ~1; // must be even number if double lines

	int vact = h;	// active lines in progress mode

	if (cfg->dbly) h /= 2; // return double lines to single lines
	vmode->height = h;

	// vertical timings
	vmode->vtot = v->vtot; // total scanlines

	vmode->vact = vact; // active scanlines 
	int dh = vact - v->vact; // difference
	vmode->vsync = v->vsync; // V sync (half-)pulses
	vmode->vback = v->vback - dh/2; // V back porch (after VSYNC, before image)
	vmode->vfront = v->vfront - ((dh < 0) ? (dh-1)/2 : (dh+1)/2); // V front porch (after image, before VSYNC)

	// frequency
	vmode->hfreq = vmode->freq * 1000.0f / vmode->div / vmode->htot;
	vmode->vfreq = vmode->hfreq / vmode->vtot;

	// flags
	vmode->lockfreq = cfg->lockfreq; // lock current frequency, do not change it
	vmode->dbly = cfg->dbly; // double scanlines
	vmode->psync = v->psync; // positive synchronization

	// first active scanline
	vmode->vfirst = vmode->vsync + vmode->vback + 1;
}



