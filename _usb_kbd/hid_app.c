/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board.h"
#include "tusb.h"
#include "kbd.h"

/* From https://www.kernel.org/doc/html/latest/input/gamepad.html
          ____________________________              __
         / [__ZL__]          [__ZR__] \               |
        / [__ TL __]        [__ TR __] \              | Front Triggers
     __/________________________________\__         __|
    /                                  _   \          |
   /      /\           __             (N)   \         |
  /       ||      __  |MO|  __     _       _ \        | Main Pad
 |    <===DP===> |SE|      |ST|   (W) -|- (E) |       |
  \       ||    ___          ___       _     /        |
  /\      \/   /   \        /   \     (S)   /\      __|
 /  \________ | LS  | ____ |  RS | ________/  \       |
|         /  \ \___/ /    \ \___/ /  \         |      | Control Sticks
|        /    \_____/      \_____/    \        |    __|
|       /                              \       |
 \_____/                                \_____/

     |________|______|    |______|___________|
       D-Pad    Left       Right   Action Pad
               Stick       Stick

                 |_____________|
                    Menu Pad

  Most gamepads have the following features:
  - Action-Pad 4 buttons in diamonds-shape (on the right side) NORTH, SOUTH, WEST and EAST.
  - D-Pad (Direction-pad) 4 buttons (on the left side) that point up, down, left and right.
  - Menu-Pad Different constellations, but most-times 2 buttons: SELECT - START.
  - Analog-Sticks provide freely moveable sticks to control directions, Analog-sticks may also
  provide a digital button if you press them.
  - Triggers are located on the upper-side of the pad in vertical direction. The upper buttons
  are normally named Left- and Right-Triggers, the lower buttons Z-Left and Z-Right.
  - Rumble Many devices provide force-feedback features. But are mostly just simple rumble motors.
 */

// Sony DS4 report layout detail https://www.psdevwiki.com/ps4/DS4-USB
typedef struct TU_ATTR_PACKED
{
  uint8_t x, y, z, rz; // joystick

  struct {
    uint8_t dpad     : 4; // (hat format, 0x08 is released, 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW)
    uint8_t square   : 1; // west
    uint8_t cross    : 1; // south
    uint8_t circle   : 1; // east
    uint8_t triangle : 1; // north
  };

  struct {
    uint8_t l1     : 1;
    uint8_t r1     : 1;
    uint8_t l2     : 1;
    uint8_t r2     : 1;
    uint8_t share  : 1;
    uint8_t option : 1;
    uint8_t l3     : 1;
    uint8_t r3     : 1;
  };

  struct {
    uint8_t ps      : 1; // playstation button
    uint8_t tpad    : 1; // track pad click
    uint8_t counter : 6; // +1 each report
  };

  // comment out since not used by this example
  // uint8_t l2_trigger; // 0 released, 0xff fully pressed
  // uint8_t r2_trigger; // as above

  //  uint16_t timestamp;
  //  uint8_t  battery;
  //
  //  int16_t gyro[3];  // x, y, z;
  //  int16_t accel[3]; // x, y, z

  // there is still lots more info

} sony_ds4_report_t;

// check if device is Sony DualShock 4
static inline bool is_sony_ds4(uint8_t dev_addr)
{
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  return ( (vid == 0x054c && (pid == 0x09cc || pid == 0x05c4)) // Sony DualShock4 
           || (vid == 0x0f0d && pid == 0x005e)                 // Hori FC4 
           || (vid == 0x0f0d && pid == 0x00ee)                 // Hori PS4 Mini (PS4-099U) 
           || (vid == 0x1f4f && pid == 0x1002)                 // ASW GG xrd controller
         );
}

/* ==================  Keycode translation table. ==================== */

/*
 * We only need one translation table, as this simple application only
 * supports 102-key US layouts. We need separate translations for
 * non-shifted and shifted keys because shift is not just a modifier:
 * shift turns a '5' to a '$', for example. We don't need a separate
 * column for 'ctrl', because we can handle that simply by logical
 * AND operations on the keycode. Alt/Meta does not affect the keycode
 * at all -- it's just a flag. 
 */ 

