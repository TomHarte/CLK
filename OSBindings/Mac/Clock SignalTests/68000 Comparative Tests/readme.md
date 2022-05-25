# 68000 Comparative Tests

Tests contained in this folder are original to Clock Signal. All are JSON.

Tests assume a test machine consisting of a vanilla 68000 with 16mb of RAM. For each test either:
1. start from a reset, e.g. if you have a prefetch queue you need to fill; or
2. just apply the entire initial state, which indicates the proper PC and A7 for itself.

Then execute to the end of a single instruction (including any generated exception).

Each file contains an array of dictionaries. Each dictionary is a single test and includes:
* a name;
* initial memory contents;
* initial register state;
* any changes to memory that occur during the test; and
* the final register state.

Both the initial and final memory arrays specify bytes and are in the form:

	[address, value, address, value ..., -1]

All tests are randomly generated, and the end results were achieved empirically using a believed-good 68000 emulation. Some have subsequently been patched as and when 68000 emulation faults are found. JSON formatting is not guaranteed to be consistent.

Nothing here is intelligent or crafted, it's merely an attempt to get a lot of coverage with limited effort.

## Methodology

Neither file names nor test names are necessarily accurate; process was:

* look up an instruction encoding in the 68000 Programmer's Reference Manual, starting from page 8-5 (p561 of the PDF I'm using);
* that'll look something like: ORI -> 0000 0000 SS MMM RRR where SS = size, MMM = mode, RRR = register;
* therefore, generate the 256 possible 16-bit words that match that pattern; and
* for each one that passes a does-this-instruction-exist test, produce a test case.

Since the 68000 isn't fully orthogonal in its instruction encodings — in the ORI example above some modes and some sizes are illegal, those opcodes being used for other instructions — the tests labelled ORI will include both: (i) all valid ORIs; and (ii) some other instructions. I didn't consider this worth fixing.

Every generated opcode is followed by three words of mostly-random data; this data — and almost all other random numbers — never has the lowest bit set, and contains 00 where the size field would be if used for an An + Xn + d addressing mode.

All initial register contents are random except that the lowest bit is never set, to avoid accidental address errors.

So the output is very scattergun approach, with a lot of redundancy. 

## Known Issues

Errors in generation mean that:
1. MOVE is mostly untested; MOVEq is well-tested and other MOVEs appear within the test set as per the approximate generation algorithm above but due to an error in the generation of move.json, all of its opcodes are $2000 less than they should be, causing them to hit various instructions other than MOVE;
2. there is sparse coverage of the rotates and shifts: LS[L/R], AS[L/R], RO[L/R] and ROX[L/R]; and
3. there are similarly few tests of MULU.

Issues with comparing results between multiple emulators in the case of unusual instructions mean that no tests have been generated for:
1. MOVE [to or from] SR;
2. TRAP;
3. TRAPV;
4. MOVE [to or from] USP;
5. STOP;
6. RTE;
7. Bcc where the offset is an odd number; or
8. BSR where the offset is an odd number.

For both Bcc and BSR, there is good coverage of even-quantity offsets.

## Questionable Results

Values for the undocumented flags of DIVU and DIVS have not yet been verified, due to a lack of documentation.
