## build procedure (PICOW only required for standalone emu mode with tftp over WIFI)
# install PICO-SDK (or update it by pulling)
pico-sdk 2.0 (for RP2350, pico2)
--------------------
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk/
cd lib
mv tinyusb tinyusb.old
git clone https://github.com/hathach/tinyusb.git
cd tinyusb
python3 tools/get_deps.py rp2040
cd ..
git submodule update --init
export PICO_SDK_PATH=/Users/jean-marcharvengt/Documents/pico/pico-sdk (path to pico-sdk!)

pico-sdk 1.0 (for RP2040, pico or pico-w)
--------------------
Download pico-sdk-1.5.1.tar.gz from https://github.com/raspberrypi/pico-sdk/releases
tar xfvz pico-sdk-1.5.1.tar.gz
export PICO_SDK_PATH=/Users/jean-marcharvengt/Documents/pico/pico-sdk-1.5.1 (path to pico-sdk!)


# build (you cloned this project!)
cd hyperpetpico
edit CMakeLists.txt and uncomment proper target
#set(TARGET hyperpetpico)        => to use in a real PET (pico,picow and pico2)
#set(TARGET hyperpetpicoemu)     => to use as standalone emu without wifi (pico and pico2)
#set(TARGET hyperpetpicoemuwifi) => to use as standalone emu with wifi (picow only)
mkdir build
cd build
pico : cmake .. 
picow: cmake -DPICO_BOARD=pico_w ..
pico2: cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..
make
 
## connect over WiFi (picow standalone)
accesspoint :	hyperpetpico
passwd      :   picopet123
IP address  :	192.168.123.1

## commpile 6502 samples (on PC/MAC/Linux)
download and install acme from https://github.com/meonwax/acme
cd prgs
./acme <example>.a

## upload 6502 samples (standalone emulator mode only picow)
connect your PC to the "hyperpetpico" access point
tftp 192.168.123.1
binary                       => Set transfer to binary more
put <example>.a.prg          => LIST and RUN program
put <example>.a.prg reset    => Reset the PET
put <example>.a.prg key      => Send a KEY to PET
q                            => Quit

## upload 6502 samples (standalone emulator mode only pico2)
connect pico2 USB micro to PC via USB (data/prog cable)
Use vkey application in tools

## compress bitmap for PET
resize your image to 320x200 and save as JPG
convert to "binary RGB332" as "convert.bin" using https://lvgl.io/tools/imageconverter
skip first 4 bytes: tail -c +5 convert.bin > bitmap.bin 
compress using pucrunch: ./pucrunch -c0 -d bitmap.bin bitmap.cru
MUST be less than 28K to fit on PET8032/4032.