// TODO: fill in the rest of the non-ASCII key codes
// There's a nice list here:
// https://gist.github.com/ekaitz-zarraga/2b25b94b711684ba4e969e5a5723969b
static int conv_table_us[128][2] = 
{
    {0     , 0      }, /* 0x00 */ \
    {0     , 0      }, /* 0x01 */ \
    {0     , 0      }, /* 0x02 */ \
    {0     , 0      }, /* 0x03 */ \
    {'a'   , 'A'    }, /* 0x04 */ \
    {'b'   , 'B'    }, /* 0x05 */ \
    {'c'   , 'C'    }, /* 0x06 */ \
    {'d'   , 'D'    }, /* 0x07 */ \
    {'e'   , 'E'    }, /* 0x08 */ \
    {'f'   , 'F'    }, /* 0x09 */ \
    {'g'   , 'G'    }, /* 0x0a */ \
    {'h'   , 'H'    }, /* 0x0b */ \
    {'i'   , 'I'    }, /* 0x0c */ \
    {'j'   , 'J'    }, /* 0x0d */ \
    {'k'   , 'K'    }, /* 0x0e */ \
    {'l'   , 'L'    }, /* 0x0f */ \
    {'m'   , 'M'    }, /* 0x10 */ \
    {'n'   , 'N'    }, /* 0x11 */ \
    {'o'   , 'O'    }, /* 0x12 */ \
    {'p'   , 'P'    }, /* 0x13 */ \
    {'q'   , 'Q'    }, /* 0x14 */ \
    {'r'   , 'R'    }, /* 0x15 */ \
    {'s'   , 'S'    }, /* 0x16 */ \
    {'t'   , 'T'    }, /* 0x17 */ \
    {'u'   , 'U'    }, /* 0x18 */ \
    {'v'   , 'V'    }, /* 0x19 */ \
    {'w'   , 'W'    }, /* 0x1a */ \
    {'x'   , 'X'    }, /* 0x1b */ \
    {'y'   , 'Y'    }, /* 0x1c */ \
    {'z'   , 'Z'    }, /* 0x1d */ \
    {'1'   , '!'    }, /* 0x1e */ \
    {'2'   , '@'    }, /* 0x1f */ \
    {'3'   , '#'    }, /* 0x20 */ \
    {'4'   , '$'    }, /* 0x21 */ \
    {'5'   , '%'    }, /* 0x22 */ \
    {'6'   , '^'    }, /* 0x23 */ \
    {'7'   , '&'    }, /* 0x24 */ \
    {'8'   , '*'    }, /* 0x25 */ \
    {'9'   , '('    }, /* 0x26 */ \
    {'0'   , ')'    }, /* 0x27 */ \
    {KBD_KEY_ENTER  , KBD_KEY_ENTER   }, /* 0x28 */ \
    {KBD_KEY_ESC,  KBD_KEY_ESC }, /* 0x29 */ \
    {KBD_KEY_BS  , KBD_KEY_BS }, /* 0x2a */ \
    {'\t'  , '\t'   }, /* 0x2b */ \
    {' '   , ' '    }, /* 0x2c */ \
    {'-'   , '_'    }, /* 0x2d */ \
    {'='   , '+'    }, /* 0x2e */ \
    {'['   , '{'    }, /* 0x2f */ \
    {']'   , '}'    }, /* 0x30 */ \
    {'\\'  , '|'    }, /* 0x31 */ \
    {'#'   , '~'    }, /* 0x32 */ \
    {';'   , ':'    }, /* 0x33 */ \
    {'\''  , '\"'   }, /* 0x34 */ \
    {'`'   , '~'    }, /* 0x35 */ \
    {','   , '<'    }, /* 0x36 */ \
    {'.'   , '>'    }, /* 0x37 */ \
    {'/'   , '?'    }, /* 0x38 */ \
    {0     , 0      }, /* 0x39 */ \
    {0     , 0      }, /* 0x3a */ \
    {0     , 0      }, /* 0x3b */ \
    {0     , 0      }, /* 0x3c */ \
    {0     , 0      }, /* 0x3d */ \
    {0     , 0      }, /* 0x3e */ \
    {0     , 0      }, /* 0x3f */ \
    {0     , 0      }, /* 0x40 */ \
    {0     , 0      }, /* 0x41 */ \
    {0     , 0      }, /* 0x42 */ \
    {0     , 0      }, /* 0x43 */ \
    {0     , 0      }, /* 0x44 */ \
    {0     , 0      }, /* 0x45 */ \
    {0     , 0      }, /* 0x46 */ \
    {0     , 0      }, /* 0x47 */ \
    {0     , 0      }, /* 0x48 */ \
    {0     , 0      }, /* 0x49 */ \
    {KBD_KEY_HOME , KBD_KEY_HOME}, /* 0x4a */ \
    {KBD_KEY_PGUP , KBD_KEY_PGUP}, /* 0x4b */ \
    {0     , 0      }, /* 0x4c */ \
    {KBD_KEY_END, KBD_KEY_END}, /* 0x4d */ \
    {KBD_KEY_PGDN , KBD_KEY_PGDN }, /* 0x4e */ \
    {KBD_KEY_RIGHT, KBD_KEY_RIGHT }, /* 0x4f */ \
    {KBD_KEY_LEFT, KBD_KEY_LEFT }, /* 0x50 */ \
    {KBD_KEY_DOWN, KBD_KEY_DOWN }, /* 0x51 */ \
    {KBD_KEY_UP, KBD_KEY_UP }, /* 0x52 */ \
    { 0   , 0       }, /* 0x53 */ \
                                  \
    { 0   , 0       }, /* 0x54 */ \
    { 0   , 0       }, /* 0x55 */ \
    { 0   , 0       }, /* 0x56 */ \
    { 0   , 0       }, /* 0x57 */ \
    { 0   , 0       }, /* 0x58 */ \
    { 0   , 0       }, /* 0x59 */ \
    { 0   , 0       }, /* 0x5a */ \
    { 0   , 0       }, /* 0x5b */ \
    { 0   , 0       }, /* 0x5c */ \
    { 0   , 0       }, /* 0x5d */ \
    { 0   , 0       }, /* 0x5e */ \
    { 0   , 0       }, /* 0x5f */ \
    { 0   , 0       }, /* 0x60 */ \
    { 0   , 0       }, /* 0x61 */ \
    { 0   , 0       }, /* 0x62 */ \
    { 0   , 0       }, /* 0x63 */ \
    { 0   , 0       }, /* 0x64 */ \
    { 0   , 0       }, /* 0x65 */ \
    { 0   , 0       }, /* 0x66 */ \
    { 0   , 0       }, /* 0x67 */ \
};

