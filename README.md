# hyperpetpico for PET 4032/8032
The commodore PET is the father of all personal computers.<br>
The model 4032/8032 remains impressive machines (great keyboard, 80 colums...)<br>
<br>
Today the PET deserves a modern hardware expansion board!<br>
This project intent to upgrade the PET to a modern 6502 computer.<br>
<br>
Initially the purpose of the project was only to offer VGA output.<br><br>
At this point of time, the hyperpetpico project supports<br>
* VGA output up to 640x200 (80 colums)
* SID sound emulation
* extended graphical modes supporting (640/320/256)x200 resolution in 256 colors.
* 16 sprites (reusable up to 128)
* dual layers tiles mode or tiles + text with smooth scrolling
* 320x200 bitmap mode (256 colors)
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
* new GFX/sound features are available for new development 
* developed programs can be injected over WiFI for testing
<br>

