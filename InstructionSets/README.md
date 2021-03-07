#  Instruction Sets

Code in here provides the means to disassemble, and to execute code for certain instruction sets.

It **does not seek to emulate specific processors** other than in terms of implementing their instruction sets. So:
* it doesn't involve itself in the actual bus signalling of real processors; and
* instruction-level timing (e.g. total cycle counts) may be unimplemented, and is likely to be incomplete. 

This part of CLK is intended primarily to provide disassembly services for static analysis, and processing for machines where timing is not part of the specification — i.e. anything that's an instruction set and a HAL.

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

### Likely Interfaces

These examples assume that the processor itself doesn't hold any state that affects instruction parsing. Whether processors with such state offer more than one decoder or take state as an argument will be a question of measure and effect.  

#### Fixed-size instruction words

If the instructions are a fixed size, the decoder can provide what is functionally a simple lookup, whether implemented as such or not:

    Instruction decode(word_type instruction) { ... }

For now I have preferred not to make this a simple constructor on `Instruction` because I'm reserving the option of switching to an ephemeral/permanent split in what's returned. More consideration needs to be applied here.

#### Variable-size instruction words

If instructions are a variable size, the decoder should maintain internal state such that it can be provided with fragments of instructions until a full decoding has occurred — this avoids an assumption that all source bytes will always be laid out linearly in memory.

A sample interface:

    std::pair<int, Instruction> decode(word_type *stream, size_t length) { ... }

In this sample the returned pair provides an `int` size that is one of:
* a positive number, indicating a completed decoding that consumed that many `word_type`s; or
* a negative number, indicating the [negatived] minimum number of `word_type`s that the caller should try to get hold of before calling `decode` again.

A caller is permitted to react in any way it prefers to negative numbers; they're a hint potentially to reduce calling overhead only. A size of `0` would be taken to have the same meaning as a size of `-1`.  

## Parsers

A parser sits one level above a decoder; it is handed:
* a start address;
* a closing bound; and
* a target.

It is responsible for parsing the instruction stream from the start address up to and not beyond the closing bound, and no further than any unconditional branches.

It should post to the target:
* any instructions fully decoded;
* any conditional branch destinations encountered;
* any immediately-knowable accessed addresses; and
* if a final instruction exists but runs beyond the closing bound, notification of that fact.

So a parser has the same two primary potential recipients as a decoder: diassemblers, and executors.

## Executors

An executor is responsible for only one thing:
* mapping from decoded instructions to objects that can perform those instructions.

An executor is assumed to bundle all the things that go into instruction set execution: processor state and memory, alongside a parser.

## Caching Executor

The caching executor is a generic class templated on a specific executor. It will use an executor to cache the results of parsing. 

Idiomatically, the objects that perform instructions will expect to receive an appropriate executor as an argument. If they require other information, such as a copy of the decoded instruction, it should be built into the classes. 