static int conv_table_be[128][2] = 
{
    {0     , 0      }, /* 0x00 */ \
    {0     , 0      }, /* 0x01 */ \
    {0     , 0      }, /* 0x02 */ \
    {0     , 0      }, /* 0x03 */ \
    {'q'   , 'Q'    }, /* 0x04 */ \
    {'b'   , 'B'    }, /* 0x05 */ \
    {'c'   , 'C'    }, /* 0x06 */ \
    {'d'   , 'D'    }, /* 0x07 */ \
    {'e'   , 'E'    }, /* 0x08 */ \
    {'f'   , 'F'    }, /* 0x09 */ \
    {'g'   , 'G'    }, /* 0x0a */ \
    {'h'   , 'H'    }, /* 0x0b */ \
    {'i'   , 'I'    }, /* 0x0c */ \
    {'j'   , 'J'    }, /* 0x0d */ \
    {'k'   , 'K'    }, /* 0x0e */ \
    {'l'   , 'L'    }, /* 0x0f */ \
    {','   , '?'    }, /* 0x10 */ \
    {'n'   , 'N'    }, /* 0x11 */ \
    {'o'   , 'O'    }, /* 0x12 */ \
    {'p'   , 'P'    }, /* 0x13 */ \
    {'a'   , 'A'    }, /* 0x14 */ \
    {'r'   , 'R'    }, /* 0x15 */ \
    {'s'   , 'S'    }, /* 0x16 */ \
    {'t'   , 'T'    }, /* 0x17 */ \
    {'u'   , 'U'    }, /* 0x18 */ \
    {'v'   , 'V'    }, /* 0x19 */ \
    {'z'   , 'Z'    }, /* 0x1a */ \
    {'x'   , 'X'    }, /* 0x1b */ \
    {'y'   , 'Y'    }, /* 0x1c */ \
    {'w'   , 'W'    }, /* 0x1d */ \
    {'1'   , '&'    }, /* 0x1e */ \
    {'2'   , '@'    }, /* 0x1f */ \
    {'3'   , '\"'   }, /* 0x20 */ \
    {'4'   , '\''   }, /* 0x21 */ \
    {'5'   , '('    }, /* 0x22 */ \
    {'6'   , 0      }, /* 0x23 */ \
    {'7'   , 0      }, /* 0x24 */ \
    {'8'   , '!'    }, /* 0x25 */ \
    {'9'   , '{'    }, /* 0x26 */ \
    {'0'   , '}'    }, /* 0x27 */ \
    {KBD_KEY_ENTER  , KBD_KEY_ENTER   }, /* 0x28 */ \
    {KBD_KEY_ESC,  KBD_KEY_ESC }, /* 0x29 ESC*/ \
    {KBD_KEY_BS  , KBD_KEY_BS }, /* 0x2a */ \
    {'\t'  , '\t'   }, /* 0x2b */ \
    {' '   , ' '    }, /* 0x2c */ \
    {')'   , 0      }, /* 0x2d */ \
    {'-'   , '_'    }, /* 0x2e */ \
    {'^'   , '['    }, /* 0x2f */ \
    {'$'   , ']'    }, /* 0x30 */ \
    {0     , 0      }, /* 0x31 */ \
    {0     , 0      }, /* 0x32 */ \
    {'m'   , 'M'    }, /* 0x33 */ \
    {0     , '%'    }, /* 0x34 */ \
    {0     , 0      }, /*{'`'   , '~'    },*/ /* 0x35 */ \
    {';'   , '.'    }, /* 0x36 */ \
    {':'   , '/'    }, /* 0x37 */ \
    {'='   , '+'    }, /* 0x38 */ \
    {0     , 0      }, /* 0x39 */ \
    {0     , 0      }, /* 0x3a */ \
    {0     , 0      }, /* 0x3b */ \
    {0     , 0      }, /* 0x3c */ \
    {0     , 0      }, /* 0x3d */ \
    {0     , 0      }, /* 0x3e */ \
    {0     , 0      }, /* 0x3f */ \
    {0     , 0      }, /* 0x40 */ \
    {0     , 0      }, /* 0x41 */ \
    {0     , 0      }, /* 0x42 */ \
    {0     , 0      }, /* 0x43 */ \
    {0     , 0      }, /* 0x44 */ \
    {0     , 0      }, /* 0x45 */ \
    {0     , 0      }, /* 0x46 */ \
    {0     , 0      }, /* 0x47 */ \
    {0     , 0      }, /* 0x48 */ \
    {0     , 0      }, /* 0x49 */ \
    {KBD_KEY_HOME , KBD_KEY_HOME}, /* 0x4a */ \
    {KBD_KEY_PGUP , KBD_KEY_PGUP}, /* 0x4b */ \
    {0     , 0      }, /* 0x4c */ \
    {KBD_KEY_END, KBD_KEY_END}, /* 0x4d */ \
    {KBD_KEY_PGDN , KBD_KEY_PGDN }, /* 0x4e */ \
    {KBD_KEY_RIGHT, KBD_KEY_RIGHT }, /* 0x4f */ \
    {KBD_KEY_LEFT, KBD_KEY_LEFT }, /* 0x50 */ \
    {KBD_KEY_DOWN, KBD_KEY_DOWN }, /* 0x51 */ \
    {KBD_KEY_UP, KBD_KEY_UP }, /* 0x52 */ \
    { 0   , 0       }, /* 0x53 */ \
    { 0   , 0       }, /* 0x54 */ \
    { 0   , 0       }, /* 0x55 */ \
    { 0   , 0       }, /* 0x56 */ \
    { 0   , 0       }, /* 0x57 */ \
    { 0   , 0       }, /* 0x58 */ \
    { 0   , 0       }, /* 0x59 */ \
    { 0   , 0       }, /* 0x5a */ \
    { 0   , 0       }, /* 0x5b */ \
    { 0   , 0       }, /* 0x5c */ \
    { 0   , 0       }, /* 0x5d */ \
    { 0   , 0       }, /* 0x5e */ \
    { 0   , 0       }, /* 0x5f */ \
    { 0   , 0       }, /* 0x60 */ \
    { 0   , 0       }, /* 0x61 */ \
    { 0   , 0       }, /* 0x62 */ \
    { 0   , 0       }, /* 0x63 */ \
    { '<' , '>'     }, /* 0x64 */ \
    { 0   , 0       }, /* 0x65 */ \
    { 0   , 0       }, /* 0x66 */ \
    { 0   , 0       }, /* 0x67 */ \
};


