## build procedure (PICOW only required for standalone emu mode with tftp over WIFI)
# install PICO-SDK (or update it by pulling)
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk/
git submodule update --init
export PICO_SDK_PATH=/Users/jean-marcharvengt/Documents/pico/pico-sdk (path to pico-sdk!)
# build (you cloned this project!)
cd hyperpetpico
edit CMakeLists.txt and uncomment proper target
#set(TARGET hyperpetpico)        => to use in a real PET
#set(TARGET hyperpetpicoemu)     => to use as standalone emu without wifi
#set(TARGET hyperpetpicoemuwifi) => to use as standalone emu with wifi (picow only)
mkdir build
cd build
picow: cmake -DPICO_BOARD=pico_w ..
pico : cmake .. 
make
 
## connect over WiFi (picow standalone)
accesspoint :	hyperpetpico
passwd      :   picopet123
IP address  :	192.168.123.1

## commpile 6502 samples (on PC/MAC/Linux)
download and install acme from https://github.com/meonwax/acme
cd prgs
./acme <example>.a

## upload 6502 samples (standalone emulator mode only)
connect your PC to the "hyperpetpico" access point
tftp 192.168.123.1
binary                       => Set transfer to binary more
put <example>.a.prg          => LIST and RUN program
put <example>.a.prg reset    => Reset the PET
put <example>.a.prg key      => Send a KEY to PET
q                            => Quit


## compress bitmap for PET
resize your image to 320x200 and save as JPG
convert to "binary RGB332" as "convert.bin" using https://lvgl.io/tools/imageconverter
skip first 4 bytes: tail -c +5 convert.bin > bitmap.bin 
compress using pucrunch: ./pucrunch -c0 -d bitmap.bin bitmap.cru
MUST be less than 28K to fit on PET8032/4032.