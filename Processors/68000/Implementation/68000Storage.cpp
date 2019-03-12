//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

using namespace CPU::MC68000;

ProcessorStorage::ProcessorStorage() {
	// Create the reset program.
	reset_program_ = assemble_program("n- n- n- n- n- nn nF nf nV nv np np");

	// TODO: install access patterns.

	// Install operations.
	for(int c = 0; c < 65536; ++c) {
		install_instruction(c);
	}

	// Set initial state. Largely TODO.
	active_program_ = reset_program_.data();
	effective_address_ = 0;
	is_supervisor_ = 1;
}

// TODO: allow actions to be specified, of course.
std::vector<ProcessorStorage::Step> ProcessorStorage::assemble_program(const char *access_pattern) {
	std::vector<Step> steps;

	// Parse the access pattern to build microcycles.
	while(*access_pattern) {
		Step step;

		switch(*access_pattern) {
			case '\t': case ' ': // White space acts as a no-op; it's for clarity only.
				++access_pattern;
			break;

			case 'n':	// This might be a plain NOP cycle, in which some internal calculation occurs,
						// or it might pair off with something afterwards.
				switch(access_pattern[1]) {
					default:	// This is probably a pure NOP; if what comes after this 'n' isn't actually
								// valid, it should be caught in the outer switch the next time around the loop.
						steps.push_back(step);
						++access_pattern;
					break;

					case '-':	// This is two NOPs in a row.
						steps.push_back(step);
						steps.push_back(step);
						access_pattern += 2;
					break;

					case 'F':	// Fetch SSP MSW.
					case 'f':	// Fetch SSP LSW.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &stack_pointers_[1].halves.high : &stack_pointers_[1].halves.low;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementEffectiveAddress;
						steps.push_back(step);

						access_pattern += 2;
					break;

					case 'V':	// Fetch exception vector low.
					case 'v':	// Fetch exception vector high.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &program_counter_.halves.high : &program_counter_.halves.low;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementEffectiveAddress;
						steps.push_back(step);

						access_pattern += 2;
					break;

					case 'p':	// Fetch from the program counter into the prefetch queue.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;
						step.microcycle.address = &program_counter_.full;
						step.microcycle.value = &prefetch_queue_[1];
						step.action = Step::Action::AdvancePrefetch;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementProgramCounter;
						steps.push_back(step);

						access_pattern += 2;
					break;
				}
			break;

			default:
				std::cerr << "MC68000 program builder; Unknown access type " << *access_pattern << std::endl;
				assert(false);
		}
	}

	// Add a final 'ScheduleNextProgram' sentinel.
	Step end_program;
	end_program.action = Step::Action::ScheduleNextProgram;
	steps.push_back(end_program);

	return steps;
}