/* =================  End keycode translation table. ================== */

/*===========================================================================
 * is_key_held 
 * Check whether the current key scancode is a repetition of the previous
 * one. One USB report can contain multiple keystrokes, if one key is pressed
 * before another is released. As each new key is pressed, a new report
 * is delivered that contains the keystrokes of the set of keys that
 * are down. As a result, the same keystroke can be found in multiple
 * successive reports.  
 * ========================================================================*/
static inline bool is_key_held (hid_keyboard_report_t const *report, uint8_t keycode)
{
  for (uint8_t i=0; i < 6; i++) 
  {
    if (report->keycode[i] == keycode) 
      return true;
  }
  return false;
}

/*===========================================================================
 * process_kbd_report 
 * Process a report from a keyboard device. The report will contain up
 * to 6 keystrokes, each representing a different key that is down.
 * We need to filter duplicate keystrokes that arise when multiple keys
 * are pressed such that one key is pressed before another is released.
 * This is quite common when typing quickly on a decent keyboard.  
 * ========================================================================*/
static void process_kbd_report (hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} };
  static int prev_ch, prev_chshift, prev_flags, prev_keycode;
  int found=0;
  bool is_lshift_pressed = report->modifier 
    & KEYBOARD_MODIFIER_LEFTSHIFT;
  bool is_lctrl_pressed = report->modifier 
    & KEYBOARD_MODIFIER_LEFTCTRL;
  bool is_lalt_pressed = report->modifier 
    & KEYBOARD_MODIFIER_LEFTALT;  
  bool is_rshift_pressed = report->modifier 
    & (KEYBOARD_MODIFIER_RIGHTSHIFT);
  bool is_rctrl_pressed = report->modifier 
    & KEYBOARD_MODIFIER_RIGHTCTRL;
  bool is_ralt_pressed = report->modifier 
    & KEYBOARD_MODIFIER_RIGHTALT;  
  for (uint8_t i=0; i < 6; i++) 
  {
    if (report->keycode[i]) 
    {
      int ch = conv_table_be[report->keycode[i]][0];
      int chshift = conv_table_be[report->keycode[i]][is_lshift_pressed?1:0];
      int flags = 0;
      if (is_lshift_pressed) flags |= KBD_FLAG_LSHIFT;
      if (is_lctrl_pressed) flags |= KBD_FLAG_LCONTROL;
      if (is_lalt_pressed) flags |= KBD_FLAG_LALT;
      if (is_rshift_pressed) flags |= KBD_FLAG_RSHIFT;
      if (is_rctrl_pressed) flags |= KBD_FLAG_RCONTROL;
      if (is_ralt_pressed) flags |= KBD_FLAG_RALT;
      if (!is_key_held (&prev_report, report->keycode[i])) 
      {        
        // Call back into the application, passing the keystroke and a
        //   set of flags that indicate which modifiers are held down.
        //printf("down %d %d %d\r\n",report->keycode[i], ch, flags);
        /*if (ch)*/ kbd_signal_raw_key(report->keycode[i], ch, chshift, flags, KEY_PRESSED);
        prev_keycode = report->keycode[i];
        prev_ch = ch;
        prev_chshift = chshift;
        prev_flags = flags;
        found = 1;
      } 
    }
  }
  if (!found) 
  {
    if (prev_keycode) 
    {
      //printf("up %d %d %d\r\n",prev_keycode, prev_ch, prev_flags);
      kbd_signal_raw_key(prev_keycode, prev_ch, prev_chshift, prev_flags, KEY_RELEASED);
      prev_keycode = 0;
    }     
  }  
  prev_report = *report;
}
 
