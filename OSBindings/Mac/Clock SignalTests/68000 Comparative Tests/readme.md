# 68000 Comparative Tests

Tests contained in this folder are original to Clock Signal. All are in JSON format so there's no need to write and maintain a specialised parser.

Tests assume a test machine consisting of a vanilla 68000 with 16mb of RAM. For each test the 68000 should be started from power on and allowed to execute a single instruction.

Each file contains an array of dictionaries. Each dictionary is a single test. It includes a name, initial memory contents and register state, and any changes to memory that occur during the test plus the final register state.

Both the initial and final memory arrays are in the form:

	[address, value, address, value ..., -1]

All tests are randomly generated, and the end results were achieved empirically using a believed-good 68000 emulation. Nothing here is intelligent or crafted, this merely an attempt to get a lot of coverage with limited effort.