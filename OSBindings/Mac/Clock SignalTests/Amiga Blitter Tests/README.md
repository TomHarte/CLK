## Amiga Blitter Tests

These tests record register writes and subsequent memory accesses by the Amiga Blitter over a variety of test cases. It is believed that they test all functionality other than stippled lines.

They were generated using a slightly-inaccurate public domain model of the chip rather than from the real thing. In particular:
* these tests record the output as though the Blitter weren't pipelined — assuming all channels enabled, it always reads via A, then B, then C, then writes via D. The real Blitter performs two cycles of reads before its first write, and adds a final write with no additional reads; and
* the tests do not record which pointer is used for a write target and therefore do not observe that the Blitter will use pointer C as a write destination for the first pixel of a line.
