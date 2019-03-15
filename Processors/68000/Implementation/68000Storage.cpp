//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

#include <array>

using namespace CPU::MC68000;

ProcessorStorage::ProcessorStorage() {
	// Create the exception programs.
	const size_t reset_offset = assemble_program("n- n- n- n- n- nn nF nf nV nv np np");

	// Install all necessary access patterns.
	const BusStepCollection bus_steps = assemble_standard_bus_steps();

	// Install operations.
	install_instructions(bus_steps);

	// Realise the exception programs as direct pointers.
	reset_program_ = &all_bus_steps_[reset_offset];

	// Set initial state. Largely TODO.
	active_step_ = reset_program_;
	effective_address_ = 0;
	is_supervisor_ = 1;
}

size_t ProcessorStorage::assemble_program(const char *access_pattern, const std::vector<uint32_t *> &addresses, int data_mask) {
	const size_t start = all_bus_steps_.size();
	auto address_iterator = addresses.begin();
	RegisterPair32 *scratch_data_read = bus_data_;
	RegisterPair32 *scratch_data_write = bus_data_;

	// Parse the access pattern to build microcycles.
	while(*access_pattern) {
		BusStep step;

		switch(*access_pattern) {
			case '\t': case ' ': // White space acts as a no-op; it's for clarity only.
				++access_pattern;
			break;

			case 'n':	// This might be a plain NOP cycle, in which some internal calculation occurs,
						// or it might pair off with something afterwards.
				switch(access_pattern[1]) {
					default:	// This is probably a pure NOP; if what comes after this 'n' isn't actually
								// valid, it should be caught in the outer switch the next time around the loop.
						all_bus_steps_.push_back(step);
						++access_pattern;
					break;

					case '-':	// This is two NOPs in a row.
						all_bus_steps_.push_back(step);
						all_bus_steps_.push_back(step);
						access_pattern += 2;
					break;

					case 'F':	// Fetch SSP MSW.
					case 'f':	// Fetch SSP LSW.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &stack_pointers_[1].halves.high : &stack_pointers_[1].halves.low;
						all_bus_steps_.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = BusStep::Action::IncrementEffectiveAddress;
						all_bus_steps_.push_back(step);

						access_pattern += 2;
					break;

					case 'V':	// Fetch exception vector low.
					case 'v':	// Fetch exception vector high.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &program_counter_.halves.high : &program_counter_.halves.low;
						all_bus_steps_.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = BusStep::Action::IncrementEffectiveAddress;
						all_bus_steps_.push_back(step);

						access_pattern += 2;
					break;

					case 'p':	// Fetch from the program counter into the prefetch queue.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;
						step.microcycle.address = &program_counter_.full;
						step.microcycle.value = &prefetch_queue_[1];
						step.action = BusStep::Action::AdvancePrefetch;
						all_bus_steps_.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = BusStep::Action::IncrementProgramCounter;
						all_bus_steps_.push_back(step);

						access_pattern += 2;
					break;

					case 'r':	// Fetch LSW (or only) word (/byte)
					case 'R':	// Fetch MSW word
					case 'w':	// Store LSW (or only) word (/byte)
					case 'W': {	// Store MSW word
						const bool is_read = tolower(access_pattern[1]) == 'r';
						RegisterPair32 **scratch_data = is_read ? &scratch_data_read : &scratch_data_write;

						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | (is_read ? Microcycle::ReadWrite : 0);
						step.microcycle.address = *address_iterator;
						step.microcycle.value = isupper(access_pattern[1]) ? &(*scratch_data)->halves.high : &(*scratch_data)->halves.low;
						all_bus_steps_.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= data_mask;
						all_bus_steps_.push_back(step);

						++address_iterator;
						if(!isupper(access_pattern[1])) ++(*scratch_data);
						access_pattern += 2;
					} break;
				}
			break;

			default:
				std::cerr << "MC68000 program builder; Unknown access type " << *access_pattern << std::endl;
				assert(false);
		}
	}

	// Add a final 'ScheduleNextProgram' sentinel.
	BusStep end_program;
	end_program.action = BusStep::Action::ScheduleNextProgram;
	all_bus_steps_.push_back(end_program);

	return start;
}

