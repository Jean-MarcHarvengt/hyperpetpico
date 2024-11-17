cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

#set(TARGET hyperpetpicopetio)
set(TARGET hyperpetpicoemu)
#set(TARGET hyperpetpicoemuwifi)

project(${TARGET} C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

OPTION(PICO_PLATFORM "Build specific pico platform" OFF)

# Initialize the Pico SDK
pico_sdk_init()

include_directories(.)
include_directories(_vga)
include_directories(_hdmi)
include_directories(_gfx)
include_directories(_audio)
include_directories(_resid)
include_directories(_petemu)
include_directories(_petbus)
include_directories(_network)
include_directories(_usb_kbd)
include_directories(_decomp)
include_directories(_flashfs)
include_directories(_flashfs/fatfs)
include_directories(_usb_serial)

if( ${TARGET} MATCHES "hyperpetpicopetio" )
add_compile_definitions(${TARGET} PUBLIC HAS_PETIO=1)
endif()

if( ${TARGET} MATCHES "hyperpetpicoemuwifi" )
add_compile_definitions(${TARGET} PUBLIC HAS_NETWORK=1)
endif()

set(MAIN_SOURCES
		main.cpp
	)


set(GFXRENDER_SOURCES
		_gfx/gfx_tile8.S
		_gfx/gfx_tile16.S
		_gfx/gfx_text8.S
		_gfx/gfx_sprite16.S
		_gfx/gfx_color.S
	)


if(PICO_PLATFORM MATCHES "rp2040")
add_compile_definitions(${TARGET} PUBLIC ISRP2040=1)
endif()
if(PICO_PLATFORM MATCHES "rp2350")
add_compile_definitions(${TARGET} PUBLIC ISRP2350=1)
endif()


if(PICO_PLATFORM MATCHES "rp2350")
set(GFXOUTPUT_SOURCES
		_gfx/gfx_output.cpp
		_hdmi/hdmi.cpp
	)
else()
set(GFXOUTPUT_SOURCES
		_gfx/gfx_output.cpp
		_vga/vga.cpp
		_vga/vga_vmode.cpp
		_vga/util/overclock.cpp
	)
endif()


set(AUDIO_SOURCES
		_audio/pwm_audio.cpp
	)

set(SID_SOURCES 
		_resid/sid.cpp
		_resid/reSID.cpp	
	)

set(PETBUS_SOURCES 
		_petbus/petbus.cpp		
	)

set(PETEMU_SOURCES 
		_petemu/mos6502.cpp		
	)

if( ${TARGET} MATCHES "hyperpetpicoemuwifi" )
set(NETWORK_SOURCES 	
		_network/network.cpp
		_network/dhcpserver.c
	)
endif()

if( NOT ${TARGET} MATCHES "hyperpetpicopetio" )
set(USB_SOURCES 	
		_usb_kbd/hid_app.c
	)
endif()

set(DECOMP_SOURCES 	
		_decomp/decrunch.c
	)

set(FF_SOURCES
#		_flashfs/flash.c
		_flashfs/fatfs_disk_ro.c
		_flashfs/fatfs/ff.c	
		_flashfs/fatfs/diskio.c
		_flashfs/fatfs/ffunicode.c
	)

if( NOT ${TARGET} MATCHES "hyperpetpicopetio" )
if(PICO_PLATFORM MATCHES "rp2350")
set(USBSERIAL_SOURCES
		_usb_serial/dev_lowlevel.c
	)
endif()
endif()

add_executable(${TARGET}
		${MAIN_SOURCES}
		${GFXRENDER_SOURCES}
		${GFXOUTPUT_SOURCES}
		${AUDIO_SOURCES}
		${SID_SOURCES}
		${PETEMU_SOURCES}
		${PETBUS_SOURCES}
		${DECOMP_SOURCES}		
		${NETWORK_SOURCES}
		${FF_SOURCES}
		${USB_SOURCES}
		${USBSERIAL_SOURCES}
	)

pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/_petbus/petbus.pio)
pico_generate_pio_header(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/_vga/vga.pio)

target_link_libraries(${TARGET} 
        pico_multicore
        pico_stdlib
	hardware_pio
        hardware_pwm
        hardware_dma
        hardware_flash        
	)

if( NOT ${TARGET} MATCHES "hyperpetpicopetio" )
target_link_libraries(${TARGET}
#        tinyusb_device
        tinyusb_host 
        tinyusb_board
	)
endif()

if( ${TARGET} MATCHES "hyperpetpicoemuwifi" )
target_link_libraries(${TARGET}
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_tftp
	)
endif()

if(PICO_PLATFORM MATCHES "rp2350")
#if( ${TARGET} MATCHES "hyperpetpicoemu" )
target_link_libraries(${TARGET} 
	tinyusb_pico_pio_usb
	)
target_compile_definitions(${TARGET} PRIVATE
        PIO_USB_DP_PIN_DEFAULT=2
        )
endif()

target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

#target_compile_definitions(${TARGET} PRIVATE CFG_TUSB_DEBUG=2)
#target_compile_options(${TARGET} PUBLIC -Ofast)
#target_compile_options(${TARGET} PUBLIC -O)
#target_compile_options(${TARGET} PUBLIC -O2)
#target_compile_options(${TARGET} PUBLIC -O3)
#target_compile_options(${TARGET} PUBLIC -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
#target_compile_definitions(${TARGET} PRIVATE
#	PICO_CORE1_STACK_SIZE=0x200	
#	)

pico_enable_stdio_uart(${TARGET} TRUE)
#pico_enable_stdio_usb(${TARGET} TRUE)

#suppress_pico_sdk_warnings()


pico_add_extra_outputs(${TARGET})
