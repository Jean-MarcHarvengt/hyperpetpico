// ****************************************************************************
//                                 
//                           Global common definitions
//
// ****************************************************************************
// defined in CMakeList.txt by target
//#define HAS_PETIO       1  	// PET extension (or standalone emu)
//#define HAS_NETWORK     1     // enable network wifi (standalone mode emu only)  

#define SIXTYHZ         1     // 60Hz mode
#define EIGHTYCOL       1     // 80 column mode
#define WIFI_AP         1     // WIFI as access point (preferred!)

#define AUDIO_8BIT      1     // audio samples are 8bits

//#define AUDIO_CBACK     1     // handle audio samples from VGA line callback
//#define AUDIO_IRQ       1     // handle audio samples from IRQ
//#define AUDIO_1DMA      1     // handle audio samples with 1 DMA
#define AUDIO_3DMA      1     // handle audio samples with 3 DMA

#ifdef AUDIO_1DMA
#undef AUDIO_8BIT
#endif

//#define RETROVGA        1
#ifdef RETROVGA 
#define AUDIO_PIN       9
#else
#define AUDIO_PIN       21
#endif
#define VGA_DMA_CHANNEL 2 // requires 2 channels
#define AUD_DMA_CHANNEL 4 // requires 1 or 3 channels

// ----------------------------------------------------------------------------
//                              Base data types
// ----------------------------------------------------------------------------
typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed long int s32;
typedef unsigned long int u32;
typedef signed long long int s64;
typedef unsigned long long int u64;
typedef unsigned int uint;
typedef unsigned char Bool;
#define True 1
#define False 0

// NULL
#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

// I/O port prefix
#define __IO	volatile

// request to use inline
#define INLINE __attribute__((always_inline)) inline

// avoid to use inline
#define NOINLINE __attribute__((noinline))

// weak function
#define WEAK __attribute__((weak))

// align array to 4-bytes
#define ALIGNED __attribute__((aligned(4)))


// ----------------------------------------------------------------------------
//                               Constants
// ----------------------------------------------------------------------------
#define	B0 (1<<0)
#define	B1 (1<<1)
#define	B2 (1<<2)
#define	B3 (1<<3)
#define	B4 (1<<4)
#define	B5 (1<<5)
#define	B6 (1<<6)
#define	B7 (1<<7)
#define	B8 (1U<<8)
#define	B9 (1U<<9)
#define	B10 (1U<<10)
#define	B11 (1U<<11)
#define	B12 (1U<<12)
#define	B13 (1U<<13)
#define	B14 (1U<<14)
#define	B15 (1U<<15)
#define B16 (1UL<<16)
#define B17 (1UL<<17)
#define B18 (1UL<<18)
#define	B19 (1UL<<19)
#define B20 (1UL<<20)
#define B21 (1UL<<21)
#define B22 (1UL<<22)
#define B23 (1UL<<23)
#define B24 (1UL<<24)
#define B25 (1UL<<25)
#define B26 (1UL<<26)
#define B27 (1UL<<27)
#define B28 (1UL<<28)
#define B29 (1UL<<29)
#define B30 (1UL<<30)
#define B31 (1UL<<31)

#define BIT(pos) (1UL<<(pos))

#define	BIGINT	0x40000000 // big int value

#define _T(a) a

#define PI 3.14159265358979324
#define PI2 (3.14159265358979324*2)

// ----------------------------------------------------------------------------
//                                   Includes
// ----------------------------------------------------------------------------
// system includes
#include <string.h>

// SDK includes
#include "boards/pico.h"

#include "hardware/clocks.h"
#include "hardware/divider.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/platform_defs.h"
#include "hardware/pll.h"
#include "hardware/pwm.h"
#include "hardware/resets.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/vreg.h"
#include "hardware/structs/ssi.h"
//#include "hardware/uart.h"

#include "pico.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/types.h"
#include "pico/config.h"
#include "pico/float.h"
#include "pico/mutex.h"
#include "pico/platform.h"
//#include "pico/printf.h"
#include "pico/util/queue.h"

// PicoVGA includes
#include "define.h"	// common definitions of C and ASM
#include "util/overclock.h" // overclock
#include "vga_vmode.h"	// VGA videomodes
#include "vga.h"	 // VGA output

#define PrintSetup(...)
#define PrintClear()
