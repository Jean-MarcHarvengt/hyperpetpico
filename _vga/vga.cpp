
// ****************************************************************************
//
//                                 VGA output
//
// ****************************************************************************

#include "include.h"

// scanline type
#define LINE_VSYNC	0	// long vertical sync
#define LINE_DARK	1	// dark line
#define LINE_IMG	2	// progressive image 0, 1, 2,...

// color
#define COL_BLACK	0

// base layer commands
#define VGADARK(num,col) (((u32)(vga_offset_dark+BASE_OFFSET)<<27) | ((u32)(num)<<8) | (u32)(col)) // assemble control word of "dark" command
#define VGACMD(jmp,num) (((u32)(jmp)<<27) | (u32)(num)) // assemble control word
// swap bytes of command
#define BYTESWAP(n) ((((n)&0xff)<<24)|(((n)&0xff00)<<8)|(((n)&0xff0000)>>8)|(((n)&0xff000000)>>24))



// current videomode
static int DispDev;			// current display device
static sVmode CurVmode;		// copy of current videomode table
volatile int ScanLine;	// current scan line 1...
volatile u32 Frame;		// frame counter
volatile int BufInx;	// current buffer set (0..1)
volatile Bool VSync;	// current scan line is vsync or dark

// scanline type
static u8 ScanlineType[MAXLINE];
// next control buffer
static u32* CtrlBufNext;

// line buffers
static ALIGNED u8 LineBuf1[DBUF_MAX]; // scanline 1 image data
static ALIGNED u8 LineBuf2[DBUF_MAX]; // scanline 2 image data

static u32	LineBufHsBp[4];		// HSYNC ... back porch-1 ... IRQ command ... image command
static u32	LineBufFp;		// front porch+1
static u32	LineBufDark[2];		// HSYNC ... dark line
static u32	LineBufSync[10];	// vertical synchronization
				//  interlaced (5x half scanlines):
				//	2x half synchronization (HSYNC pulse/2 ... line dark/2)
				//	2x vertical synchronization (invert line dark/2 ... invert HSYNC pulse)
				//	1x half synchronization (HSYNC pulse/2 ... line dark/2)
				// progressive: 1x scanline with vertical synchronization (invert line dark ... invert HSYNC pulse)

static ALIGNED u8	LineBuf0[BLACK_MAX]; // line buffer with black color (used to clear rest of scanline)

// control buffers (BufInx = 0 running CtrlBuf1 and preparing CtrlBuf2, BufInx = 1 running CtrlBuf2 and preparing CtrlBuf1)
static u32	CtrlBuf1[CBUF_MAX]; // base layer control pairs: u32 count, read address (must be terminated with [0,0])
static u32	CtrlBuf2[CBUF_MAX]; // base layer control pairs: u32 count, read address (must be terminated with [0,0])

static u32	CtrlBufVsync[4];
static u32	CtrlBufDark[4];

// saved integer divider state
static hw_divider_state_t DividerState;


extern void VideoRenderLineBG(u8 * linebuffer, int scanline);
extern void VideoRenderLineL0(u8 * linebuffer, int scanline);
extern void VideoRenderLineL1(u8 * linebuffer, int scanline);
extern void VideoRenderUpdate(void);
extern void AudioRender(void);
extern void Core1Call(void);

