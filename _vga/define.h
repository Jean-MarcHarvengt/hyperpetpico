
// ****************************************************************************
//                                 
//                     VGA common definitions of C and ASM
//
// ****************************************************************************

#include "vga_config.h"		// VGA configuration

#define LAYERS_MAX	1	// max. number of layers (should be 4)

#define BLACK_MAX	MAXX	// size of buffer with black color (used to clear rest of unused line)

// VGA PIO program
#define BASE_OFFSET	17	// offset of base layer program
#define LAYER_OFFSET	0	// offset of overlapped layer program

// layer program
#define LAYERPROG_BASE	0	// program of base layer (overlapped layers are OFF)

#define LAYERPROG_NUM	1	// number of layer programs

// layer mode (CPP = clock cycles per pixel)
//	Control buffer: 16 bytes
//	Data buffer: 4 bytes
// fast sprites can be up Control buffer: width*2 bytes
// sprites Data buffer: width bytes
#define LAYERMODE_BASE		0	// base layer

#define LAYERMODE_NUM	1	// number of overlapped layer modes

