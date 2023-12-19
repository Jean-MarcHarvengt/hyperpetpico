# hyperpetpico for PET 4032/8032
The commodore PET is the grand father of all personal computers.<br>
The model 4032/8032 remains impressive machines per today (great keyboard, 80 colums...)<br>
Today the PET also deserves its modern hardware extention!<br>
<br>
This project intent to upgrade the PET to a late 80s computer.<br>
Initially the purpose was to offer a VGA output.<br><br>
At this point of time, the hyperpetpico project supports<br>
* VGA output up to 640x200 (80 colums)
* SID sound emulation
* extended graphical modes supporting (640/320/256)x200 resolution in 256 colors.
* 16 (reusable) sprites
* dual layers tiles mode or tiles + text with smooth scrolling
* bitmap mode
* [Memory remains as original (4 to 32K)]

<br>

The hyperperpico exists as 2 modules<br>
* a first module (residing inside the PET) multiplexing the memory expansion connector to the outside
* a second module as a plugin board offering the new features (with VGA/audio connectors)

<br>

The picture of the integrated monitor is mirrored to the VGA by default.<br>
Programs can be loaded (via tape emulation, or modern as PETdisk or PETdisk MAX 2)<br>
As soon a program make uses of extended graphics capabilities, only VGA is usable<br>
The rest of the PET hardware is used (CPU, keyboatd, ...)
<br>

To ease development, the second module can be used as a standalone PET emulator<br>
* developed programs can be injected over WiFI for testing
* terminal can be used a keyboard input
* all new features are available, only the CPU is emulated
<br>