/*
	install_instruction acts, in effect, in the manner of a disassembler. So this class is
	formulated to run through all potential 65536 instuction encodings and attempt to
	disassemble each, rather than going in the opposite direction.

	This has two benefits:

		(i) which addressing modes go with which instructions falls out automatically;
		(ii) it is a lot easier during the manual verification stage of development to work
			from known instructions to their disassembly rather than vice versa; especially
		(iii) given that there are plentiful disassemblers against which to test work in progress.
*/
void ProcessorStorage::install_instruction(int instruction) {
	enum class Operation {
		ABCD,	ADD,	ADDA,	ADDI,	ADDQ,	ADDX,	AND,	ANDI,
		ASL,	ASLrmw,	ASR,	ASRrmw,	Bcc,

		TODO
	};

	struct PatternMapping {
		uint16_t mask, value;
		Operation operation;
	};

	/*
		Credit here is due to 'wrm' (https://github.com/wrm-za I assume) for his public
		domain 68000 disassembler, from which the table below was largely sourced.
		Manual legwork has been extended to check this table against the M68000
		Programmer's Reference Manual, currently available at
		https://www.nxp.com/files-static/archives/doc/ref_manual/M68000PRM.pdf
	*/
	const std::vector<PatternMapping> mappings = {
		{0xf1f0, 0xc100, Operation::ABCD},	{0xf000, 0xd000, Operation::ADD},
		{0xf0c0, 0xd0c0, Operation::ADDA},	{0xff00, 0x0600, Operation::ADDI},
		{0xf100, 0x5000, Operation::ADDQ},	{0xf130, 0xd100, Operation::ADDX},
		{0xf000, 0xc000, Operation::AND},	{0xff00, 0x0200, Operation::ANDI},
		{0xf118, 0xe100, Operation::ASL},	{0xffc0, 0xe1c0, Operation::ASLrmw},
		{0xf118, 0xe000, Operation::ASR},	{0xffc0, 0xe0c0, Operation::ASRrmw},
		{0xf000, 0x6000, Operation::Bcc},	{0xf1c0, 0x0140, Operation::TODO},
		{0xffc0, 0x0840, Operation::TODO},	{0xf1c0, 0x0180, Operation::TODO},
		{0xffc0, 0x0880, Operation::TODO},	{0xf1c0, 0x01c0, Operation::TODO},
		{0xffc0, 0x08c0, Operation::TODO},	{0xf1c0, 0x0100, Operation::TODO},
		{0xffc0, 0x0800, Operation::TODO},	{0xf1c0, 0x4180, Operation::TODO},
		{0xff00, 0x4200, Operation::TODO},	{0xf100, 0xb000, Operation::TODO},
		{0xf0c0, 0xb0c0, Operation::TODO},	{0xff00, 0x0c00, Operation::TODO},
		{0xf138, 0xb108, Operation::TODO},	{0xf0f8, 0x50c8, Operation::TODO},
		{0xf1c0, 0x81c0, Operation::TODO},	{0xf1c0, 0x80c0, Operation::TODO},
		{0xf100, 0xb100, Operation::TODO},	{0xff00, 0x0a00, Operation::TODO},
		{0xf100, 0xc100, Operation::TODO},	{0xffb8, 0x4880, Operation::TODO},
		{0xffc0, 0x4ec0, Operation::TODO},	{0xffc0, 0x4e80, Operation::TODO},
		{0xf1c0, 0x41c0, Operation::TODO},	{0xfff8, 0x4e50, Operation::TODO},
		{0xf118, 0xe108, Operation::TODO},	{0xffc0, 0xe3c0, Operation::TODO},
		{0xf118, 0xe008, Operation::TODO},	{0xffc0, 0xe2c0, Operation::TODO},
		{0xc000, 0x0000, Operation::TODO},	{0xffc0, 0x44c0, Operation::TODO},
		{0xffc0, 0x46c0, Operation::TODO},	{0xffc0, 0x40c0, Operation::TODO},
		{0xfff0, 0x4e60, Operation::TODO},	{0xc1c0, 0x0040, Operation::TODO},
		{0xfb80, 0x4880, Operation::TODO},	{0xf138, 0x0108, Operation::TODO},
		{0xf100, 0x7000, Operation::TODO},	{0xf1c0, 0xc1c0, Operation::TODO},
		{0xf1c0, 0xc0c0, Operation::TODO},	{0xffc0, 0x4800, Operation::TODO},
		{0xff00, 0x4400, Operation::TODO},	{0xff00, 0x4000, Operation::TODO},
		{0xffff, 0x4e71, Operation::TODO},	{0xff00, 0x4600, Operation::TODO},
		{0xf000, 0x8000, Operation::TODO},	{0xff00, 0x0000, Operation::TODO},
		{0xffc0, 0x4840, Operation::TODO},	{0xffff, 0x4e70, Operation::TODO},
		{0xf118, 0xe118, Operation::TODO},	{0xffc0, 0xe7c0, Operation::TODO},
		{0xf118, 0xe018, Operation::TODO},	{0xffc0, 0xe6c0, Operation::TODO},
		{0xf118, 0xe110, Operation::TODO},	{0xffc0, 0xe5c0, Operation::TODO},
		{0xf118, 0xe010, Operation::TODO},	{0xffc0, 0xe4c0, Operation::TODO},
		{0xffff, 0x4e73, Operation::TODO},	{0xffff, 0x4e77, Operation::TODO},
		{0xffff, 0x4e75, Operation::TODO},	{0xf1f0, 0x8100, Operation::TODO},
		{0xf0c0, 0x50c0, Operation::TODO},	{0xffff, 0x4e72, Operation::TODO},
		{0xf000, 0x9000, Operation::TODO},	{0xf0c0, 0x90c0, Operation::TODO},
		{0xff00, 0x0400, Operation::TODO},	{0xf100, 0x5100, Operation::TODO},
		{0xf130, 0x9100, Operation::TODO},	{0xfff8, 0x4840, Operation::TODO},
		{0xffc0, 0x4ac0, Operation::TODO},	{0xfff0, 0x4e40, Operation::TODO},
		{0xffff, 0x4e76, Operation::TODO},	{0xff00, 0x4a00, Operation::TODO},
		{0xfff8, 0x4e58, Operation::TODO}
	};

	// Perform a linear search of the mappings above for this instruction.
	for(const auto &mapping: mappings) {
		if((instruction & mapping.mask) == mapping.value) {
			if(mapping.operation == Operation::TODO) {
				std::cout << std::hex << std::setw(4) << std::setfill('0');
				std::cout << "Yet to implement, instruction matching: x & " << mapping.mask << " == " << mapping.value << std::endl;
			}
			break;
		}
	}
}
