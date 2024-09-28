#ifndef PET_INTF_H
#define PET_INTF_H

#include "stdint.h"

#ifdef __cplusplus
}
#endif

extern void pet_command( const char * cmd );
extern void pet_prg_write(uint8_t * src, int length );
extern void pet_prg_run( void );

#ifdef __cplusplus
}
#endif

#endif
