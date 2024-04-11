#include "petbus.h"

#ifdef HAS_PETIO
#include "petbus.pio.h"
#ifdef PETIO_EDIT
//#include "edit4.h"
//#include "edit480.h"
#include "edit450.h"
#include "edit48050.h"
#endif
#endif

// PET shadow memory 8000-9fff
unsigned char mem[0x2000];
// PET shadow memory a000-afff
#ifdef PETIO_A000
unsigned char mem_a000[0x1000];
#endif
bool font_lowercase = false;

#ifdef HAS_PETIO
// Petbus PIO config
#define CONFIG_PIN_PETBUS_DATA_BASE 0 /* 8+1(RW) pins */
#define CONFIG_PIN_PETBUS_RW (CONFIG_PIN_PETBUS_DATA_BASE + 8)
#define CONFIG_PIN_PETBUS_CONTROL_BASE (CONFIG_PIN_PETBUS_DATA_BASE + 9) //CE DATA,ADDRLO,ADDRHI
#define CONFIG_PIN_PETBUS_PHI2  26
#define CONFIG_PIN_PETBUS_DATADIR 28
#define CONFIG_PIN_PETBUS_READCTRL 27
#define CONFIG_PIN_PETBUS_RESET 22

#define VALID_CYCLE ((1 << CONFIG_PIN_PETBUS_PHI2) | (1 << CONFIG_PIN_PETBUS_RESET))

const PIO pio = pio1;
const uint sm = 0;
const uint smread = 1;

#define RESET_TRESHOLD 15000
static uint32_t reset_counter = 0;
static bool got_reset = false;

#ifdef PETIO_EDIT
static unsigned char mem_9000[0x0800];
static bool edit80 = true;
#endif

/********************************
 * petio PIO read table
********************************/ 
static void __not_in_flash("readNone") readNone(uint32_t address) {
  pio1->txf[smread] = 0;
}

static void __not_in_flash("read8000") read8000(uint32_t address) {
  pio1->txf[smread] = 0x100 | mem[address-0x8000];
}

static void __not_in_flash("read9000") read9000(uint32_t address) {
  pio1->txf[smread] = 0x100 | mem[address-0x8000];
}

static void __not_in_flash("readA000") readA000(uint32_t address) {
  pio1->txf[smread] = 0x100 | mem_a000[address-0xa000];
}

static void (*readFuncTable[16])(uint32_t);/*
 = {

  readNone, // 0
  readNone, // 1
  readNone, // 2 
  readNone, // 3
  readNone, // 4
  readNone, // 5
  readNone, // 6
  readNone, // 7
  read8000, // 8
  read9000, // 9
  readA000, // a
  readNone, // b
  readNone, // c
  readNone, // d
  readNone, // e
  readNone, // f
};
*/

extern void handle_custom_registers(uint16_t address, uint8_t value);

/********************************
 * petio loop
********************************/ 
void __not_in_flash("__time_critical_func") petbus_loop(void) {
//void __noinline __time_critical_func(petbus_loop)(void) {
  for(;;) {
    uint32_t allgpios = sio_hw->gpio_in; 
    if ((allgpios & VALID_CYCLE) == VALID_CYCLE) {
      uint32_t value = pio_sm_get_blocking(pio, sm);
      const bool is_write = ((value & (1u << (CONFIG_PIN_PETBUS_RW - CONFIG_PIN_PETBUS_DATA_BASE))) == 0);
      uint16_t address = (value >> 9) & 0xffff;      
      if (is_write)
      {
        if ( address >= 0x8000)
        {
          if ( address < 0xa000)
          {
            mem[address-0x8000] = value &= 0xff;
            //handle_custom_registers(address, value &= 0xff);
          }
#ifdef PETIO_A000
          else if ( address < 0xb000)
          {
            mem_a000[address-0xa000] = value &= 0xff;
          }
#endif
          else if (address == 0xe84C)
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
#ifdef PETIO_EDIT
          else if ( address == 0xe000)
          {
            edit80 = (value &= 0xff)?false:true;
          }
#endif
        }
        pio_sm_drain_tx_fifo(pio,smread);
      }
      else {
        void (*readFunc)(uint32_t) = readFuncTable[address>>12];
        readFunc(address);
/*        
        value = 0;
#ifdef PETIO_A000        
        if ( ( address >= 0xa000) && ( address < 0xb000) ) 
        {
          value = 0x100 | mem_a000[address-0xa000];
        }
        else  
#endif 
        if ( ( address >= 0x9000) && ( address < 0xa000) ) 
        {
          value = 0x100 | mem[address-0x9000+0x1000];
        }
#ifdef PETIO_EDIT        
        else if ( ( address >= 0xe000) && ( address < 0xe800) ) 
        {
          value = 0x100 | mem_9000[address-0xe000];
        } 
#endif
        pio1->txf[smread] = value;
*/        
      }      
    }
  }
}