// VGA DMA handler - called on end of every scanline
extern "C" void __not_in_flash_func(VgaLine)()
{
	// process scanline buffers (will save integer divider state into DividerState)

	// Clear the interrupt request for DMA control channel
	dma_hw->ints0 = (1u << VGA_DMA_PIO0);

	// update DMA control channels of base layer, and run it
	dma_channel_set_read_addr(VGA_DMA_CB0, CtrlBufNext, true);

	// save integer divider state
	hw_divider_save_state(&DividerState);
    
    AudioRender();
	// increment scanline
	int line = ScanLine;	// current scanline
	line++; 		// new current scanline
	if (line > CurVmode.vtot) // last scanline?
	{
		Frame++;	// increment frame counter
		line = 1; 	// restart scanline
	}
	ScanLine = line;	// store new scanline

	// next rendered scanline
	u8 linetype = ScanlineType[line];
	switch (linetype)
	{
	case LINE_VSYNC:	// long vertical sync
		VSync = True;
		CtrlBufNext = CtrlBufVsync;
		BufInx = 0;
		if (line == 1) {
			VideoRenderUpdate();
		}			
		break;

	case LINE_DARK:		// dark line
		VSync = True;
		CtrlBufNext = CtrlBufDark;
		break;

	case LINE_IMG:		// progressive image		
		VSync = False;
		int y0 = line - CurVmode.vfirst;;
		// prepare buffers to be processed next
		u8* dbuf; // data buffer
		u32* cbuf; // control buffer
		if (BufInx == 0)
		{
			dbuf = LineBuf1;
			cbuf = CtrlBuf1;
		}
		else
		{
			dbuf = LineBuf2;
			cbuf = CtrlBuf2;
		}
#ifdef SIXTYHZ
		u8 BufMod = y0 & 1;
		if (BufMod)
			BufInx = BufInx ^ 1;
		if (!BufMod) {  
			if (CurVmode.dbly) y0 >>= 1;
			// HSYNC + back porch
			*cbuf++ = 4; // send 4x u32
			*cbuf++ = (u32)LineBufHsBp; // HSYNC + back porch
			// render scanline
			*cbuf++ = CurVmode.width/4;
			*cbuf++ = (u32)dbuf;
		 	VideoRenderLineBG(dbuf, y0);
		 	VideoRenderLineL0(dbuf, y0);
			// front porch
			*cbuf++ = 1; // send 1x u32
			*cbuf++ = (u32)&LineBufFp; // front porch
		}
		else {
			CtrlBufNext = cbuf;
			if (CurVmode.dbly) y0 >>= 1;
			// HSYNC + back porch
			*cbuf++ = 4; // send 4x u32
			*cbuf++ = (u32)LineBufHsBp; // HSYNC + back porch
			// render scanline
			*cbuf++ = CurVmode.width/4;
			*cbuf++ = (u32)dbuf;
		 	VideoRenderLineL1(dbuf, y0);
			// front porch
			*cbuf++ = 1; // send 1x u32
			*cbuf++ = (u32)&LineBufFp; // front porch

		}
#else
		CtrlBufNext = cbuf;
		BufInx = BufInx ^ 1;
		// HSYNC + back porch
		*cbuf++ = 4; // send 4x u32
		*cbuf++ = (u32)LineBufHsBp; // HSYNC + back porch
		// render scanline
		*cbuf++ = CurVmode.width/4;
		*cbuf++ = (u32)dbuf;
	 	VideoRenderLineBG(dbuf, y0);
	 	VideoRenderLineL0(dbuf, y0);
		VideoRenderLineL1(dbuf, y0);
		// front porch
		*cbuf++ = 1; // send 1x u32
		*cbuf++ = (u32)&LineBufFp; // front porch
#endif
		*cbuf++ = 0; // end mark
		*cbuf++ = 0; // end mark
		break;
	}

	// restore integer divider state
	hw_divider_restore_state(&DividerState);
}

// initialize VGA DMA
//   control blocks aliases:
//                  +0x0        +0x4          +0x8          +0xC (Trigger)
// 0x00 (alias 0):  READ_ADDR   WRITE_ADDR    TRANS_COUNT   CTRL_TRIG
// 0x10 (alias 1):  CTRL        READ_ADDR     WRITE_ADDR    TRANS_COUNT_TRIG
// 0x20 (alias 2):  CTRL        TRANS_COUNT   READ_ADDR     WRITE_ADDR_TRIG
// 0x30 (alias 3):  CTRL        WRITE_ADDR    TRANS_COUNT   READ_ADDR_TRIG ... !