//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

void hid_app_task(void)
{
  // nothing to do
}

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);
  printf("VID = %04x, PID = %04x\r\n", vid, pid);

  // Sony DualShock 4 [CUH-ZCT2x]
  if ( is_sony_ds4(dev_addr) )
  {
    // request to receive report
    // tuh_hid_report_received_cb() will be invoked when report is available
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
      printf("Error: cannot request to receive report\r\n");
    }
  }

  /* Ask for a report only if this is a keyboard device */
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) 
  {
    printf("Keyboard found\n");
    tuh_hid_receive_report (dev_addr, instance);
  }

}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
}

// check if different than 2
bool diff_than_2(uint8_t x, uint8_t y)
{
  return (x - y > 2) || (y - x > 2);
}

// check if 2 reports are different enough
bool diff_report(sony_ds4_report_t const* rpt1, sony_ds4_report_t const* rpt2)
{
  bool result;

  // x, y, z, rz must different than 2 to be counted
  result = diff_than_2(rpt1->x, rpt2->x) || diff_than_2(rpt1->y , rpt2->y ) ||
           diff_than_2(rpt1->z, rpt2->z) || diff_than_2(rpt1->rz, rpt2->rz);

  // check the reset with mem compare
  result |= memcmp(&rpt1->rz + 1, &rpt2->rz + 1, sizeof(sony_ds4_report_t)-4);

  return result;
}

