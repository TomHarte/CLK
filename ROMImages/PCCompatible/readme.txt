Expected files:

For XT-class emulation:

	GLABIOS_0.2.5_8T.ROM — the 8088 GLaBIOS ROM.
	GLaTICK_0.8.5_AT.ROM (optionally) — the GLaBIOS AT RTC option ROM.

For specific video cards:

	EUMDA9.F14 — the MDA font.
	CGA.F08 — the CGA font.

In the future:

	Phoenix 80286 ROM BIOS Version 3.05.bin — Phoenix's 80286 AT-clone BIOS.
	ibm_vga.bin — the VGA BIOS.
	ibm_6277356_ega_card_u44_27128.bin — the EGA BIOS.


GLaBIOS is an open-source GPLv3 alternative BIOS for XT clones, available from https://glabios.org/

GLaTICK is a real-time clock option ROM, also available from available from https://glabios.org/

The MDA and CGA fonts are in the form offered at https://github.com/viler-int10h/vga-text-mode-fonts i.e. the MDA font is 256 lots of 14 bytes, the first 14 being the content of character 0, the next 14 being the content of character 1, etc; the CGA font is 256 lots of 8 bytes with a similar arrangement.