void VgaDmaInit()
{
	dma_channel_config cfg;

// ==== prepare DMA control channel

	// prepare DMA default config
	cfg = dma_channel_get_default_config(VGA_DMA_CB0);

	// increment address on read from memory
	channel_config_set_read_increment(&cfg, true);

	// increment address on write to DMA port
	channel_config_set_write_increment(&cfg, true);

	// each DMA transfered entry is 32-bits
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);

	// write ring - wrap to 8-byte boundary (TRANS_COUNT and READ_ADDR_TRIG of data DMA)
	channel_config_set_ring(&cfg, true, 3);

	// DMA configure
	dma_channel_configure(
		VGA_DMA_CB0,	// channel
		&cfg,			// configuration
		&dma_hw->ch[VGA_DMA_PIO0].al3_transfer_count, // write address
		&CtrlBuf1[0],		// read address - as first, control buffer 1 will be sent out
		2,			// number of transfers in u32
		false			// do not start yet
		);

// ==== prepare DMA data channel

	// prepare DMA default config
	cfg = dma_channel_get_default_config(VGA_DMA_PIO0);

	// increment address on read from memory
	channel_config_set_read_increment(&cfg, true);

	// do not increment address on write to PIO
	channel_config_set_write_increment(&cfg, false);

	// each DMA transfered entry is 32-bits
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);

	// DMA data request for sending data to PIO
	channel_config_set_dreq(&cfg, pio_get_dreq(VGA_PIO, VGA_SM0, true));

	// chain channel to DMA control block
	channel_config_set_chain_to(&cfg, VGA_DMA_CB0);

	// raise the IRQ flag when 0 is written to a trigger register (end of chain)
	channel_config_set_irq_quiet(&cfg, true);

	// set byte swapping
	channel_config_set_bswap(&cfg, true);

	// set high priority
	cfg.ctrl |= DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS;

	// DMA configure
	dma_channel_configure(
		VGA_DMA_PIO0,	// channel
		&cfg,			// configuration
		&VGA_PIO->txf[VGA_SM0], // write address
		NULL,			// read address
		0,			// number of transfers in u32
		false			// do not start immediately
	);


// ==== initialize IRQ0, raised from base layer 0

	// enable DMA channel IRQ0
	dma_channel_set_irq0_enabled(VGA_DMA_PIO0, true);

	// set DMA IRQ handler
	irq_set_exclusive_handler(DMA_IRQ_0, VgaLine);

	// set highest IRQ priority
	irq_set_priority(DMA_IRQ_0, 0);
}

// initialize VGA PIO
void VgaPioInit()
{
	int i;

	// clear PIO instruction memory 
	pio_clear_instruction_memory(VGA_PIO);

	// configure main program instructions
	uint16_t ins[32]; // temporary buffer of program instructions
	memcpy(ins, &vga_program_instructions, vga_program.length*sizeof(uint16_t)); // copy program into buffer
	u16 cpp = (u16)CurVmode.cpp; // number of clocks per pixel
	ins[vga_offset_extra1] |= (cpp-2) << 8; // update waits
	ins[vga_offset_extra2] |= (cpp-2) << 8; // update waits
	
	// load main program into PIO's instruction memory
	struct pio_program prg;
	prg.instructions = ins;
	prg.length = vga_program.length;
	prg.origin = BASE_OFFSET;
	pio_add_program(VGA_PIO, &prg);

	// connect PIO to the pad
	for (i = VGA_GPIO_FIRST; i <= VGA_GPIO_LAST; i++) pio_gpio_init(VGA_PIO, i);

	// negative HSYNC output
	if (!CurVmode.psync) gpio_set_outover(VGA_GPIO_SYNC, GPIO_OVERRIDE_INVERT);

	// set pin direction to output
	pio_sm_set_consecutive_pindirs(VGA_PIO, VGA_SM0, VGA_GPIO_FIRST, VGA_GPIO_NUM, true);

	// get default config
	pio_sm_config cfg = pio_get_default_sm_config();

	// map state machine's OUT and MOV pins	
	sm_config_set_out_pins(&cfg, VGA_GPIO_FIRST, VGA_GPIO_OUTNUM);

	// join FIFO to send only
	sm_config_set_fifo_join(&cfg, PIO_FIFO_JOIN_TX);

	// PIO clock divider
	sm_config_set_clkdiv(&cfg, CurVmode.div);

	// shift left, autopull, pull threshold
	sm_config_set_out_shift(&cfg, false, true, 32);

	// base layer 0
	// set wrap
	sm_config_set_wrap(&cfg, vga_wrap_target+BASE_OFFSET, vga_wrap+BASE_OFFSET);

	// set sideset pins of base layer
	sm_config_set_sideset(&cfg, 1, false, false);
	sm_config_set_sideset_pins(&cfg, VGA_GPIO_SYNC);

	// initialize state machine
	pio_sm_init(VGA_PIO, VGA_SM0, vga_offset_entry+BASE_OFFSET, &cfg);
}

