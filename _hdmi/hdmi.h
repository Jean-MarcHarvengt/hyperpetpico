#ifndef HDMI_H
#define HDMI_H

#include "stdint.h"

extern void HdmiCore(void);
extern void HdmiVSync(void);
extern void Core1Exec(void (*fnc)());
extern void HdmiInit(int mode);
extern bool HdmiIsVSync(void);

#endif