ProcessorStorage::BusStepCollection ProcessorStorage::assemble_standard_bus_steps() {
	ProcessorStorage::BusStepCollection collection;

	collection.four_step_Dn = assemble_program("np");
	collection.six_step_Dn = assemble_program("np n");

	for(int s = 0; s < 8; ++s) {
		for(int d = 0; d < 8; ++d) {
			collection.double_predec_byte[s][d] = assemble_program("n nr nr np nw", { &address_[s].full, &address_[d].full, &address_[d].full }, Microcycle::LowerData);
			collection.double_predec_word[s][d] = assemble_program("n nr nr np nw", { &address_[s].full, &address_[d].full, &address_[d].full });
//			collection.double_predec_long[s][d] = assemble_program("n nr nR nr nR nw np nW", { &address_[s].full, &address_[d].full, &address_[d].full });
		}
	}

	return collection;
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
void ProcessorStorage::install_instructions(const BusStepCollection &bus_step_collection) {
	enum class Decoder {
		Decimal,
		RegOpModeReg,
		SizeModeRegisterImmediate,
		DataSizeModeQuick
	};

	struct PatternMapping {
		uint16_t mask, value;
		Operation operation;
		Decoder decoder;
	};

	/*
		Inspired partly by 'wrm' (https://github.com/wrm-za I assume); the following
		table draws from the M68000 Programmer's Reference Manual, currently available at
		https://www.nxp.com/files-static/archives/doc/ref_manual/M68000PRM.pdf

		After each line is the internal page number on which documentation of that
		instruction mapping can be found, followed by the page number within the PDF
		linked above.

		NB: a vector is used to allow easy iteration.
	*/
	const std::vector<PatternMapping> mappings = {
		{0xf1f0, 0x8100, Operation::SBCD, Decoder::Decimal},		// 4-171 (p275)
		{0xf1f0, 0xc100, Operation::ABCD, Decoder::Decimal},		// 4-3 (p107)

		{0xf000, 0x8000, Operation::OR, Decoder::RegOpModeReg},		// 4-150 (p226)
		{0xf000, 0x9000, Operation::SUB, Decoder::RegOpModeReg},	// 4-174 (p278)
		{0xf000, 0xb000, Operation::EOR, Decoder::RegOpModeReg},	// 4-100 (p204)
		{0xf000, 0xc000, Operation::AND, Decoder::RegOpModeReg},	// 4-15 (p119)
		{0xf000, 0xd000, Operation::ADD, Decoder::RegOpModeReg},	// 4-4 (p108)

		{0xff00, 0x0600, Operation::ADD, Decoder::SizeModeRegisterImmediate},	// 4-9 (p113)

		{0xff00, 0x0600, Operation::ADD, Decoder::DataSizeModeQuick},	// 4-11 (p115)
	};

	std::vector<size_t> micro_op_pointers(65536, std::numeric_limits<size_t>::max());

	// Perform a linear search of the mappings above for this instruction.
	for(size_t instruction = 0; instruction < 65536; ++instruction)	{
		for(const auto &mapping: mappings) {
			if((instruction & mapping.mask) == mapping.value) {
				// Install the operation and make a note of where micro-ops begin.
				instructions[instruction].operation = mapping.operation;
				micro_op_pointers[instruction] = all_micro_ops_.size();

				switch(mapping.decoder) {
					case Decoder::Decimal: {
						const int destination = (instruction >> 9) & 7;
						const int source = instruction & 7;

						if(instruction & 8) {
							instructions[instruction].source = &bus_data_[0];
							instructions[instruction].destination = &bus_data_[1];

							all_micro_ops_.emplace_back(
								MicroOp::Action::PredecrementSourceAndDestination1,
								&all_bus_steps_[bus_step_collection.double_predec_byte[source][destination]]);
							all_micro_ops_.emplace_back(MicroOp::Action::PerformOperation);
							all_micro_ops_.emplace_back();
						} else {
							instructions[instruction].source = &data_[source];
							instructions[instruction].destination = &data_[destination];

							all_micro_ops_.emplace_back(
								MicroOp::Action::PerformOperation,
								&all_bus_steps_[bus_step_collection.six_step_Dn]);
							all_micro_ops_.emplace_back();
						}
					} break;

					case Decoder::RegOpModeReg: {
					} break;

					default:
						std::cerr << "Unhandled decoder " << int(mapping.decoder) << std::endl;
					break;
				}

				// Don't search further through the list of possibilities.
				break;
			}
		}
	}

	// Finalise micro-op pointers.
	for(size_t instruction = 0; instruction < 65536; ++instruction) {
		if(micro_op_pointers[instruction] != std::numeric_limits<size_t>::max()) {
			instructions[instruction].micro_operations = &all_micro_ops_[micro_op_pointers[instruction]];
		}
	}
}
