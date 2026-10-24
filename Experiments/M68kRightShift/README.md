# Independent 68000 right-shift regression

Original MIT-licensed test stimuli. Requires CMake 3.19+, a C++20 compiler and the CPU headers in this checkout; no firmware, disk images or external libraries are required.

From the repository root:

```sh
cmake -S Experiments/M68kRightShift -B build/m68k-right-shift
cmake --build build/m68k-right-shift --config Release
ctest --test-dir build/m68k-right-shift -C Release --output-on-failure
```

The suite checks 648 ASR and 648 LSR cases across byte, word and longword operands, six operand patterns and both initial X states. Register counts include 0, 1, width-1, width, width+1, 63, 64, 65, 128 and a large value with effective count 44. Immediate-count cases cover 1 through 8. An independent one-bit-at-a-time model supplies expected results and all X/N/Z/V/C flags, including preserved X at zero effective count. Initial N/Z/V/C are set to test recomputation.

The executable tests the shared CPU primitive, not instruction decoding. It also checks preserved supervisor/interrupt state and exactly one timing notification carrying the modulo-64 count; it does not independently measure physical instruction timings.

Specification: Motorola [M68000 Family Programmer's Reference Manual, ASL/ASR pp. 4-21 to 4-22 and LSL/LSR pp. 4-113 to 4-114](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf#page=125). ASR shifts the sign bit back into the operand and transfers the last shifted-out bit to C/X. Zero count clears C/V and preserves X; register counts are modulo 64.