// initialize scanline buffers
void VgaBufInit()
{
	// init HSYNC..back porch buffer
	//  hsync must be min. 3
	//  hback must be min. 13
	LineBufHsBp[0] = BYTESWAP(VGACMD(vga_offset_sync+BASE_OFFSET,CurVmode.hsync-3)); // HSYNC
	LineBufHsBp[1] = BYTESWAP(VGADARK(CurVmode.hback-4-1-9,0)); // back porch - 1 - 9
	LineBufHsBp[2] = BYTESWAP(VGACMD(vga_offset_irqset+BASE_OFFSET,0)); // IRQ command (takes 9 clock cycles)
	LineBufHsBp[3] = BYTESWAP(VGACMD(vga_offset_output+BASE_OFFSET, CurVmode.width - 2)); // missing 2 clock cycles after last pixel

	// init front porch buffer
	//  hfront must be min. 4
	LineBufFp = BYTESWAP(VGADARK(CurVmode.hfront-4,0)); // front porch

	// init dark line
	LineBufDark[0] = BYTESWAP(VGACMD(vga_offset_sync+BASE_OFFSET,CurVmode.hsync-3)); // HSYNC
	LineBufDark[1] = BYTESWAP(VGADARK(CurVmode.htot-CurVmode.hsync-4,0)); // dark line

	// VGA mode
	// vertical synchronization
	//   hsync must be min. 4
	LineBufSync[0] = BYTESWAP(VGACMD(vga_offset_sync+BASE_OFFSET,CurVmode.htot-CurVmode.hsync-3)); // invert dark line
	LineBufSync[1] = BYTESWAP(VGADARK(CurVmode.hsync-4,0)); // invert HSYNC

	// control blocks - initialize to VSYNC
	CtrlBuf1[0] = 2; // send 2x u32
	CtrlBuf1[1] = (u32)&LineBufSync[0]; // VSYNC
	CtrlBuf1[2] = 0; // stop mark
	CtrlBuf1[3] = 0; // stop mark

	CtrlBuf2[0] = 2; // send 2x u32
	CtrlBuf2[1] = (u32)&LineBufSync[0]; // VSYNC
	CtrlBuf2[2] = 0; // stop mark
	CtrlBuf2[3] = 0; // stop mark

	CtrlBufVsync[0] = 2; // send 2x u32
	CtrlBufVsync[1] = (u32)&LineBufSync[0]; // VSYNC
	CtrlBufVsync[2] = 0; // stop mark
	CtrlBufVsync[3] = 0; // stop mark

	CtrlBufDark[0] = 2; // send 2x u32
	CtrlBufDark[1] = (u32)LineBufDark; // dark
	CtrlBufDark[2] = 0; // stop mark
	CtrlBufDark[3] = 0; // stop mark
}

// terminate VGA service
void VgaTerm()
{
	// abort DMA channels
	dma_channel_abort(VGA_DMA_PIO0); // pre-abort, could be chaining right now
	dma_channel_abort(VGA_DMA_CB0);
	dma_channel_abort(VGA_DMA_PIO0);
	dma_channel_abort(VGA_DMA_CB0);

	// disable IRQ0 from DMA0
	irq_set_enabled(DMA_IRQ_0, false);
	dma_channel_set_irq0_enabled(VGA_DMA_PIO0, false);

	// Clear the interrupt request for DMA control channel
	dma_hw->ints0 = (1u << VGA_DMA_PIO0);

	// stop all state machines
	pio_set_sm_mask_enabled(VGA_PIO, VGA_SMALL, false);

	// restart state machine
	pio_restart_sm_mask(VGA_PIO, VGA_SMALL);

	// clear FIFOs
	pio_sm_clear_fifos(VGA_PIO, VGA_SM0);
	CtrlBufNext = NULL;

	// clear PIO instruction memory 
	pio_clear_instruction_memory(VGA_PIO);
}

