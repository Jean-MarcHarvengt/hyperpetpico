//#include "global.h"
#include "gfx_output.h"
#include "petbus.h"
#include "hardware/clocks.h"

#ifdef ISRP2040 
#include "vga_vmode.h"
#include "vga.h"
#include "pico/stdlib.h"
#endif
#ifdef ISRP2350 
#include "hdmi.h"
#endif

#ifdef ISRP2040 
// Video mode
#ifdef SIXTYHZ
#define VideoMod VideoVGA
#else
#define VideoMod VideoPALp
#endif
//#define VideoMod VideoEGA

// Video configurations
static sVmode Vmode0; // videomode setup
static sVgaCfg Cfg0;  // required configuration
static sVmode Vmode1;
static sVgaCfg Cfg1;
static sVmode Vmode2;
static sVgaCfg Cfg2;
#endif

uint8_t video_mode;
uint16_t screen_height = 200;
uint16_t screen_width;


void GfxCoreWithSound(void)
{
#ifdef ISRP2040 
  VgaCore();
#endif
#ifdef ISRP2350 
  HdmiCore();
#endif
}

void GfxWaitVSync(void)
{
#ifdef ISRP2040 
  WaitVSync();
#endif  
#ifdef ISRP2350
  HdmiVSync(); 
#endif  
}

bool GfxIsVSync(void)
{
#ifdef ISRP2350
  return HdmiIsVSync(); 
#endif   
}

/*
int GfxVCount(void)
{
#ifdef ISRP2350
  return HdmiVCount(); 
#endif
#ifdef ISRP2040
  return 0;
#endif
}
*/

void GfxVideoInit(uint8_t mode, bool firstTime)
{
#ifdef ISRP2040   
  // initialize base layer 0
  switch (mode) {
    case 0: // 640x200
      screen_width = 640;
      // initialize videomode
      if (firstTime) {
        set_sys_clock_pll(Vmode0.vco*1000, Vmode0.pd1, Vmode0.pd2);
        VgaInitReq(&Vmode0);
      }
      else VgaInit(&Vmode0);
      break;
    case 1: // 320x200
      screen_width = 320;
      // initialize videomode
      if (firstTime) {
        set_sys_clock_pll(Vmode1.vco*1000, Vmode1.pd1, Vmode1.pd2);
        VgaInitReq(&Vmode1);
      }
      else VgaInit(&Vmode1);
      break;
    case 2: // 256x200
      screen_width = 256;
      // initialize videomode
      if (firstTime) {
        set_sys_clock_pll(Vmode2.vco*1000, Vmode2.pd1, Vmode2.pd2);
        VgaInitReq(&Vmode2);
      }
      else VgaInit(&Vmode2);
    default:
      break; 
  }
#endif
#ifdef ISRP2350
  // initialize base layer 0
  switch (mode) {
    case 0: // 640x200
      screen_width = 640;
      break;
    case 1: // 320x200
      screen_width = 320;
      break;
    case 2: // 256x200
      screen_width = 256;
    default:
      break; 
  }
  HdmiInit(mode);
#endif    
  video_mode = mode;
  SET_XSCROLL_L0(0);
  SET_XSCROLL_L1(0);
}

void GfxVideoSetup(void)
{
#ifdef ISRP2040 
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
#endif  
  GfxVideoInit(video_mode, true);
  SET_VIDEO_MODE(video_mode);
}


