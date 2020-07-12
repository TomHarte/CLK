Welcome to the Zilog Z80 CPU test suite.

This set of programs is intended to help the emulator authors to reach the
desired level of the CPU emulation authenticity. Each of the included programs
performs an exhaustive computation using each of the tested Z80 instructions,
compares the results with values obtained from a real 48K Spectrum with Zilog Z80 CPU,
and reports any deviations detected.

The following variants are available:

- z80full - tests all flags and registers.
- z80doc - tests all registers, but only officially documented flags.
- z80flags - tests all flags, ignores registers.
- z80docflags - tests documented flags only, ignores registers.
- z80ccf - tests all flags after executing CCF after each instruction tested.
- z80memptr - tests all flags after executing BIT N,(HL) after each instruction tested.

The first four are the standard tests for CPU emulation. The CCF variant is
used to thoroughly test the authentic SCF/CCF behavior after each Z80
instruction. Finally the MEMPTR variant can be used to discover problems in
the MEMPTR emulation - however note that the current set of test was not
specifically designed to stress test MEMPTR, so many of the possible
problems are very likely left undetected. I may eventually add specific
MEMPTR tests in later releases.

Enjoy!

Patrik Rak
