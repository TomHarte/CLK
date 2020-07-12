# 68000 Comparative Tests

Tests contained in this folder are original to Clock Signal. All are in JSON format so there's no need to write and maintain a specialised parser.

Tests assume a test machine consisting of a vanilla 68000 with 16mb of RAM. For each test the 68000 should be started from power on and allowed to execute a single instruction.

Each file contains an array of dictionaries. Each dictionary is a single test. It includes a name, initial memory contents and register state, and any changes to memory that occur during the test plus the final register state.

Both the initial and final memory arrays are in the form:

	[address, value, address, value ..., -1]

All tests are randomly generated, and the end results were achieved empirically using a believed-good 68000 emulation. Nothing here is intelligent or crafted, this merely an attempt to get a lot of coverage with limited effort.

## Methodology

Neither the file names nor the test names are necessarily accurate; the process taken for generating tests was:

* look up instruction encoding for instruction in the 68000 Programmer's Reference Manual, starting from page 8-5 (p561 of the PDF I'm using);
* those encodings will look something like: ORI -> 0000 0000 SS MMM RRR where SS = size, MMM = mode, RRR = register;
* therefore, generate the 256 possible 16-bit words that match that pattern and for each one that passes a does-this-instruction-exist test, produce a test case.

However, the 68000 isn't fully orthogonal in its instruction encodings — in the ORI example above, some modes and some sizes are illegal, and those opcodes are used for other instructions. But my test generator isn't smart enough to spot that — it just spots that what it's generating is a real instruction, and therefore produces a test.   

As a result, the tests labelled ORI will include all valid ORIs, plus some other instructions. I didn't consider this worth fixing, as it acts to increase my testing base.

I follow every generated opcode with three words of mostly-random data; this data — and almost all other random numbers — never have the lowest bit set, and supply 00 where the size field would be if they got used for an An + Xn + d addressing mode.

Similarly, all initial register contents are random except that the lowest bit is never set, just in case any of them is used to calculate an address for a word or longword.

So the output is very scattergun approach, with a lot of redundancy. 

## Questionable Results

I am presently unconvinced by the results for  the N flag on many of the [A/S/N]BCD results, as these often seem to conflict with Bart Trzynadlowski's 68knotes.txt.

This emulator seems not yet to generate values for the undocumented flags of DIVU and DIVS that match those listed here, but that's through lack of documentation. The objective here is to test my implementation of behaviour I am able to find descriptions of against other people's implementations of that same behaviour, to flush out errors in my comprehension and implementation.
