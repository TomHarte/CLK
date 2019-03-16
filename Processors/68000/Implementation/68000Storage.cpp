//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

#include <array>

namespace CPU {
namespace MC68000 {

struct ProcessorStorageConstructor {
	ProcessorStorageConstructor(ProcessorStorage &storage) : storage_(storage) {}

	using BusStep = ProcessorStorage::BusStep;

	/*!
		Installs BusSteps that implement the described program into the relevant
		instance storage, returning the offset within @c all_bus_steps_ at which
		the generated steps begin.

		@param access_pattern A string describing the bus activity that occurs
			during this program. This should follow the same general pattern as
			those in yacht.txt; full description below.

		@discussion
		The access pattern is defined, as in yacht.txt, to be a string consisting
		of the following discrete bus actions. Spaces are ignored.

		* n: no operation; data bus is not used;
		* -: idle state; data bus is not used but is also not available;
		* p: program fetch; reads from the PC and adds two to it;
		* W: write MSW of something onto the bus;
		* w: write LSW of something onto the bus;
		* R: read MSW of something from the bus;
		* r: read LSW of soemthing from the bus;
		* S: push the MSW of something onto the stack;
		* s: push the LSW of something onto the stack;
		* U: pop the MSW of something from the stack;
		* u: pop the LSW of something from the stack;
		* V: fetch a vector's MSW;
		* v: fetch a vector's LSW;
		* i: acquire interrupt vector in an IACK cycle;
		* F: fetch the SSPs MSW;
		* f: fetch the SSP's LSW.

		Quite a lot of that is duplicative, implying both something about internal
		state and something about what's observable on the bus, but it's helpful to
		stick to that document's coding exactly for easier debugging.

		p fetches will fill the prefetch queue, attaching an action to both the
		step that precedes them and to themselves. The SSP fetches will go straight
		to the SSP.

		Other actions will by default act via effective_address_ and bus_data_.
		The user should fill in the steps necessary to get data into or extract
		data from those.
	*/
	size_t assemble_program(const char *access_pattern, const std::vector<uint32_t *> &addresses = {}, bool read_full_words = true) {
		const size_t start = storage_.all_bus_steps_.size();
		auto address_iterator = addresses.begin();
		RegisterPair32 *scratch_data_read = storage_.bus_data_;
		RegisterPair32 *scratch_data_write = storage_.bus_data_;
		using Action = BusStep::Action;

		// Parse the access pattern to build microcycles.
		while(*access_pattern) {
			ProcessorBase::BusStep step;

			switch(*access_pattern) {
				case '\t': case ' ': // White space acts as a no-op; it's for clarity only.
					++access_pattern;
				break;

				case 'n':	// This might be a plain NOP cycle, in which some internal calculation occurs,
							// or it might pair off with something afterwards.
					switch(access_pattern[1]) {
						default:	// This is probably a pure NOP; if what comes after this 'n' isn't actually
									// valid, it should be caught in the outer switch the next time around the loop.
							storage_.all_bus_steps_.push_back(step);
							++access_pattern;
						break;

						case '-':	// This is two NOPs in a row.
							storage_.all_bus_steps_.push_back(step);
							storage_.all_bus_steps_.push_back(step);
							access_pattern += 2;
						break;

						case 'F':	// Fetch SSP MSW.
						case 'f':	// Fetch SSP LSW.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
							step.microcycle.address = &storage_.effective_address_;
							step.microcycle.value = isupper(access_pattern[1]) ? &storage_.stack_pointers_[1].halves.high : &storage_.stack_pointers_[1].halves.low;
							storage_.all_bus_steps_.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation = Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementEffectiveAddress;
							storage_.all_bus_steps_.push_back(step);

							access_pattern += 2;
						break;

						case 'V':	// Fetch exception vector low.
						case 'v':	// Fetch exception vector high.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
							step.microcycle.address = &storage_.effective_address_;
							step.microcycle.value = isupper(access_pattern[1]) ? &storage_.program_counter_.halves.high : &storage_.program_counter_.halves.low;
							storage_.all_bus_steps_.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementEffectiveAddress;
							storage_.all_bus_steps_.push_back(step);

							access_pattern += 2;
						break;

						case 'p':	// Fetch from the program counter into the prefetch queue.
							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;
							step.microcycle.address = &storage_.program_counter_.full;
							step.microcycle.value = &storage_.prefetch_queue_[1];
							step.action = Action::AdvancePrefetch;
							storage_.all_bus_steps_.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
							step.action = Action::IncrementProgramCounter;
							storage_.all_bus_steps_.push_back(step);

							access_pattern += 2;
						break;

						case 'r':	// Fetch LSW (or only) word (/byte)
						case 'R':	// Fetch MSW word
						case 'w':	// Store LSW (or only) word (/byte)
						case 'W': {	// Store MSW word
							const bool is_read = tolower(access_pattern[1]) == 'r';
							RegisterPair32 **scratch_data = is_read ? &scratch_data_read : &scratch_data_write;

							step.microcycle.length = HalfCycles(5);
							step.microcycle.operation = Microcycle::NewAddress | (is_read ? Microcycle::Read : 0);
							step.microcycle.address = *address_iterator;
							step.microcycle.value = isupper(access_pattern[1]) ? &(*scratch_data)->halves.high : &(*scratch_data)->halves.low;
							storage_.all_bus_steps_.push_back(step);

							step.microcycle.length = HalfCycles(3);
							step.microcycle.operation |= (read_full_words ? Microcycle::SelectWord : Microcycle::SelectByte) | (is_read ? Microcycle::Read : 0);
							storage_.all_bus_steps_.push_back(step);

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
		end_program.action = Action::ScheduleNextProgram;
		storage_.all_bus_steps_.push_back(end_program);

		return start;
	}

	struct BusStepCollection {
		size_t six_step_Dn;
		size_t four_step_Dn;

		// The next two are indexed as [source][destination].
		size_t double_predec_byte[8][8];
		size_t double_predec_word[8][8];
		size_t double_predec_long[8][8];
	};
	BusStepCollection assemble_standard_bus_steps() {
		BusStepCollection collection;

		collection.four_step_Dn = assemble_program("np");
		collection.six_step_Dn = assemble_program("np n");

		for(int s = 0; s < 8; ++s) {
			for(int d = 0; d < 8; ++d) {
				collection.double_predec_byte[s][d] = assemble_program("n nr nr np nw", { &storage_.address_[s].full, &storage_.address_[d].full, &storage_.address_[d].full }, false);
				collection.double_predec_word[s][d] = assemble_program("n nr nr np nw", { &storage_.address_[s].full, &storage_.address_[d].full, &storage_.address_[d].full });
	//			collection.double_predec_long[s][d] = assemble_program("n nr nR nr nR nw np nW", { &address_[s].full, &address_[d].full, &address_[d].full });
			}
		}

		return collection;

	}

	/*!
		Disassembles the instruction @c instruction and inserts it into the
		appropriate lookup tables.

		install_instruction acts, in effect, in the manner of a disassembler. So this class is
		formulated to run through all potential 65536 instuction encodings and attempt to
		disassemble each, rather than going in the opposite direction.

		This has two benefits:

			(i) which addressing modes go with which instructions falls out automatically;
			(ii) it is a lot easier during the manual verification stage of development to work
				from known instructions to their disassembly rather than vice versa; especially
			(iii) given that there are plentiful disassemblers against which to test work in progress.
	*/
	void install_instructions(const BusStepCollection &bus_step_collection) {
		enum class Decoder {
			Decimal,
			RegOpModeReg,
			SizeModeRegisterImmediate,
			DataSizeModeQuick,
			RegisterModeModeRegister
		};

		using Operation = ProcessorStorage::Operation;
		using Action = ProcessorStorage::MicroOp::Action;
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

			{0x1000, 0xf000, Operation::MOVEb, Decoder::RegisterModeModeRegister},	// 4-116 (p220)
			{0x3000, 0xf000, Operation::MOVEw, Decoder::RegisterModeModeRegister},	// 4-116 (p220)
			{0x2000, 0xf000, Operation::MOVEl, Decoder::RegisterModeModeRegister},	// 4-116 (p220)
		};

		std::vector<size_t> micro_op_pointers(65536, std::numeric_limits<size_t>::max());

		// Perform a linear search of the mappings above for this instruction.
		for(size_t instruction = 0; instruction < 65536; ++instruction)	{
			for(const auto &mapping: mappings) {
				if((instruction & mapping.mask) == mapping.value) {
					// Install the operation and make a note of where micro-ops begin.
					storage_.instructions[instruction].operation = mapping.operation;
					micro_op_pointers[instruction] = storage_.all_micro_ops_.size();

					switch(mapping.decoder) {
						case Decoder::Decimal: {
							const int destination = (instruction >> 9) & 7;
							const int source = instruction & 7;

							if(instruction & 8) {
								storage_.instructions[instruction].source = &storage_.bus_data_[0];
								storage_.instructions[instruction].destination = &storage_.bus_data_[1];

								storage_.all_micro_ops_.emplace_back(
									Action::PredecrementSourceAndDestination1,
									&storage_.all_bus_steps_[bus_step_collection.double_predec_byte[source][destination]]);
								storage_.all_micro_ops_.emplace_back(Action::PerformOperation);
								storage_.all_micro_ops_.emplace_back();
							} else {
								storage_.instructions[instruction].source = &storage_.data_[source];
								storage_.instructions[instruction].destination = &storage_.data_[destination];

								storage_.all_micro_ops_.emplace_back(
									Action::PerformOperation,
									&storage_.all_bus_steps_[bus_step_collection.six_step_Dn]);
								storage_.all_micro_ops_.emplace_back();
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
				storage_.instructions[instruction].micro_operations = &storage_.all_micro_ops_[micro_op_pointers[instruction]];
			}
		}

	}

	private:
		ProcessorStorage &storage_;
};

}
}

CPU::MC68000::ProcessorStorage::ProcessorStorage() {
	ProcessorStorageConstructor constructor(*this);

	// Create the exception programs.
	const size_t reset_offset = constructor.assemble_program("n- n- n- n- n- nn nF nf nV nv np np");

	// Install all necessary access patterns.
	const auto bus_steps = constructor.assemble_standard_bus_steps();

	// Install operations.
	constructor.install_instructions(bus_steps);

	// Realise the exception programs as direct pointers.
	reset_program_ = &all_bus_steps_[reset_offset];

	// Set initial state. Largely TODO.
	active_step_ = reset_program_;
	effective_address_ = 0;
	is_supervisor_ = 1;
}
