
65C816 instruction set test


2017-12-13 J.E. Klasek j+816 AT klasek DOT at


ACME syntax, green border shows success. in case of failure red border
is shown and $0400 contains number of failed test and $0401 a bitmap
showing which tests actually failed.
If all tests fail on screen "f?" will be shown (corresponds to 6 failures)
and the bitmap %00111111 ($3F = '?')

There are 6 tests (bit 5 to bit 0):

	STX $FFFF	fails in 16 mode for X/Y if wrapping to location 0
	STY $FFFF	fails in 16 mode for X/Y if wrapping to location 0
	LDX $FFFF,Y	fails if wrapping to same bank
	LDY $FFFF,X	fails if wrapping to same bank
	TRB $FFFF	fails in 16 mode for A/M if wrapping to location 0
	TSB $FFFF	fails in 16 mode for A/M if wrapping to location 0

-------------------------------------------------------------------------------
