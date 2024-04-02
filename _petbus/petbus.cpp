#include "petbus.h"

#ifdef HAS_PETIO
#include "petbus.pio.h"
#ifdef PETIO_EDIT
//#include "edit4.h"
//#include "edit480.h"
#include "edit450.h"
#include "edit48050.h"
#endif
#ifdef PETIO_A000
#include "rom_a000.h"
#endif
#endif

// PET shadow memory 8000-9fff
unsigned char mem[0x2000];

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
static void (*mem_write)(uint16_t address, uint8_t value) = nullptr;

#define RESET_TRESHOLD 15000
static uint32_t reset_counter = 0;
static bool got_reset = false;


#ifdef PETIO_EDIT
static unsigned char mem_9000[0x0800];
static bool edit80 = true;
#endif
#ifdef PETIO_A000
static unsigned char mem_a000[0x1000];
#endif

/********************************
 * petio PIO IRQ
********************************/ 
#ifdef PETIO_IRQ
static void __not_in_flash("rx_irq") rx_irq(void) {
  if(!pio_sm_is_rx_fifo_empty(pio, sm)) 
  {
    uint32_t value = pio_sm_get(pio, sm);
    const bool is_write = ((value & (1u << (CONFIG_PIN_PETBUS_RW - CONFIG_PIN_PETBUS_DATA_BASE))) == 0);
    uint16_t address = (value >> 9) & 0xffff;      
    if (is_write)
    {
#ifdef PETIO_EDIT        
      if ( address == 0xe000)
      {
        edit80 = (value &= 0xff)?false:true;   
      }
      else 
#endif
      if ( address >= 0x8000) 
      {
        mem_write(address, value &= 0xff);
      }
    }
    else {
      value = 0;
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
    }  
  }
}
#endif


void __noinline __time_critical_func(petbus_loop)(void) {
  for(;;) {
#ifdef PETIO_IRQ
    __dmb();
//  __wfi();
#else
    uint32_t allgpios = sio_hw->gpio_in; 
    if ((allgpios & VALID_CYCLE) == VALID_CYCLE) {
      uint32_t value = pio_sm_get_blocking(pio, sm);
      const bool is_write = ((value & (1u << (CONFIG_PIN_PETBUS_RW - CONFIG_PIN_PETBUS_DATA_BASE))) == 0);
      uint16_t address = (value >> 9) & 0xffff;      
      if (is_write)
      {
#ifdef PETIO_EDIT        
        if ( address == 0xe000)
        {
          edit80 = (value &= 0xff)?false:true;   
        }
        else 
#endif
        if ( address >= 0x8000) 
        {
          mem_write(address, value &= 0xff);
        }
        pio_sm_drain_tx_fifo(pio,smread);
      }
      else {
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
      }      
    }
    else {
      pio_sm_drain_tx_fifo(pio,smread);
    }
#endif
  }
}

/********************************
 * Initialization
********************************/ 
void petbus_init(void (*mem_write_callback)(uint16_t address, uint8_t value))
{ 
  mem_write = mem_write_callback;

#ifdef PETIO_A000
  
    memcpy((void *)&mem_a000[0], (void *)&rom_a000[0], 0x1000);
#endif  
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

#ifdef PETIO_IRQ
  // Find a free irq
  static int8_t pio_irq;
  static_assert(PIO0_IRQ_1 == PIO0_IRQ_0 + 1 && PIO1_IRQ_1 == PIO1_IRQ_0 + 1, "");
  pio_irq = (pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
  if (irq_get_exclusive_handler(pio_irq)) {
      pio_irq++;
      if (irq_get_exclusive_handler(pio_irq)) {
          panic("All IRQs are in use");
      }
  }

  // Enable interrupt
  irq_add_shared_handler(pio_irq, rx_irq, 1); // Add a shared IRQ handler
  irq_set_enabled(pio_irq, true); // Enable the IRQ
  const uint irq_index = pio_irq - ((pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0); // Get index of the IRQ
  pio_set_irqn_source_enabled(pio, irq_index, (pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + sm), true); // Set pio to tell us when the FIFO is NOT empty
  //pio_set_irqn_source_enabled(pio, irq_index, (pio_interrupt_source)(pis_interrupt0 + sm), true);
  pio_sm_set_enabled(pio, sm, true);
#endif

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
