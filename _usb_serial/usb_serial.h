#ifndef USB_SERIAL_H
#define USB_SERIALF_H

#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
  sercmd_undef=0,
  sercmd_reset=1,
  sercmd_key=2,
  sercmd_prg=3,
  sercmd_run=4,
} SerialCmd;

extern void usb_serial_init(int (*receive)(uint8_t *buf, int bytes));
extern int usb_serial_tx(uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
