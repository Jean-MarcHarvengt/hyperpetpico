# HyperPET pico expansion for PET 4032/8032
The commodore PET has always been for me the father of all Personal Computers.<br>
The model 8032 remains a impressive machine (professional keyboard, 80 columns, 32K RAM...)<br>
<br>
Today the PET deserves a modern hardware expansion board!<br>
The HyperPET pico project intents to upgrade the PET to a modern 6502 system, with improved graphics and sound capabilities.<br>
<br>
Have a look at the YouTube videos:<br>
[HyperPET pico Introduction](https://youtu.be/iDc6OViZffg?si=Qh3PRVBwq7JelLfL)<br>
[HyperPET pico In Standalone Mode](https://youtu.be/M_f5vOrPuFs?si=RCkrxrRP44IA1p1Q)<br>
[HyperPET pico Assembling the Inner-Board](https://youtu.be/Ujg4q-dqkjs?si=gjwIFBtap3c-Fr7f)<br>
[HyperPET pico Development Tutorial (Part1)](https://youtu.be/D6H2RW0BwQ0?si=l-4htJ7oOXDetJPa)<br>
[HyperPET pico Development Tutorial (Part2: sprites graphical objects)](https://youtu.be/8ZjlqQmJXdo?si=lpqTXvzPPhYjKBgU)<br>
[HyperPET pico Development Tutorial (Part3: input controls, sprites collision and more)](https://youtu.be/Cvhr81-Nsuc?si=LmOhA7yWKkGvkm37)<br>
[HyperPET pico Development Tutorial (Part4: font and bitmap graphical objects)](https://youtu.be/PuI7KokQmL0?si=zpOtQVOJBIqyxxmz
)<br>

<br>
Initially the purpose of the project was to offer VGA output to the PET.<br>
At this point of time, the HyperPET pico project supports:
<br>

* VGA output up to 640x200 (80 colums)
* extended graphical modes including (640/320/256)x200 resolutions in 256 colors.
* 16 "flippable" SPRITES (reusable up to 96)
* dual layers: 2 TILES layers (8x8 or 16x16)
* dual layers: 1 TILES layer + PET TEXT layer
* dual layers: 1 BITMAP layer (max 320x200, 256 colors) + 1 TILES layer
* background layer: plain color or line raster colors
* smooth scrolling in all layers (horizontal and verstical)
* SID sound emulation
* extra 4K RAM in $a000 (1 bank, may be more in future!)
* OR selectable ROM in $a000 (via resident File Browser)
* 1MB flash storage for programs (ro), accessible via the resident File Browser (sys40960)
* possibility to emulate the EDIT ROM (e.g. 60Hz in a PAL machine)
* possibility to toggle 80 to 40 columns on the real PET monitor (AnyHZ routine via the resident File Browser)

<br>

The HyperPET pico exists in 2 boards<br>
* The inner board (residing inside the PET) multiplexing the memory expansion connectors to the outside
* The outer board (HyperPET pico module), as a plugin module, offering all the new features (with VGA/audio output)

<br>

The picture on the PET monitor is mirrored to VGA by default (80/40 columns modes)<br>
Programs can be loaded (via tape emulation, or modern devices as PETdisk or PETdisk MAX 2 or the resident File Browser)<br>
As soon a program make uses of extended graphics capabilities, only VGA output will show it<br>
The rest of the PET hardware is just used normally (CPU, RAM, ROM, keyboard, PIA, VIA ...)<br>
New registers are available through the memory region $9000-$9FFF (R/W)<br>
The region $A000-$AFFF is available as extra RAM (R/W) or to store custom ROMs (the File Browser being the default).
The region $E000-$E7FF can be optionnaly emulated if the EDIT rom is removed from its socket.
At any moment, the resident File Browser can be invoked using the command sys40960 from the BASIC.

<br>

To ease development, the plugin module can be used as a standalone PET system<br>
* the CPU, RAM (32K), ROM, PIA, VIA are emulated
* a USB keyboard can be used to interract (OTG adapter)
* all new GFX/sound features can be used for development 
* developed programs can be injected over WiFI for testing (via TFTP server)

## Initial prototypes
<p align="left">
<img src="/images/proto1_1.jpg" width="320" height="260"  />  
<img src="/images/proto1_2.jpg" width="240" height="260" />  
<img src="/images/proto2.png" width="240" height="260" />  
</p>

## Installation procedure
* all PICO binaries are part of the repository in the "/bin" subdir.
* format the 1MB hyperpet flash storage (one time operation)
  * power the PICO while pressing the button (uf2 programming mode)
  * program the "filesystem exposure utility" by drag and drop of the file "/bin/flashfs_mount_or_create+mount.uf2" on the PICO storage
  * When the flashing is finished, another mass storage icon HP-PICOCARD will appear representing the 1MB storage available for the hyperpet
  * by default HP-PICOCARD contains the file "HYPERPET.CFG".
  * this file contains the default configuration. You can change the options in the file using an editor (e.g. boot in 40 colums if you have a 4032 PET) 
* drag and drop all the programs you want to the HP-PICOCARD USB storage device
  * the "/prgs" subdir contains few examples, you can copy for e.g. basic,demos,petscii and roms subdirectories to HP-PICOCARD
* finally flash the hyperpetpico application to the PICO
  * power the PICO while pressing the button (uf2 programming mode)
  * drag and drop "bin/hyperpetpicopetio.uf2" if you intent to use the HyperPET module on a real PET
  * OR drag and drop "bin/hyperpetpicoemuwifi.uf2" if you intent to use the module as standalone emulator
* the HyperPET module is ready to use! 
* on a real pet:
  * install the innerboard inside the PET8032, on the memory expansion slots
  * cut the 9xxx pullup and connect the inner mudule CS wire to PET PCB (see picture)
  * plug the HyperPET module to the inner board connector comming at the side of the pet
  * connect VGA and audio 
  * power the HyperPET module via USB cable
  * finally (and only at the end), power the PET
* as standalone system:
  * connect VGA and audio 
  * connect a USB keyboard via a USB power splitter cable (keyboard layout via "HYPERPET.CFG")
  * power the HyperPET module via USB    

## Build procedure
* install the ARM compiler (see PICO documentation)
* install PICO-SDK (or update it by pulling from the github)
  * git clone -b master https://github.com/raspberrypi/pico-sdk.git
  * cd pico-sdk/
  * git submodule update --init
* export PICO_SDK_PATH=/Users/jean-marcharvengt/Documents/pico/pico-sdk (e.g. path to pico-sdk!)
* clone this project
  * git clone git@github.com:Jean-MarcHarvengt/hyperpetpico.git
  * cd hyperpetpico
  * edit CMakeLists.txt and uncomment desired target
    * #set(TARGET hyperpetpico)        => to use in a real PET
    * #set(TARGET hyperpetpicoemu)     => to use as standalone emu without wifi
    * #set(TARGET hyperpetpicoemuwifi) => to use as standalone emu with wifi (picow only)
  * mkdir build
  * cd build
  * picow: cmake -DPICO_BOARD=pico_w ..
  * pico : cmake .. 
  * make

## Connect over WiFi to the HyperPET pico standalone module (with picow)
* accesspoint :	hyperpetpico
* passwd      :   picopet123
* IP address  :	192.168.123.1
* tftp 192.168.123.1
  * binary
  * put "myprogram.prg"       => will run the program immediately
  * put "myprogram.prg" reset => will reset the PET emulation
  * put "myprogram.prg" key   => will simulate a key press (space key)

## Hardware modification
* this is required to allow the CPU to read the memory range $9000-$FFFF on the bus expansion and not only $9000-$9fff 
* as such, emulate ROMs in $A000 and above (if not in socket)
<p align="left"> 
<img src="/images/mod.png" width="320" height="240" />  
</p>

## HYPERPET PICO Memory Map
|  Register        | Address    | Data                            | Description                                                             |
| ---------------- | -------    | ------------------------------- | ----------------------------------------------------------------------- |
| REG_TEXTMAP_L1   | 8000-87ff  | PETSCI chars      (0-255)       | petfont text map in L1 (0x400 for 4032, 0x800 for 8032)|
| REG_TILEMAP_L1   | 8800-8fff  | tile id (0-255/0-63) 8x8/16x16  | tiles map in L1 |
| REG_TILEMAP_L0   | 9000-97ff  | tile id (0-255/0-63) 8x8/16x16  | tiles map in L0 |
| REG_SPRITE_IND   | 9800-9860  | bits0-5: id (max 63) <br> bit6: hflip <br> bit7: vflip | sprite id (96 sprites) |
| REG_SPRITE_XHI   | 9880-98e0  | bits0-7: x (hi-byte) | sprite X coord HI (96 sprites) |
| REG_SPRITE_XLO   | 9900-9960  | bits0-7: x (lo-byte) | sprite X coord LO (96 sprites) |
| REG_SPRITE_Y     | 9980-99e0  | bits0-7: y | sprite Y coord (96 sprites) |
| REG_TLOOKUP      | 9a00-9aff  | 1/2/4/8/N bytes as 1/4/8/16..256 RGB332 colors | CLUT Palette <br> Also used as transfer parameters extension|
| REG_VIDEO_MODE   | 9b00       | 0 = 640x200 <br> 1 = 320x200 <br> 2 = 256x200 <br>| video mode (resolution) |
| REG_BG_COL       | 9b01       | bits0-7: color (RGB332) <br> bits5-7, R 0x20 -> 0xe0 <br> bits2-4, G 0x04 -> 0x1c <br> bits0-1, B 0x00 -> 0x03 | background color |
| REG_FG_COL       | 9b0d       | bits0-7: color (RGB332) | foreground/text color |
| REG_LAYERS_CFG   | 9b02       | bit0: L0 on/off (1=on) <br> bit1: L1 on/off (1=on) <br> bit2: L2 on/off (1=on) <br> bit3: L2 inbetween (0 = sprites top) <br> bit4: bitmap/tile in L0 (0=bitmap) <br> bit5: petfont/tile in L1 (0=petfont) <br> bit6: enable scroll area in L0 <br> bit7: enable scroll area in L1| layers configuration |
| REG_TILES_CFG    | 9b0e       | | |
| REG_LINES_CFG    | 9b03       | | |
| REG_XSCROLL_HI   | 9b04       | | |
| REG_XSCROLL_L0   | 9b05       | | |
| REG_XSCROLL_L1   | 9b06       | | |
| REG_YSCROLL_L0   | 9b07       | | |
| REG_YSCROLL_L1   | 9b08       | | |
| REG_SC_START_L0  | 9b09       | | |
| REG_SC_END_L0    | 9b0a       | | |
| REG_SC_START_L1  | 9b0b       | | |
| REG_SC_END_L1    | 9b0c       | | |
| REG_VSYNC        | 9b0f       | | |
| REG_TDEPTH       | 9b10       | | |
| REG_TCOMMAND     | 9b11       | | |
| REG_TPARAMS      | 9b12       | | |
| REG_TDATA        | 9b13       | | |
| REG_TSTATUS      | 9b14       | | |
| REG_?????????????| 9b15-9b17  | | |
| REG_LINES_BG_COL | 9b38-9bff  | | |
| REG_LINES_XSCR_HI| 9c00-9cc7  | | |
| REG_LINES_L0_XSCR| 9cc8-9d8f  | | |
| REG_LINES_L1_XSCR| 9d90-9e57  | | |
| REG_?????????????| 9e60-9eff  | | |
| REG_SPRITE_COL_LO| 9f00-9f7f  | | |
| REG_SPRITE_COL_HI| 9f80-9fff  | | |
| REG_SID_BASE     | 9b18-9b37  | | |

## Special credits
Hyperpetpico reuse or is inspired from the code of below projects
* [PicoVGA](https://github.com/Panda381/PicoVGA)
* [AppleII-VGA](https://github.com/markadev/AppleII-VGA)
* Tftp server on Raspberry PICO from Damien P. George
* [Pucrunch compressor for 6502 CPU](https://github.com/mist64/pucrunch)
* [reSID emulator](https://en.wikipedia.org/wiki/ReSID)
* [Teensy-reSID](https://github.com/FrankBoesing/Teensy-reSID)
* [A8PicoCart](https://github.com/robinhedwards/A8PicoCart)
* [MOS6502](https://github.com/gianlucag/mos6502)

