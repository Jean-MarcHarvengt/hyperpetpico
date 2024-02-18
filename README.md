# hyperpetpico for PET 4032/8032
The commodore PET is the father of all personal computers.<br>
The model 8032 remains a impressive machine (great keyboard, 80 colums, 32K RAM...)<br>
<br>
Today the PET deserves a modern hardware expansion board!<br>
This project intents to upgrade the PET to a modern 6502 computer.<br>
<br>
Initially the purpose of the project was to offer VGA output to the PET.<br>
At this point of time, the hyperpetpico project supports:<br>
* VGA output up to 640x200 (80 colums)
* SID sound emulation
* extended graphical modes supporting (640/320/256)x200 resolution in 256 colors.
* 16 sprites (reusable up to 128)
* dual layers: 2 tiles layers or tiles+text with smooth scrolling
* 320x200 bitmap mode (256 colors) on the lower layer
* background color/raster colors
* [RAM remains as original (4 to 32K)!]
* [No storage is offered!]

<br>

The hyperperpico exists as 2 modules<br>
* a first module (residing inside the PET) multiplexing the memory expansion connectors to the outside
* a second module, as a plugin board, offering all the new features (VGA/audio connectors)

<br>

The picture on the PET monitor is mirrored to VGA by default.<br>
Programs can be loaded (via tape emulation, or modern devices as PETdisk or PETdisk MAX 2)<br>
As soon a program make uses of extended graphics capabilities, only VGA is usable<br>
The rest of the PET hardware is just used for its CPU,RAM,ROM,keyboard,PIA,VIA ...
<br>

To ease development, the second module can be used as a standalone PET emulator<br>
* the CPU,RAM,ROM,PIA,VIA are emulated
* a OTG USB keyboard can be used
* new GFX/sound features are available for development 
* developed programs can be injected over WiFI for testing

## Initial prototypes
<p align="center">
<img src="/images/proto1_1.jpg" width="200" height="260"  />  
<img src="/images/proto1_2.jpg" width="200" height="260" />  
<img src="/images/proto2.png" width="200" height="260" />  
</p>

## Build procedure
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
* [MOS6502](https://github.com/gianlucag/mos6502)

