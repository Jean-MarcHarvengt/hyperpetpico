
// ****************************************************************************
//                                 
//                            VGA configuration
//
// ****************************************************************************

// === Configuration
#define MAXX		640	// max. resolution in X direction (must be power of 4)
#define MAXY		480	// max. resolution in Y direction

#define MAXLINE		700	// max. number of scanlines (including sync and dark lines)

// === Scanline render buffers (800 pixels: default size of buffers = 2*4*(800+8+800+24)+800 = 13856 bytes
//  Requirements by format, base layer 0, 1 wrap X segment:
//	data buffer "width" bytes, control buffer 16 bytes
#define DBUF0_MAX	(MAXX+8)	// max. size of data buffer of layer 0
#define CBUF0_MAX	((MAXX+24)/4)	// max. size of control buffer of layer 0


#define	DBUF_MAX	DBUF0_MAX	// max. size of data buffer
#define	CBUF_MAX	CBUF0_MAX	// max. size of control buffer

// === VGA port pins
//	GP0 ... VGA B0 blue
//	GP1 ... VGA B1
//	GP2 ... VGA G0 green
//	GP3 ... VGA G1
//	GP4 ... VGA G2
//	GP5 ... VGA R0 red
//	GP6 ... VGA R1
//	GP7 ... VGA R2
//	GP8 ... VGA SYNC synchronization (inverted: negative SYNC=LOW=0x80, BLACK=HIGH=0x00)

#ifdef RETROVGA 
#define VGA_GPIO_FIRST	0	// first VGA GPIO
#define VGA_GPIO_NUM	9	// number of VGA GPIOs, including HSYNC and VSYNC
#define VGA_GPIO_OUTNUM	8	// number of VGA color GPIOs, without HSYNC and VSYNC
#define VGA_GPIO_LAST	(VGA_GPIO_FIRST+VGA_GPIO_NUM-1)	// last VGA GPIO
#define VGA_GPIO_SYNC	8	// VGA SYNC GPIO
#else
#define VGA_GPIO_FIRST	12 //0	// first VGA GPIO
#define VGA_GPIO_NUM	9	// number of VGA GPIOs, including HSYNC and VSYNC
#define VGA_GPIO_OUTNUM	8	// number of VGA color GPIOs, without HSYNC and VSYNC
#define VGA_GPIO_LAST	(VGA_GPIO_FIRST+VGA_GPIO_NUM-1)	// last VGA GPIO
#define VGA_GPIO_SYNC	20 //8	// VGA SYNC GPIO
#endif

// VGA PIO and state machines
#define VGA_PIO		pio0	// VGA PIO
#define VGA_SM0		0	// VGA state machine of base layer 0
#define VGA_SMALL	B0	// mask of all state machines

// VGA DMA
#define VGA_DMA		  2		// VGA DMA base channel
#define VGA_DMA_CB0	  (VGA_DMA+0)	// VGA DMA channel - control block of base layer
#define VGA_DMA_PIO0  (VGA_DMA+1)	// VGA DMA channel - copy data of base layer to PIO (raises IRQ0 on quiet)

#define VGA_DMA_NUM	  (2)	// number of used DMA channels
#define VGA_DMA_FIRST VGA_DMA		// first used DMA
#define VGA_DMA_LAST  (VGA_DMA_FIRST+VGA_DMA_NUM-1) // last used DMA