/********************************
 * Initialization
********************************/ 
void petbus_init(void)
{ 

  readFuncTable[0]=readNone;
  readFuncTable[1]=readNone;
  readFuncTable[2]=readNone;
  readFuncTable[3]=readNone;
  readFuncTable[4]=readNone;
  readFuncTable[5]=readNone;
  readFuncTable[6]=readNone;
  readFuncTable[7]=readNone;
  readFuncTable[8]=read8000;
  readFuncTable[9]=read9000;
  readFuncTable[10]=readA000;
  readFuncTable[11]=readNone;
  readFuncTable[12]=readNone;
  readFuncTable[13]=readNone;
  readFuncTable[14]=readNone;
  readFuncTable[15]=readNone;
 
#ifdef PETIO_EDIT
  if (edit80) {
    memcpy((void *)&mem_9000[0], (void *)&edit480[0], 0x800);
  }
  else {
    memcpy((void *)&mem_9000[0], (void *)&edit4[0], 0x800);
  }
#endif

  // Init PETBUS read SM
  uint progra_offsetread = pio_add_program(pio, &petbus_device_read_program);
  pio_sm_claim(pio, smread);
  pio_sm_config cread = petbus_device_read_program_get_default_config(progra_offsetread);
  // map the OUT pin group to the data signals
  sm_config_set_out_pins(&cread, CONFIG_PIN_PETBUS_DATA_BASE, 8);
  // map the SET pin group to the Data transceiver control signals
  sm_config_set_set_pins(&cread, CONFIG_PIN_PETBUS_DATADIR, 1);
  sm_config_set_sideset_pins(&cread, CONFIG_PIN_PETBUS_READCTRL);
//  sm_config_set_sideset_pins(&cread, CONFIG_PIN_PETBUS_CONTROL_BASE);
  pio_sm_init(pio, smread, progra_offsetread, &cread);

  // Init PETBUS main SM
  uint progra_offset = pio_add_program(pio, &petbus_program);
  pio_sm_claim(pio, sm);
  pio_sm_config c = petbus_program_get_default_config(progra_offset);
  // set the bus R/W pin as the jump pin
  sm_config_set_jmp_pin(&c, CONFIG_PIN_PETBUS_RW);
  // map the IN pin group to the data signals
  sm_config_set_in_pins(&c, CONFIG_PIN_PETBUS_DATA_BASE);
  // map the SET pin group to the bus transceiver enable signals
  sm_config_set_set_pins(&c, CONFIG_PIN_PETBUS_CONTROL_BASE, 3);
  // configure left shift into ISR & autopush every 25 bits
  sm_config_set_in_shift(&c, false, true, 24+1);
  pio_sm_init(pio, sm, progra_offset, &c);

  // configure the GPIOs
  // Ensure all transceivers disabled and datadir is 0 (in) 
  pio_sm_set_pins_with_mask(
      pio, sm, ((uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_READCTRL) , 
               ((uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_READCTRL));
  pio_sm_set_pindirs_with_mask(pio, sm, ((uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_READCTRL) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR),
      ((uint32_t)0x1 << CONFIG_PIN_PETBUS_PHI2) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_RESET) | ((uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_READCTRL) | ((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR) | ((uint32_t)0x1ff << CONFIG_PIN_PETBUS_DATA_BASE));

 // pio_sm_set_pins_with_mask(pio, smread, 0, ((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR));
 // pio_sm_set_pindirs_with_mask(pio, smread,((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR), ((uint32_t)0x1 << CONFIG_PIN_PETBUS_DATADIR) /*| ((uint32_t)0xff << CONFIG_PIN_PETBUS_DATA_BASE)*/) ;

  // Disable input synchronization on input pins that are sampled at known stable times
  // to shave off two clock cycles of input latency
  pio->input_sync_bypass |= (0x1ff << CONFIG_PIN_PETBUS_DATA_BASE);
  
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_PHI2);
  gpio_set_pulls(CONFIG_PIN_PETBUS_PHI2, false, false);
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_RESET);
  gpio_set_pulls(CONFIG_PIN_PETBUS_RESET, false, false);

  for(int pin = CONFIG_PIN_PETBUS_CONTROL_BASE; pin < CONFIG_PIN_PETBUS_CONTROL_BASE + 3; pin++) {
      pio_gpio_init(pio, pin);
  }
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_DATADIR);
  pio_gpio_init(pio, CONFIG_PIN_PETBUS_READCTRL);

  for(int pin = CONFIG_PIN_PETBUS_DATA_BASE; pin < CONFIG_PIN_PETBUS_DATA_BASE + 9; pin++) {
      pio_gpio_init(pio, pin);
      gpio_set_pulls(pin, false, false);
  }

//  pio_sm_set_enabled(pio, sm, true);

  irq_set_enabled(TIMER_IRQ_0, false);
  irq_set_enabled(TIMER_IRQ_1, false);
  irq_set_enabled(TIMER_IRQ_2, false);
  irq_set_enabled(TIMER_IRQ_3, false);
  irq_set_enabled(PWM_IRQ_WRAP, false);
  irq_set_enabled(USBCTRL_IRQ, false);
  irq_set_enabled(XIP_IRQ, false);
  irq_set_enabled(PIO0_IRQ_0, false);
  irq_set_enabled(PIO0_IRQ_1, false);
  irq_set_enabled(PIO1_IRQ_0, false);
  irq_set_enabled(PIO1_IRQ_1, false);
  irq_set_enabled(DMA_IRQ_0, false);
  irq_set_enabled(DMA_IRQ_1, false);
  irq_set_enabled(IO_IRQ_BANK0, false);
  irq_set_enabled(IO_IRQ_QSPI , false);
  irq_set_enabled(SIO_IRQ_PROC0, false);
  irq_set_enabled(SIO_IRQ_PROC1, false);
  irq_set_enabled(CLOCKS_IRQ  , false);
  irq_set_enabled(SPI0_IRQ  , false);
  irq_set_enabled(SPI1_IRQ  , false);
  irq_set_enabled(UART0_IRQ , false);
  irq_set_enabled(UART1_IRQ , false);
  irq_set_enabled(I2C0_IRQ, false);
  irq_set_enabled(I2C1_IRQ, false);
  irq_set_enabled(RTC_IRQ, false);

  pio_enable_sm_mask_in_sync(pio, (1 << sm) | (1 << smread));
  pio_sm_clear_fifos(pio,sm);
  pio_sm_clear_fifos(pio,smread);
}


/********************************
 * Check for reset
********************************/ 
extern bool petbus_poll_reset(void)
{  
  bool retval = false;
  // low is reset => true
  bool reset_state = !(sio_hw->gpio_in & (1 << CONFIG_PIN_PETBUS_RESET));
  if (reset_state) {
    if (!got_reset) {
      if (reset_counter < RESET_TRESHOLD) {
        reset_counter++;
        mem[0] = mem[0] + 1; 
      }
      else {
        got_reset = true;
        retval = true;
      }
    }
  }
  else {
    got_reset = false;
    reset_counter = 0;
  }
  return retval;
}

extern void petbus_reset(void)
{
 #ifdef PETIO_EDIT        
  if (edit80) {
    memcpy((void *)&mem_9000[0], (void *)&edit480[0], 0x800);
  }
  else {
    memcpy((void *)&mem_9000[0], (void *)&edit4[0], 0x800);
  }
#endif 
}

#endif
