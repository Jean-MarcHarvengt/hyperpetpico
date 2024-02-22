#ifdef HAS_PETIO

#include "petbus.h"
#include "petbus.pio.h"

// Petbus PIO config
#define CONFIG_PIN_PETBUS_DATA_BASE 0 /* 8+1(RW) pins */
#define CONFIG_PIN_PETBUS_RW (CONFIG_PIN_PETBUS_DATA_BASE + 8)
#define CONFIG_PIN_PETBUS_CONTROL_BASE (CONFIG_PIN_PETBUS_DATA_BASE + 9) //CE DATA,ADDRLO,ADDRHI
#define CONFIG_PIN_PETBUS_PHI2  26
#define CONFIG_PIN_PETBUS_RESET 22
const PIO pio = pio1;
const uint sm = 0;
static void (*mem_write)(uint16_t address, uint8_t value) = nullptr;
static bool reset_prev = true;

/********************************
 * petio PIO IRQ
********************************/ 
static void rx_irq(void) {
  while(!pio_sm_is_rx_fifo_empty(pio, sm)) {
    uint32_t value = pio_sm_get(pio, sm);
    const bool is_write = ((value & (1u << (CONFIG_PIN_PETBUS_RW - CONFIG_PIN_PETBUS_DATA_BASE))) == 0);
    uint_fast16_t address = (value >> 9) & 0xffff;
    if ( (is_write) && (gpio_get(CONFIG_PIN_PETBUS_RESET)) )
    {
      if ( address >= 0x8000) 
      {
        mem_write(address, value &= 0xff);
      }
    }  
  }
}

/********************************
 * Initialization
********************************/ 
void petbus_init(void (*mem_write_callback)(uint16_t address, uint8_t value))
{
  mem_write = mem_write_callback;
  
  // Init PETBUS
  uint offset = pio_add_program(pio, &petbus_program);
  pio_sm_claim(pio, sm);
  pio_sm_config c = petbus_program_get_default_config(offset);
  // set the bus R/W pin as the jump pin
  sm_config_set_jmp_pin(&c, CONFIG_PIN_PETBUS_RW);
  // map the IN pin group to the data signals
  sm_config_set_in_pins(&c, CONFIG_PIN_PETBUS_DATA_BASE);
  // map the SET pin group to the bus transceiver enable signals
  sm_config_set_set_pins(&c, CONFIG_PIN_PETBUS_CONTROL_BASE, 3);
  // configure left shift into ISR & autopush every 25 bits
  sm_config_set_in_shift(&c, false, true, 24+1);
  pio_sm_init(pio, sm, offset, &c);
  // configure the GPIOs
  // Ensure all transceivers will start disabled
  pio_sm_set_pins_with_mask(
      pio, sm, (uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE, (uint32_t)0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE);
  pio_sm_set_pindirs_with_mask(pio, sm, (0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE),
      (1 << CONFIG_PIN_PETBUS_PHI2) | (1 << CONFIG_PIN_PETBUS_RESET) | (0x7 << CONFIG_PIN_PETBUS_CONTROL_BASE) | (0x1ff << CONFIG_PIN_PETBUS_DATA_BASE));

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
  for(int pin = CONFIG_PIN_PETBUS_DATA_BASE; pin < CONFIG_PIN_PETBUS_DATA_BASE + 9; pin++) {
      pio_gpio_init(pio, pin);
      gpio_set_pulls(pin, false, false);
  }

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
  irq_add_shared_handler(pio_irq, rx_irq, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY); // Add a shared IRQ handler
  irq_set_enabled(pio_irq, true); // Enable the IRQ
  const uint irq_index = pio_irq - ((pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0); // Get index of the IRQ
  pio_set_irqn_source_enabled(pio, irq_index, (pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + sm), true); // Set pio to tell us when the FIFO is NOT empty

  pio_sm_set_enabled(pio, sm, true);
  pio_enable_sm_mask_in_sync(pio, (1 << sm));  
}


/********************************
 * Check for reset
********************************/ 
extern bool petbus_poll_reset(void)
{  
  bool reset_next = gpio_get(CONFIG_PIN_PETBUS_RESET) != 0;
  // REST was high and becomes low
  if ( (reset_prev == true) && (!reset_next) )
  {
    reset_prev = false;
    return true;
  }
  else {
    reset_prev = reset_next;
  }
  return false;
}

#endif
