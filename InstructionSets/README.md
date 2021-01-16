#  Instruction Sets

## Decoders

A decoder extracts fully-decoded instructions from a data stream for its associated architecture. 

The meaning of 'fully-decoded' is flexible but it means that a caller can easily discern at least:  
* the operation in use;
* its addressing mode; and
* relevant registers.

It may be assumed that callers will have access to the original data stream for immediate values, if it is sensible to do so.

In deciding what to expose, what to store ahead of time and what to obtain just-in-time a decoder should have an eye on two principal consumers:
1. disassemblers; and
2. instruction executors.

It may also be reasonable to make allowances for bus-centric CPU emulators, but those will be tightly coupled to specific decoders so no general rules need apply. 

Disassemblers are likely to decode an instruction, output it, and then immediately forget about it.

Instruction executors may opt to cache decoded instructions to reduce recurrent costs, but will always be dealing with an actual instruction stream. The chance of caching means that decoded instructions should seek to be small. If helpful then a decoder might prefer to return a `std::pair` or similar of ephemeral information and stuff that it is meaningful to store.

## Likely Interfaces

These examples assume that the processor itself doesn't hold any state that affects instruction parsing. Whether processors with such state offer more than one decoder or take state as an argument will be a question of measure and effect.  

### Fixed-size instruction words

If the instructions are a fixed size, the decoder can provide what is functionally a simple lookup, whether implemented as such or not:

    Instruction decode(word_type instruction) { ... }

For now I have preferred not to make this a simple constructor on `Instruction` because I'm reserving the option of switching to an ephemeral/permanent split in what's returned. More consideration needs to be applied here.

### Variable-size instruction words

If instructions are a variable size, the decoder should maintain internal state such that it can be provided with fragments of instructions until a full decoding has occurred — this avoids an assumption that all source bytes will always be laid out linearly in memory.

A sample interface:

    std::pair<int, Instruction> decode(word_type *stream, size_t length) { ... }

In this sample the returned pair provides an `int` size that is one of:
* a positive number, indicating a completed decoding that consumed that many `word_type`s; or
* a negative number, indicating the [negatived] minimum number of `word_type`s that the caller should try to get hold of before calling `decode` again.

A caller is permitted to react in any way it prefers to negative numbers; they're a hint potentially to reduce calling overhead only. A size of `0` would be taken to have the same meaning as a size of `-1`.  

## Tying Decoders into Instruction Executors

It is assumed that disassemblers and bus-centric CPU emulators have limited generic functionality; for executors it is assumed that a processor-specific instruction fetcher and a dispatcher will be provided to couple with the decoder. 

Therefore decoders should adopt whatever interface is most natural; the expected uses information above is to provide a motivation for the scope of responsibilities and hints as to likely performance objectives only. Beyond requiring that decoded instructions be a tangible struct or class, it is not intended to be prescriptive as to form or interface.  
