#define __ASSEMBLER__
#include "config.h"
#include "cpu.h"

ENTRY(start)

OUTPUT_FORMAT(elf32-littlearm)
OUTPUT_ARCH(arm)
STARTUP(target/arm/s5l8702/crt0.o)

#define PLUGINSIZE PLUGIN_BUFFER_SIZE
#define CODECSIZE CODEC_SIZE

#define IRAMORIG 0x0
#define DRAMORIG 0x08000000

/* End of the audio buffer, where the codec buffer starts */
#define ENDAUDIOADDR  (DRAMORIG + DRAMSIZE)

#define DRAMSIZE (DRAM_SIZE - PLUGINSIZE - CODECSIZE - TTB_SIZE)
#define CODECORIG (ENDAUDIOADDR)
#define IRAMSIZE (48*1024)  /* 256KB total - 48KB for core, 200KB for codecs */

/* Where the codec buffer ends, and the plugin buffer starts */
#define ENDADDR (ENDAUDIOADDR + CODECSIZE)

MEMORY
{
    IRAM : ORIGIN = IRAMORIG, LENGTH = IRAMSIZE
    DRAM : ORIGIN = DRAMORIG, LENGTH = DRAMSIZE
}

SECTIONS
{
    loadaddress = DRAMORIG;

    .intvect : {
        _intvectstart = . ;
        *(.intvect)
        _intvectend = _newstart ;  
    } >IRAM AT> DRAM
    _intvectcopy = LOADADDR(.intvect) ;

    .text :
    {
        _loadaddress = .;
        _textstart = .;
        *(.init.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(0x4);
    } > DRAM

    .rodata :
    {
        *(.rodata*)
        . = ALIGN(0x4);
    } > DRAM

    .data :
    {
        *(.data*)
        . = ALIGN(0x4);
    } > DRAM

    /DISCARD/ :
    {
        *(.eh_frame)
    }

    .iram :
    {
        _iramstart = .;
        *(.icode*)
        *(.irodata*)
        *(.idata*)
        . = ALIGN(0x4);
        _iramend = .;
    } > IRAM AT> DRAM
    _iramcopy = LOADADDR(.iram) ;

    .ibss (NOLOAD) :
    {
        _iedata = .;
        *(.qharray)
        *(.ibss*)
        . = ALIGN(0x4);
        _iend = .;
    } > IRAM

    .stack (NOLOAD) :
    {
       *(.stack)
       stackbegin = .;
       _stackbegin = .;
       /* Metro (M-101): main thread stack 8 KB -> 12 KB. The UI thread
          carries frames that Rockbox never had (JPEG decode + tagcache
          walks under one lock, master-art derivation), and Aura-Firmware
          -- same app.lds, same SoC -- took a real `Stkov main` panic with
          the 8 KB figure. Core IRAM is 48 KB (0xC000); with +0x1000 the
          FIQ stack still ends at 0xb530, leaving ~2.7 KB free. */
       . += 0x3000;
       stackend = .;
       _stackend = .;
       _irqstackbegin = .;
       . += 0x400;
       _irqstackend = .;
       _fiqstackbegin = .;
       . += 0x400;
       _fiqstackend = .;
    } > IRAM

    .bss (NOLOAD) :
    {
       _edata = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(0x4);
       _end = .;
    } > DRAM

    .audiobuf (NOLOAD) :
    {
        . = ALIGN(4);
        _audiobuffer = .;
        audiobuffer = .;
    } > DRAM

    .audiobufend ENDAUDIOADDR (NOLOAD) :
    {
        audiobufend = .;
        _audiobufend = .;
    } > DRAM

    .codec CODECORIG (NOLOAD) :
    {
        codecbuf = .;
        _codecbuf = .;
    } > DRAM

    .plugin ENDADDR (NOLOAD) :
    {
        _pluginbuf = .;
        pluginbuf = .;
    }
}
