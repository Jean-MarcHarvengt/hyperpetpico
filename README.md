# HyperPET pico expansion for PET 4032/8032
The commodore PET is the father of all personal computers.<br>
The model 8032 remains a impressive machine (professional keyboard, 80 colums, 32K RAM...)<br>
<br>
Today the PET deserves a modern hardware expansion board!<br>
The HyperPET pico project intents to upgrade the PET to a modern 6502 system, with improved graphics and sound capabilities.<br>
<br>
Initially the purpose of the project was to offer VGA output to the PET.<br>
At this point of time, the HyperPET pico project supports:<br>
* VGA output up to 640x200 (80 colums)
* SID sound emulation
* extended graphical modes including (640/320/256)x200 resolutions in 256 colors.
* 16 flippable sprites (reusable up to 128)
* dual layers: 2 tiles layers or tiles+text with smooth scrolling
* 320x200 bitmap mode (256 colors) on the lower layer
* background color/raster colors
* extra 4K RAM in $a000 (so far 1 bank, may be more in future!)
* OR selectable ROM in $a000 (via File Browser)
* 1MB flash storage (ro) accessible via the resident File Browser (sys40960)
* possibility to emulate the EDIT ROM (e.g. 60Hz in a PAL machine)

<br>

The HyperPET pico exists in 2 boards<br>
* The inner board (residing inside the PET) multiplexing the memory expansion connectors to the outside
* The outer board, as a plugin module, offering all the new features (VGA/audio connectors)

<br>

The picture on the PET monitor is mirrored to VGA by default.<br>
Programs can be loaded (via tape emulation, or modern devices as PETdisk or PETdisk MAX 2 or the resident File Browser)<br>
As soon a program make uses of extended graphics capabilities, only VGA will show it<br>
The rest of the PET hardware is just used for its CPU, RAM, ROM, keyboard, PIA, VIA ...<br>
New registers are available through the $9000-$9FFF region (R/W)<br>
The region $A000-$AFFF is available as extra RAM or to store custom ROMs (File Browser as default).

<br>

To ease development, the plugin module can be used as a standalone PET system<br>
* the CPU, RAM (32K), ROM, PIA, VIA are emulated
* a OTG USB keyboard can be used to interract
* all new GFX/sound features can be used for development 
* developed programs can be injected over WiFI for testing (via TFTP server)

## Initial prototypes
<p align="left">
<img src="/images/proto1_1.jpg" width="240" height="260"  />  
<img src="/images/proto1_2.jpg" width="240" height="260" />  
<img src="/images/proto2.png" width="240" height="260" />  
</p>

## Installation procedure
* all PICO binaries are part of the repository in the "/bin" subdir.
* refer to the PICO documentation for more details about how to flash it
* format the 1MB hyperpet flash storage (one time operation)
  * power the PICO while pressing the button (uf2 programming mode)
  * program the "filesystem exposure utility" by drag and drop of the file "/bin/flashfs_mount_or_create+mount.uf2" on the PICO storage
  * When the flashing is finished, another mass storage icon HP-PICOCARD will appear representing the 1MB available for the hyperpet
  * by default HP-PICOCARD contains the file "HYPERPET.CFG".
  * this file contains the default configuration. You can change the options in the file using an editor (e.g. boot in 40 colums if you have a 4032 PET) 
* drag and drop all the programs you want to the HP-PICOCARD USB storage device
  * the "/prgs" subdir contains few examples, you can copy for e.g. basic,demos,petscii and roms subdirectories to HP-PICOCARD
* flash the hyperpetpico application to the PICO
  * power the PICO while pressing the button (uf2 programming mode)
  * drag and drop "bin/hyperpetpicopetio.uf2" if you intent to use the HyperPET module on a real PET
  * OR drag and drop "bin/hyperpetpicoemuwifi.uf2" if you intent to use the module as standalone emulator
* the HyperPET module is ready to use! 
* on a real pet:
  * install the innerboard inside the PET8032, on the memory expansion slots
  * cut the 9xxx pullup and connect the CS wire (see picture)
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

## connect over WiFi (picow standalone)
* accesspoint :	hyperpetpico
* passwd      :   picopet123
* IP address  :	192.168.123.1

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