// initialize scanline type table
static void ScanlineTypeInit(const sVmode* v)
{
	u8* d = ScanlineType;
	int i, k;

	// line 0 is not used
	*d++ = LINE_DARK;

	// progressive mode (VGA 525)
	// vertical sync (VGA 2)
	for (i = v->vsync; i > 0; i--) *d++ = LINE_VSYNC;

	// dark (VGA 33)
	for (i = v->vback; i > 0; i--) *d++ = LINE_DARK;

	// image (VGA 480)
	for (i = v->vact; i > 0; i--) *d++ = LINE_IMG;

	// dark (VGA 10)
	for (i = v->vfront; i > 0; i--) *d++ = LINE_DARK;
}



// initialize videomode (returns False on bad configuration)
// - All layer modes must use same layer program (LAYERMODE_BASE = overlapped layers are OFF)
void VgaInit(const sVmode* vmode)
{
	// stop old state
	VgaTerm();

	// initialize scanline type table
	ScanlineTypeInit(vmode);

	// clear buffer with black color
	memset(LineBuf0, COL_BLACK, BLACK_MAX);

	// save current videomode
	memcpy(&CurVmode, vmode, sizeof(sVmode));

	// initialize parameters
	ScanLine = 1; // currently processed scanline
//	Frame = 0;
	BufInx = 0; // at first, control buffer 1 will be sent out
//	CtrlBufNext = CtrlBuf2;
	CtrlBufNext = CtrlBufVsync;

	// initialize VGA PIO
	VgaPioInit();

	// initialize scanline buffers
	VgaBufInit();

	// initialize DMA
	VgaDmaInit();

	// enable DMA IRQ
	irq_set_enabled(DMA_IRQ_0, true);

	// start DMA with base layer 0
	dma_channel_start(VGA_DMA_CB0);

	// run state machines
	pio_enable_sm_mask_in_sync(VGA_PIO, VGA_SMALL);
}

const sVmode* volatile VgaVmodeReq = NULL; // request to reinitialize videomode, 1=only stop driver

void (* volatile Core1Fnc)() = NULL; // core 1 remote function

// VGA core
void VgaCore()
{
	const sVmode* v;
	void (*fnc)();
	while (1)
	{
		__dmb();

		// initialize videomode
		v = VgaVmodeReq;
		if (v != NULL)
		{
			if ((u32)v == (u32)1)
				VgaTerm(); // terminate
			else
				VgaInit(v);
			__dmb();
			VgaVmodeReq = NULL;
		}

		// execute remote function
		fnc = Core1Fnc;
		if (fnc != NULL)
		{
			fnc();
			__dmb();
			Core1Fnc = NULL;
		}
		Core1Call();		
	}
}

// request to initialize VGA videomode, NULL=only stop driver (wait to initialization completes)
void VgaInitReq(const sVmode* vmode)
{
	if (vmode == NULL) vmode = (const sVmode*)1;
	__dmb();
	VgaVmodeReq = vmode;
	while (VgaVmodeReq != NULL) { __dmb(); }
}

// execute core 1 remote function
void Core1Exec(void (*fnc)())
{
	__dmb();
	Core1Fnc = fnc;
	__dmb();
}

// check if core 1 is busy (executing remote function)
Bool Core1Busy()
{
	__dmb();
	return Core1Fnc != NULL;
}

// wait if core 1 is busy (executing remote function)
void Core1Wait()
{
	while (Core1Busy()) {}
}

// wait for VSync scanline
void WaitVSync()
{
	// wait for end of VSync
	while (VSync) { __dmb(); }

	// wait for start of VSync
	while (!VSync) { __dmb(); }
}

void WaitScanline(int scanline)
{
	while (ScanLine != scanline) { __dmb(); }
}

int GetScanline()
{
	return ScanLine;
}

