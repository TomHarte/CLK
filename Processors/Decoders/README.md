#  Decoders

A decoder extracts fully-decoded instructions from a data stream for its associated architecture. 

An instruction is 'fully-decoded' when an instance of a suitable struct or class has been created that can provide at least the instruction's length, and will usually also provide as relevant:
* the operation in use;
* its addressing mode; and
* relevant registers.

It will have access to the original data stream again before being asked to provide any immediate values associated with the instruction.

In deciding what to expose, what to store ahead of time and what to obtain just-in-time a decoder should have an eye on three main potential types of consumer:
1. disassemblers;
2. instruction executors; and
3. bus-centric CPU emulators.

The first of those is likely to decode an instruction, output it to somewhere, and then immediately forget about it.

The second may opt to cache the decoded forms of instructions to reduce recurrent costs, but will always be dealing with an actual instruction stream. The chance of caching means that decoded instructions should seek to be small; however it also implies that as much processing cost as is reasonable should be spent up-front.     

The third may use a decoder live (especially if the instruction stream is complicated, or instruction words are large) or may use one once ahead of time in order to build up internal tables related to abstract interpretation of operations. The first use suggests a further premium on speed, the second implies that 'fully-decoded' instructions shouldn't seek to collect all possible immediate values ahead of time whenever doing so is avoidable — as a reult of thumb, they will usually return an instruction as soon as its length is fully known.

## Likely Interfaces

These examples assume that the processor itself doesn't hold any state that affects instruction parsing. Whether processors with such state offer more than one decoder or take state as an argument will be a question of measure and effect.  

### Fixed-size instruction words

If the instructions are a fixed size, the decoder can provide what is functionally a simple lookup, whether implemented as such or not:

    Instruction decode(word_type instruction) { ... }

### Variable-size instruction words

If instructions are a variable size, the decoder should maintain internal state such that it can be provided with fragments of instructions until a full decoding has occurred.

A sample interface:

    Instruction decode(word_type *stream, size_t length) { ... }

The returned instruction has a size that is one of:
* a positive number, indicating a successful decoding that consumed that many `word_type`s; or
* a negative number, indicating the [negatived] minimum number of `word_type`s that the caller should try to get hold of before calling `decode` again.

A caller is permitted to react in any way it prefers to negative numbers; they're a hint potentially to reduce calling overhead only. 

## Tying Decoders into Instruction Executors

It is assumed that disassemblers and bus-centric CPU emulators have limited generic functionality; for executors it is assumed that a processor-specific instruction fetcher and a dispatcher will be provided to couple with the decoder. 

Therefore decoders should adopt whatever interface is most natural; the expected uses information above is to provide a motivation for the scope of responsibilities and hints as to likely performance objectives only. Beyond requiring that decoded instructions be a tangible struct or class, it is not intended to be prescriptive as to form or interface.  