void process_sony_ds4(uint8_t const* report, uint16_t len)
{
  const char* dpad_str[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW", "none" };

  // previous report used to compare for changes
  static sony_ds4_report_t prev_report = { 0 };

  uint8_t const report_id = report[0];
  report++;
  len--;

  // all buttons state is stored in ID 1
  if (report_id == 1)
  {
    sony_ds4_report_t ds4_report;
    memcpy(&ds4_report, report, sizeof(ds4_report));

    // counter is +1, assign to make it easier to compare 2 report
    prev_report.counter = ds4_report.counter;

    // only print if changes since it is polled ~ 5ms
    // Since count+1 after each report and  x, y, z, rz fluctuate within 1 or 2
    // We need more than memcmp to check if report is different enough
    if ( diff_report(&prev_report, &ds4_report) )
    {
      printf("(x, y, z, rz) = (%u, %u, %u, %u)\r\n", ds4_report.x, ds4_report.y, ds4_report.z, ds4_report.rz);
      printf("DPad = %s ", dpad_str[ds4_report.dpad]);

      if (ds4_report.square   ) printf("Square ");
      if (ds4_report.cross    ) printf("Cross ");
      if (ds4_report.circle   ) printf("Circle ");
      if (ds4_report.triangle ) printf("Triangle ");

      if (ds4_report.l1       ) printf("L1 ");
      if (ds4_report.r1       ) printf("R1 ");
      if (ds4_report.l2       ) printf("L2 ");
      if (ds4_report.r2       ) printf("R2 ");

      if (ds4_report.share    ) printf("Share ");
      if (ds4_report.option   ) printf("Option ");
      if (ds4_report.l3       ) printf("L3 ");
      if (ds4_report.r3       ) printf("R3 ");

      if (ds4_report.ps       ) printf("PS ");
      if (ds4_report.tpad     ) printf("TPad ");

      printf("\r\n");
    }

    prev_report = ds4_report;
  }
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance; (void) len;
  // In principle we don't need to test that this USB report came from
  //   a keyboard, since we are only asking for reports from keyboards.
  // But, for future expansion, we should be systematic
  switch (tuh_hid_interface_protocol (dev_addr, instance)) 
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      process_kbd_report ((hid_keyboard_report_t const*) report);
      break;
  }

  // Ask the device for the next report -- asking for a report is a
  //   one-off operation, and must be repeated by the application. 
  tuh_hid_receive_report (dev_addr, instance);

  /*
  if ( is_sony_ds4(dev_addr) )
  {
    process_sony_ds4(report, len);
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request to receive report\r\n");
  }
  */
}
