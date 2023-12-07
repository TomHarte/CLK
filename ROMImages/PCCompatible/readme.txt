Expected files:

GLABIOS_0.2.5_8T.ROM — the 8088 GLaBIOS ROM.
GLaTICK_0.8.5_AT.ROM — the GLaBIOS AT RTC option ROM.
Phoenix 80286 ROM BIOS Version 3.05.bin — Phoenix's 80286 AT-clone BIOS.
EUMDA9.F14 — a dump of the MDA font.
CGA.F08 — a dump of the CGA font.


GLaBIOS is an open-source GPLv3 alternative BIOS for XT clones, available from https://glabios.org/

GLaTICK is a real-time clock option ROM, also available from available from https://glabios.org/

The MDA and CGA fonts are in the form offered at https://github.com/viler-int10h/vga-text-mode-fonts i.e. it's 256 lots of 14 bytes, the first 14 being the content of character 0, the next 14 being the content of character 1, etc.