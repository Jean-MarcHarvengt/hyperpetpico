
// ****************************************************************************
//                                 
//                              Common definitions
//
// ****************************************************************************

// ----------------------------------------------------------------------------
//                                   Includes
// ----------------------------------------------------------------------------

#include "global.h"		// global common definitions
#include "vga.pio.h"	// VGA PIO compilation

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
#ifdef ISRP2040 
#include "hardware/structs/ssi.h"
#endif
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

// PicoVGA includes (to be moved)
#include "define.h"	// common definitions of C and ASM
#include "util/overclock.h" // overclock
#include "vga_vmode.h"	// VGA videomodes
#include "vga.h"	 // VGA output

#define PrintSetup(...)
#define PrintClear()