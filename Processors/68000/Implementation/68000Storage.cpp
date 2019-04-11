//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

#include <algorithm>
#include <sstream>

namespace CPU {
namespace MC68000 {

struct ProcessorStorageConstructor {
	ProcessorStorageConstructor(ProcessorStorage &storage) : storage_(storage) {}

	using BusStep = ProcessorStorage::BusStep;

	/*!
	*/
	int calc_action_for_mode(int mode) const {
		using Action = ProcessorBase::MicroOp::Action;
		switch(mode & 0xff) {
			default: return 0;
			case 0x12:	return int(Action::CalcD16PC);		// (d16, PC)
			case 0x13:	return int(Action::CalcD8PCXn);		// (d8, PC, Xn)
			case 0x05:	return int(Action::CalcD16An);		// (d16, An)
			case 0x06:	return int(Action::CalcD8AnXn);		// (d8, An, Xn)
		}
	}

	int combined_mode(int mode, int reg) {
		return (mode == 7) ? (0x10 | reg) : mode;
	}

#define pseq(x, m) ((((m)&0xff) == 0x06) || (((m)&0xff) == 0x13) ? "n " x : x)

	/*!
		Installs BusSteps that implement the described program into the relevant
		instance storage, returning the offset within @c all_bus_steps_ at which
		the generated steps begin.

		@param access_pattern A string describing the bus activity that occurs
			during this program. This should follow the same general pattern as
			those in yacht.txt; full description below.

		@param addresses A vector of the addresses to place on the bus coincident
			with those acess steps that require them.

		@param read_full_words @c true to indicate that read and write operations are
			selecting a full word; @c false to signal byte accesses only.

		@discussion
		The access pattern is defined to correlate closely to that in yacht.txt; it is
		a space-separated sequence of the following actions:

		* n: no operation for four cycles; data bus is not used;
		* nn: no operation for eight cycles; data bus is not used;
		* r: a 'replaceable'-length no operation; data bus is not used and no guarantees are
			made about the length of the cycle other than that when it reaches the interpreter,
			it is safe to alter the length and leave it altered;
		* np: program fetch; reads from the PC and adds two to it, advancing the prefetch queue;
		* nW: write MSW of something onto the bus;
		* nw: write LSW of something onto the bus;
		* nR: read MSW of something from the bus into the source latch;
		* nr: read LSW of soemthing from the bus into the source latch;
		* nRd: read MSW of something from the bus into the destination latch;
		* nrd: read LSW of soemthing from the bus into the destination latch;
		* nS: push the MSW of something onto the stack;
		* ns: push the LSW of something onto the stack;
		* nU: pop the MSW of something from the stack;
		* nu: pop the LSW of something from the stack;
		* nV: fetch a vector's MSW;
		* nv: fetch a vector's LSW;
		* i: acquire interrupt vector in an IACK cycle;
		* nF: fetch the SSPs MSW;
		* nf: fetch the SSP's LSW;
		* _: hold the reset line active for the usual period.

		Quite a lot of that is duplicative, implying both something about internal
		state and something about what's observable on the bus, but it's helpful to
		stick to that document's coding exactly for easier debugging.

		np fetches will fill the prefetch queue, attaching an action to both the
		step that precedes them and to themselves. The SSP fetches will go straight
		to the SSP.

		Other actions will by default act via effective_address_ and bus_data_.
		The user should fill in the steps necessary to get data into or extract
		data from those.

		nr/nw-type operations may have a + or - suffix; if such a suffix is attached
		then the corresponding effective address will be incremented or decremented
		by two after the cycle has completed.
	*/
	size_t assemble_program(std::string access_pattern, const std::vector<uint32_t *> &addresses = {}, bool read_full_words = true) {
		auto address_iterator = addresses.begin();
		using Action = BusStep::Action;

		std::vector<BusStep> steps;
		std::stringstream stream(access_pattern);

		// Tokenise the access pattern by splitting on spaces.
		std::string token;
		while(stream >> token) {
			ProcessorBase::BusStep step;

			// Check for a plus-or-minus suffix.
			int post_adjustment = 0;
			if(token.back() == '-' || token.back() == '+') {
				if(token.back() == '-') {
					post_adjustment = -1;
				}

				if(token.back() == '+') {
					post_adjustment = 1;
				}

				token.pop_back();
			}

			// Do nothing (possibly twice).
			if(token == "n" || token == "nn") {
				if(token.size() == 2) {
					step.microcycle.length = HalfCycles(8);
				}
				steps.push_back(step);
				continue;
			}

			// Do nothing, but with a length that definitely won't map it to the other do-nothings.
			if(token == "r"){
				step.microcycle.length = HalfCycles(0);
				steps.push_back(step);
				continue;
			}

			// Fetch SSP.
			if(token == "nF" || token == "nf") {
				step.microcycle.length = HalfCycles(5);
				step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
				step.microcycle.address = &storage_.effective_address_[0].full;
				step.microcycle.value = isupper(token[1]) ? &storage_.stack_pointers_[1].halves.high : &storage_.stack_pointers_[1].halves.low;
				steps.push_back(step);

				step.microcycle.length = HalfCycles(3);
				step.microcycle.operation = Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
				step.action = Action::IncrementEffectiveAddress0;
				steps.push_back(step);

				continue;
			}

			// Fetch exception vector.
			if(token == "nV" || token == "nv") {
				step.microcycle.length = HalfCycles(5);
				step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;	// IsProgram is a guess.
				step.microcycle.address = &storage_.effective_address_[0].full;
				step.microcycle.value = isupper(token[1]) ? &storage_.program_counter_.halves.high : &storage_.program_counter_.halves.low;
				steps.push_back(step);

				step.microcycle.length = HalfCycles(3);
				step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
				step.action = Action::IncrementEffectiveAddress0;
				steps.push_back(step);

				continue;
			}

			// Fetch from the program counter into the prefetch queue.
			if(token == "np") {
				step.microcycle.length = HalfCycles(5);
				step.microcycle.operation = Microcycle::NewAddress | Microcycle::Read | Microcycle::IsProgram;
				step.microcycle.address = &storage_.program_counter_.full;
				step.microcycle.value = &storage_.prefetch_queue_.halves.low;
				step.action = Action::AdvancePrefetch;
				steps.push_back(step);

				step.microcycle.length = HalfCycles(3);
				step.microcycle.operation |= Microcycle::SelectWord | Microcycle::Read | Microcycle::IsProgram;
				step.action = Action::IncrementProgramCounter;
				steps.push_back(step);

				continue;
			}

			// The reset cycle.
			if(token == "_") {
				step.microcycle.length = HalfCycles(248);
				step.microcycle.operation = Microcycle::Reset;
				steps.push_back(step);

				continue;
			}

			// A standard read or write.
			if(token == "nR" || token == "nr" || token == "nW" || token == "nw" || token == "nRd" || token == "nrd") {
				const bool is_read = tolower(token[1]) == 'r';
				const bool use_source_storage = is_read && token.size() != 3;
				RegisterPair32 *scratch_data = use_source_storage ? &storage_.source_bus_data_[0] : &storage_.destination_bus_data_[0];

				assert(address_iterator != addresses.end());

				step.microcycle.length = HalfCycles(5);
				step.microcycle.operation = Microcycle::NewAddress | (is_read ? Microcycle::Read : 0);
				step.microcycle.address = *address_iterator;
				step.microcycle.value = isupper(token[1]) ? &scratch_data->halves.high : &scratch_data->halves.low;
				steps.push_back(step);

				step.microcycle.length = HalfCycles(3);
				step.microcycle.operation |= (read_full_words ? Microcycle::SelectWord : Microcycle::SelectByte) | (is_read ? Microcycle::Read : 0);
				if(post_adjustment) {
					if(tolower(token[1]) == 'r') {
						step.action = (post_adjustment > 0) ? Action::IncrementEffectiveAddress0 : Action::DecrementEffectiveAddress0;
					} else {
						step.action = (post_adjustment > 0) ? Action::IncrementEffectiveAddress1 : Action::DecrementEffectiveAddress1;

					}
				}
				steps.push_back(step);
				++address_iterator;

				continue;
			}

			std::cerr << "MC68000 program builder; Unknown access token " << token << std::endl;
			assert(false);
		}

		// Add a final 'ScheduleNextProgram' sentinel.
		BusStep end_program;
		end_program.action = Action::ScheduleNextProgram;
		steps.push_back(end_program);

		// If the new steps already exist, just return the existing index to them;
		// otherwise insert them.
		const auto position = std::search(storage_.all_bus_steps_.begin(), storage_.all_bus_steps_.end(), steps.begin(), steps.end());
		if(position != storage_.all_bus_steps_.end()) {
			return size_t(position - storage_.all_bus_steps_.begin());
		}

		const auto start = storage_.all_bus_steps_.size();
		std::copy(steps.begin(), steps.end(), std::back_inserter(storage_.all_bus_steps_));
		return start;
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
	void install_instructions() {
		enum class Decoder {
			ABCDSBCD,					// Maps source and desintation registers and a register/memory selection bit to an ABCD or SBCD.

			ADDSUB,						// Maps a register and a register and mode to an ADD or SUB.
			ADDASUBA,					// Maps a destination register and a source mode and register to an ADDA or SUBA.

			BRA,						// Maps to a BRA. All fields are decoded at runtime.
			Bcc,						// Maps to a Bcc. All fields are decoded at runtime.

			BTST,						// Maps a source register and a destination register and mode to a BTST.
			BTSTIMM,					// Maps a destination mode and register to a BTST #.

			CLRNEGNEGXNOT,				// Maps a destination mode and register to a CLR, NEG, NEGX or NOT.

			CMP,						// Maps a destination register and a source mode and register to a CMP.
			CMPI,						// Maps a destination mode and register to a CMPI.
			CMPA,						// Maps a destination register and a source mode and register to a CMPA.
			CMPM,						// Maps to a CMPM.

			DBcc,						// Maps a destination register to a DBcc.

			JMP,						// Maps a mode and register to a JMP.

			LEA,						// Maps a destination register and a source mode and register to an LEA.

			MOVE,						// Maps a source mode and register and a destination mode and register to a MOVE.
			MOVEtoSRCCR,				// Maps a source mode and register to a MOVE SR or MOVE CCR.
			MOVEq,						// Maps a destination register to a MOVEQ.

			RESET,						// Maps to a RESET.

			ASLRLSLRROLRROXLRr,			// Maps a destination register to a AS[L/R], LS[L/R], RO[L/R], ROX[L/R]; shift quantities are
										// decoded at runtime.
			ASLRLSLRROLRROXLRm,			// Maps a destination mode and register to a memory-based AS[L/R], LS[L/R], RO[L/R], ROX[L/R].
		};

		using Operation = ProcessorStorage::Operation;
		using Action = ProcessorStorage::MicroOp::Action;
		using MicroOp = ProcessorBase::MicroOp;
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
			{0xf1f0, 0xc100, Operation::ABCD, Decoder::ABCDSBCD},		// 4-3 (p107)
			{0xf1f0, 0x8100, Operation::SBCD, Decoder::ABCDSBCD},		// 4-171 (p275)

//			{0xf000, 0x8000, Operation::OR, Decoder::RegOpModeReg},		// 4-150 (p226)
//			{0xf000, 0xb000, Operation::EOR, Decoder::RegOpModeReg},	// 4-100 (p204)
//			{0xf000, 0xc000, Operation::AND, Decoder::RegOpModeReg},	// 4-15 (p119)

//			{0xff00, 0x0600, Operation::ADD, Decoder::SizeModeRegisterImmediate},	// 4-9 (p113)

//			{0xff00, 0x0600, Operation::ADD, Decoder::DataSizeModeQuick},	// 4-11 (p115)

			{0xf000, 0x1000, Operation::MOVEb, Decoder::MOVE},	// 4-116 (p220)
			{0xf000, 0x2000, Operation::MOVEl, Decoder::MOVE},	// 4-116 (p220)
			{0xf000, 0x3000, Operation::MOVEw, Decoder::MOVE},	// 4-116 (p220)

			{0xffc0, 0x46c0, Operation::MOVEtoSR, Decoder::MOVEtoSRCCR},	// 6-19 (p473)
			{0xffc0, 0x44c0, Operation::MOVEtoCCR, Decoder::MOVEtoSRCCR},	// 4-123 (p227)

			{0xf1c0, 0xb000, Operation::CMPb, Decoder::CMP},	// 4-75 (p179)
			{0xf1c0, 0xb040, Operation::CMPw, Decoder::CMP},	// 4-75 (p179)
			{0xf1c0, 0xb080, Operation::CMPl, Decoder::CMP},	// 4-75 (p179)

			{0xf1c0, 0xb0c0, Operation::CMPw, Decoder::CMPA},	// 4-77 (p181)
			{0xf1c0, 0xb1c0, Operation::CMPl, Decoder::CMPA},	// 4-77 (p181)

			{0xffc0, 0x0c00, Operation::CMPb, Decoder::CMPI},	// 4-79 (p183)
			{0xffc0, 0x0c40, Operation::CMPw, Decoder::CMPI},	// 4-79 (p183)
			{0xffc0, 0x0c80, Operation::CMPl, Decoder::CMPI},	// 4-79 (p183)

			{0xf1f8, 0xb108, Operation::CMPb, Decoder::CMPM},	// 4-81 (p185)
			{0xf1f8, 0xb148, Operation::CMPw, Decoder::CMPM},	// 4-81 (p185)
			{0xf1f8, 0xb188, Operation::CMPl, Decoder::CMPM},	// 4-81 (p185)

//			{0xff00, 0x6000, Operation::BRA, Decoder::BRA},		// 4-55 (p159)	TODO: confirm that this really, really is just a special case of Bcc.
			{0xf000, 0x6000, Operation::Bcc, Decoder::Bcc},		// 4-25 (p129)

			{0xf1c0, 0x41c0, Operation::MOVEAl, Decoder::LEA},	// 4-110 (p214)
			{0xf100, 0x7000, Operation::MOVEq, Decoder::MOVEq},	// 4-134 (p238)

			{0xffff, 0x4e70, Operation::None, Decoder::RESET},	// 6-83 (p537)

			{0xffc0, 0x4ec0, Operation::JMP, Decoder::JMP},		// 4-108 (p212)

			{0xf0c0, 0x9000, Operation::SUBb, Decoder::ADDSUB},	// 4-174 (p278)
			{0xf0c0, 0x9040, Operation::SUBw, Decoder::ADDSUB},	// 4-174 (p278)
			{0xf0c0, 0x9080, Operation::SUBl, Decoder::ADDSUB},	// 4-174 (p278)

			{0xf0c0, 0xd000, Operation::ADDb, Decoder::ADDSUB},	// 4-4 (p108)
			{0xf0c0, 0xd040, Operation::ADDw, Decoder::ADDSUB},	// 4-4 (p108)
			{0xf0c0, 0xd080, Operation::ADDl, Decoder::ADDSUB},	// 4-4 (p108)

			{0xf1c0, 0xd0c0, Operation::ADDAw, Decoder::ADDASUBA},	// 4-7 (p111)
			{0xf1c0, 0xd1c0, Operation::ADDAl, Decoder::ADDASUBA},	// 4-7 (p111)
			{0xf1c0, 0x90c0, Operation::SUBAw, Decoder::ADDASUBA},	// 4-177 (p281)
			{0xf1c0, 0x91c0, Operation::SUBAl, Decoder::ADDASUBA},	// 4-177 (p281)

			{0xf1c0, 0x0100, Operation::BTSTb, Decoder::BTST},		// 4-62 (p166)
			{0xffc0, 0x0800, Operation::BTSTb, Decoder::BTSTIMM},	// 4-63 (p167)

			{0xf0f8, 0x50c8, Operation::DBcc, Decoder::DBcc},		// 4-91 (p195)

			{0xffc0, 0x4200, Operation::CLRb, Decoder::CLRNEGNEGXNOT},	// 4-73 (p177)
			{0xffc0, 0x4240, Operation::CLRw, Decoder::CLRNEGNEGXNOT},	// 4-73 (p177)
			{0xffc0, 0x4280, Operation::CLRl, Decoder::CLRNEGNEGXNOT},	// 4-73 (p177)
			{0xffc0, 0x4400, Operation::NEGb, Decoder::CLRNEGNEGXNOT},	// 4-144 (p248)
			{0xffc0, 0x4440, Operation::NEGw, Decoder::CLRNEGNEGXNOT},	// 4-144 (p248)
			{0xffc0, 0x4480, Operation::NEGl, Decoder::CLRNEGNEGXNOT},	// 4-144 (p248)
			{0xffc0, 0x4000, Operation::NEGXb, Decoder::CLRNEGNEGXNOT},	// 4-146 (p250)
			{0xffc0, 0x4040, Operation::NEGXw, Decoder::CLRNEGNEGXNOT},	// 4-146 (p250)
			{0xffc0, 0x4080, Operation::NEGXl, Decoder::CLRNEGNEGXNOT},	// 4-146 (p250)
			{0xffc0, 0x4600, Operation::NOTb, Decoder::CLRNEGNEGXNOT},	// 4-148 (p250)
			{0xffc0, 0x4640, Operation::NOTw, Decoder::CLRNEGNEGXNOT},	// 4-148 (p250)
			{0xffc0, 0x4680, Operation::NOTl, Decoder::CLRNEGNEGXNOT},	// 4-148 (p250)

			{0xf1d8, 0xe100, Operation::ASLb, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xf1d8, 0xe140, Operation::ASLw, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xf1d8, 0xe180, Operation::ASLl, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xffc0, 0xe1c0, Operation::ASLm, Decoder::ASLRLSLRROLRROXLRm},	// 4-22 (p126)

			{0xf1d8, 0xe000, Operation::ASRb, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xf1d8, 0xe040, Operation::ASRw, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xf1d8, 0xe080, Operation::ASRl, Decoder::ASLRLSLRROLRROXLRr},	// 4-22 (p126)
			{0xffc0, 0xe0c0, Operation::ASRm, Decoder::ASLRLSLRROLRROXLRm},	// 4-22 (p126)

			{0xf1d8, 0xe108, Operation::LSLb, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xf1d8, 0xe148, Operation::LSLw, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xf1d8, 0xe188, Operation::LSLl, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xffc0, 0xe3c0, Operation::LSLm, Decoder::ASLRLSLRROLRROXLRm},	// 4-113 (p217)

			{0xf1d8, 0xe008, Operation::LSRb, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xf1d8, 0xe048, Operation::LSRw, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xf1d8, 0xe088, Operation::LSRl, Decoder::ASLRLSLRROLRROXLRr},	// 4-113 (p217)
			{0xffc0, 0xe2c0, Operation::LSRm, Decoder::ASLRLSLRROLRROXLRm},	// 4-113 (p217)

			{0xf1d8, 0xe118, Operation::ROLb, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xf1d8, 0xe158, Operation::ROLw, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xf1d8, 0xe198, Operation::ROLl, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xffc0, 0xe7c0, Operation::ROLm, Decoder::ASLRLSLRROLRROXLRm},	// 4-160 (p264)

			{0xf1d8, 0xe018, Operation::RORb, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xf1d8, 0xe058, Operation::RORw, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xf1d8, 0xe098, Operation::RORl, Decoder::ASLRLSLRROLRROXLRr},	// 4-160 (p264)
			{0xffc0, 0xe6c0, Operation::RORm, Decoder::ASLRLSLRROLRROXLRm},	// 4-160 (p264)

			{0xf1d8, 0xe110, Operation::ROXLb, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xf1d8, 0xe150, Operation::ROXLw, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xf1d8, 0xe190, Operation::ROXLl, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xffc0, 0xe5c0, Operation::ROXLm, Decoder::ASLRLSLRROLRROXLRm},	// 4-163 (p267)

			{0xf1d8, 0xe010, Operation::ROXRb, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xf1d8, 0xe050, Operation::ROXRw, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xf1d8, 0xe090, Operation::ROXRl, Decoder::ASLRLSLRROLRROXLRr},	// 4-163 (p267)
			{0xffc0, 0xe4c0, Operation::ROXRm, Decoder::ASLRLSLRROLRROXLRm},	// 4-163 (p267)
		};

#ifndef NDEBUG
		// Verify no double mappings.
		for(int instruction = 0; instruction < 65536; ++instruction) {
			int hits = 0;
			for(const auto &mapping: mappings) {
				if((instruction & mapping.mask) == mapping.value) ++hits;
			}
			assert(hits < 2);
		}
#endif

		std::vector<size_t> micro_op_pointers(65536, std::numeric_limits<size_t>::max());

		// The arbitrary_base is used so that the offsets returned by assemble_program into
		// storage_.all_bus_steps_ can be retained and mapped into the final version of
		// storage_.all_bus_steps_ at the end.
		BusStep arbitrary_base;

#define op(...) 	storage_.all_micro_ops_.emplace_back(__VA_ARGS__)
#define seq(...)	&arbitrary_base + assemble_program(__VA_ARGS__)
#define ea(n)		&storage_.effective_address_[n].full
#define a(n)		&storage_.address_[n].full

		// Perform a linear search of the mappings above for this instruction.
		for(size_t instruction = 0; instruction < 65536; ++instruction)	{
			for(const auto &mapping: mappings) {
				if((instruction & mapping.mask) == mapping.value) {
					auto operation = mapping.operation;
					const auto micro_op_start = storage_.all_micro_ops_.size();

					// The following fields are used commonly enough to be worht pulling out here.
					const int ea_register = instruction & 7;
					const int ea_mode = (instruction >> 3) & 7;

					switch(mapping.decoder) {
						case Decoder::ADDSUB: {
							// ADD and SUB definitely always involve a data register and an arbitrary addressing mode;
							// which direction they operate in depends on bit 8.
							const bool reverse_source_destination = !(instruction & 256);
							const int data_register = (instruction >> 9) & 7;

							const int mode = combined_mode(ea_mode, ea_register);
							const bool is_byte_access = !!((instruction >> 6)&3);
							const bool is_long_word_access = ((instruction >> 6)&3) == 2;

							if(reverse_source_destination) {
								storage_.instructions[instruction].destination = &storage_.data_[data_register];
								storage_.instructions[instruction].source = &storage_.source_bus_data_[0];
								storage_.instructions[instruction].source_address = &storage_.address_[ea_register];

								// Perform [ADD/SUB].blw <ea>, Dn
								switch(mode | (is_long_word_access ? 0x100 : 0x000)) {
									default: continue;

									case 0x000:	// ADD/SUB.bw Dn, Dn
										storage_.instructions[instruction].source = &storage_.data_[ea_register];
										op(Action::PerformOperation, seq("np"));
									break;

									case 0x100: // ADD/SUB.l Dn, Dn
										storage_.instructions[instruction].source = &storage_.data_[ea_register];
										op(Action::PerformOperation, seq("np nn"));
									break;

									case 0x001:	// ADD/SUB.bw An, Dn
										storage_.instructions[instruction].source = &storage_.address_[ea_register];
										op(Action::PerformOperation, seq("np"));
									break;

									case 0x101:	// ADD/SUB.l An, Dn
										storage_.instructions[instruction].source = &storage_.address_[ea_register];
										op(Action::PerformOperation, seq("np nn"));
									break;

									case 0x002:	// ADD/SUB.bw (An), Dn
									case 0x003:	// ADD/SUB.bw (An)+, Dn
										op(Action::None, seq("nr np", { a(ea_register) }, !is_byte_access));
										if(ea_mode == 0x03) {
											op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask);
										}
										op(Action::PerformOperation);
									break;

									case 0x102:	// ADD/SUB.l (An), Dn
									case 0x103:	// ADD/SUB.l (An)+, Dn
										op(	int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask,
											seq("nR+ nr np n", { ea(0), ea(0) }));
										if(ea_mode == 0x03) {
											op(int(Action::Increment4) | MicroOp::SourceMask);
										}
										op(Action::PerformOperation);
									break;

									case 0x004:	// ADD/SUB.bw -(An), Dn
										op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::SourceMask,
											seq("n nr np", { a(ea_register) }, !is_byte_access));
										op(Action::PerformOperation);
									break;

									case 0x104:	// ADD/SUB.l -(An), Dn
										op(int(Action::Decrement4) | MicroOp::SourceMask);
										op(	int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask,
											seq("n nR+ nr np n", { ea(0), ea(0) }));
										op(Action::PerformOperation);
									break;

									case 0x010:	// ADD/SUB.bw (xxx).w, Dn
										op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
											seq("np nr np", { ea(0) }, !is_byte_access));
										op(Action::PerformOperation);
									break;

									case 0x110:	// ADD/SUB.l (xxx).w, Dn
										op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
											seq("np nR+ nr np n", { ea(0), ea(0) }));
										op(Action::PerformOperation);
									break;

									case 0x011:	// ADD/SUB.bw (xxx).l, Dn
										op(Action::None, seq("np"));
										op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
											seq("np nr np", { ea(0) }, !is_byte_access));
										op(Action::PerformOperation);
									break;

									case 0x111:	// ADD/SUB.l (xxx).l, Dn
										op(Action::None, seq("np"));
										op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
											seq("np nR+ nr np n", { ea(0), ea(0) }));
										op(Action::PerformOperation);
									break;

									case 0x012:	// ADD/SUB.bw (d16, PC), Dn
									case 0x013:	// ADD/SUB.bw (d8, PC, Xn), Dn
									case 0x005:	// ADD/SUB.bw (d16, An), Dn
									case 0x006:	// ADD/SUB.bw (d8, An, Xn), Dn
										op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
											seq(pseq("np nr np", mode), { ea(0) }, !is_byte_access));
										op(Action::PerformOperation);
									break;

									case 0x112:	// ADD/SUB.l (d16, PC), Dn
									case 0x113:	// ADD/SUB.l (d8, PC, Xn), Dn
									case 0x105:	// ADD/SUB.l (d16, An), Dn
									case 0x106:	// ADD/SUB.l (d8, An, Xn), Dn
										op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
											seq(pseq("np nR+ nr np n", mode), { ea(0), ea(0) }));
										op(Action::PerformOperation);
									break;

									case 0x014:	// ADD/SUB.bw #, Dn
										op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
										op(Action::PerformOperation);
									break;

									case 0x114:	// ADD/SUB.l #, Dn
										op(Action::None, seq("np"));
										op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np nn"));
										op(Action::PerformOperation);
									break;
								}
							} else {
								storage_.instructions[instruction].source = &storage_.data_[data_register];

								const auto destination_register = ea_register;
								storage_.instructions[instruction].destination = &storage_.destination_bus_data_[1];
								storage_.instructions[instruction].destination_address = &storage_.address_[destination_register];

								// Perform [ADD/SUB].blw Dn, <ea>
								switch(mode | (is_long_word_access ? 0x100 : 0x000)) {
									default: continue;

									case 0x002:	// ADD/SUB.bw Dn, (An)
									case 0x003:	// ADD/SUB.bw Dn, (An)+
										op(Action::None, seq("nrd np", { a(destination_register) }, !is_byte_access));
										op(Action::PerformOperation, seq("nw", { a(destination_register) }, !is_byte_access));
										if(ea_mode == 0x03) {
											op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::DestinationMask);
										}
									break;

									case 0x102:	// ADD/SUB.l Dn, (An)
									case 0x103:	// ADD/SUB.l Dn, (An)+
										op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("nRd+ nrd np", { ea(1), ea(1) }));
										op(Action::PerformOperation, seq("nw- nW", { ea(1), ea(1) }));
										if(ea_mode == 0x03) {
											op(int(Action::Increment4) | MicroOp::DestinationMask);
										}
									break;

									case 0x004:	// ADD/SUB.bw Dn, -(An)
										op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::DestinationMask,
											seq("n nrd np", { a(destination_register) }, !is_byte_access));
										op(Action::PerformOperation, seq("nw", { a(destination_register) }, !is_byte_access));
									break;

									case 0x104:	// ADD/SUB.l Dn, -(An)
										op(	int(Action::Decrement4) | MicroOp::DestinationMask);
										op(	int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask,
											seq("n nRd+ nrd np", { ea(1), ea(1) }));
										op(	Action::PerformOperation,
											seq("nw- nW", { ea(1), ea(1) }));
									break;

									case 0x005:	// ADD/SUB.bw (d16, An), Dn
									case 0x006:	// ADD/SUB.bw (d8, An, Xn), Dn
										op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
											seq(pseq("np nrd np", mode), { ea(1) }, !is_byte_access));
										op(Action::PerformOperation, seq("nw", { ea(1) }, !is_byte_access));
									break;

									case 0x105:	// ADD/SUB.l (d16, An), Dn
									case 0x106:	// ADD/SUB.l (d8, An, Xn), Dn
										op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
											seq(pseq("np nRd+ nrd np", mode), { ea(1), ea(1) }));
										op(Action::PerformOperation, seq("nw- nW", { ea(1), ea(1) }));
									break;

									case 0x010:	// ADD/SUB.bw Dn, (xxx).w
										op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
											seq("np nrd np", { ea(1) }, !is_byte_access));
										op(Action::PerformOperation, seq("nw", { ea(1) }, !is_byte_access));
									break;

									case 0x110:	// ADD/SUB.l Dn, (xxx).w
										op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
											seq("np nRd+ nrd np", { ea(1), ea(1) }));
										op(	Action::PerformOperation,
											seq("nw- nW", { ea(1), ea(1) }));
									break;

									case 0x011:	// ADD/SUB.bw Dn, (xxx).l
										op(Action::None, seq("np"));
										op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
											seq("np nrd np", { ea(1) }, !is_byte_access));
										op(Action::PerformOperation, seq("nw", { ea(1) }, !is_byte_access));
									break;

									case 0x111:	// ADD/SUB.l Dn, (xxx).l
										op(Action::None, seq("np"));
										op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
											seq("np nRd+ nrd np", { ea(1), ea(1) }));
										op(	Action::PerformOperation,
											seq("nw- nW", { ea(1), ea(1) }));
									break;
								}
							}
						} break;

						case Decoder::ADDASUBA: {
							const int destination_register = (instruction >> 9) & 7;
							storage_.instructions[instruction].set_destination(storage_, 1, destination_register);
							storage_.instructions[instruction].set_source(storage_, ea_mode, ea_register);

							const int mode = combined_mode(ea_mode, ea_register);
							const bool is_long_word_access = !((instruction >> 8)&1);

							switch(mode | (is_long_word_access ? 0x100 : 0x000)) {
								default: continue;

								case 0x000:	// ADDA/SUBA.w Dn, An
								case 0x100:	// ADDA/SUBA.l Dn, An
									op(Action::PerformOperation, seq("np nn"));
								break;

								case 0x001:	// ADDA/SUBA.w An, An
								case 0x101:	// ADDA/SUBA.l An, An
									op(Action::PerformOperation, seq("np nn"));
								break;

								case 0x002:	// ADDA/SUBA.w (An), An
								case 0x003:	// ADDA/SUBA.w (An)+, An
									op(Action::None, seq("nr np nn", { a(ea_register) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x102:	// ADDA/SUBA.l (An), An
								case 0x103:	// ADDA/SUBA.l (An)+, An
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("nR+ nr np n", { ea(0), ea(0) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment4) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x004:	// ADDA/SUBA.w -(An), An
									op(int(Action::Decrement2) | MicroOp::SourceMask);
									op(Action::None, seq("n nr np nn", { a(ea_register) }));
									op(Action::PerformOperation);
								break;

								case 0x104:	// ADDA/SUBA.l -(An), An
									op(int(Action::Decrement4) | MicroOp::SourceMask);
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("n nR+ nr np n", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x005:	// ADDA/SUBA.w (d16, An), An
								case 0x006:	// ADDA/SUBA.w (d8, An, Xn), An
									op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
										seq(pseq("np nr np nn", mode), { ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x105:	// ADDA/SUBA.l (d16, An), An
								case 0x106:	// ADDA/SUBA.l (d8, An, Xn), An
									op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
										seq(pseq("np nR+ nr np n", mode), { ea(1), ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x010:	// ADDA/SUBA.w (xxx).w, An
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np nn", { ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x110:	// ADDA/SUBA.l (xxx).w, An
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr np n", { ea(1), ea(1) }));
									op(	Action::PerformOperation);
								break;

								case 0x011:	// ADDA/SUBA.w (xxx).l, An
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np nn", { ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x111:	// ADDA/SUBA.l (xxx).l, An
									op(Action::None, seq("np"));
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr np n", { ea(1), ea(1) }));
									op(	Action::PerformOperation);
								break;

								case 0x014:	// ADDA/SUBA.w #, An
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np nn"));
									op(Action::PerformOperation);
								break;

								case 0x114:	// ADDA/SUBA.l #, Dn
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np nn"));
									op(Action::PerformOperation);
								break;
							}
						} break;

						// This decoder actually decodes nothing; it just schedules a PerformOperation followed by an empty step.
						case Decoder::Bcc: {
							op(Action::PerformOperation);
							op();	// The above looks terminal, but will be dynamically reprogrammed.
						} break;

						// A little artificial, there's nothing really to decode for BRA.
						case Decoder::BRA: {
							op(Action::PerformOperation, seq("n np np"));
						} break;

						// Decodes a BTST, potential mutating the operation into a BTSTl.
						case Decoder::BTST: {
							const int mask_register = (instruction >> 9) & 7;
							storage_.instructions[instruction].set_source(storage_, 0, mask_register);
							storage_.instructions[instruction].set_destination(storage_, ea_mode, ea_register);

							const int mode = combined_mode(ea_mode, ea_register);
							switch(mode) {
								default: continue;

								case 0x00:	// BTST.l Dn, Dn
									operation = Operation::BTSTl;
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x02:	// BTST.b Dn, (An)
								case 0x03:	// BTST.b Dn, (An)+
									op(Action::None, seq("nrd np", { &storage_.data_[ea_register].full }, false));
									op(Action::PerformOperation);
									if(mode == 0x03) {
										op(int(Action::Increment1) | MicroOp::DestinationMask);
									}
								break;

								case 0x04:	// BTST.b Dn, -(An)
									op(int(Action::Decrement1) | MicroOp::DestinationMask, seq("n nrd np", { &storage_.data_[ea_register].full }, false));
									op(Action::PerformOperation);
								break;

								case 0x05:	// BTST.b Dn, (d16, An)
								case 0x06:	// BTST.b Dn, (d8, An, Xn)
								case 0x12:	// BTST.b Dn, (d16, PC)
								case 0x13:	// BTST.b Dn, (d8, PC, Xn)
									op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
										seq(pseq("np nrd np", mode), { ea(1) }, false));
									op(Action::PerformOperation);
								break;

								case 0x10:	// BTST.b Dn, (xxx).w
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }, false));
									op(Action::PerformOperation);
								break;

								case 0x11:	// BTST.b Dn, (xxx).l
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }, false));
									op(Action::PerformOperation);
								break;
							}
						} break;

						case Decoder::BTSTIMM: {
							storage_.instructions[instruction].source = &storage_.source_bus_data_[0];
							storage_.instructions[instruction].set_destination(storage_, ea_mode, ea_register);

							const int mode = combined_mode(ea_mode, ea_register);
							switch(mode) {
								default: continue;

								case 0x00:	// BTST.l #, Dn
									operation = Operation::BTSTl;
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np n"));
									op(Action::PerformOperation);
								break;

								case 0x02:	// BTST.b #, (An)
								case 0x03:	// BTST.b #, (An)+
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np nrd np", { &storage_.data_[ea_register].full }, false));
									op(Action::PerformOperation);
									if(mode == 0x03) {
										op(int(Action::Increment1) | MicroOp::DestinationMask);
									}
								break;

								case 0x04:	// BTST.b #, -(An)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(int(Action::Decrement1) | MicroOp::DestinationMask, seq("n nrd np", { &storage_.data_[ea_register].full }, false));
									op(Action::PerformOperation);
								break;

								case 0x05:	// BTST.b #, (d16, An)
								case 0x06:	// BTST.b #, (d8, An, Xn)
								case 0x12:	// BTST.b #, (d16, PC)
								case 0x13:	// BTST.b #, (d8, PC, Xn)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
										seq(pseq("np nrd np", mode), { ea(1) }, false));
									op(Action::PerformOperation);
								break;

								case 0x10:	// BTST.b #, (xxx).w
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }, false));
									op(Action::PerformOperation);
								break;

								case 0x11:	// BTST.b #, (xxx).l
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }, false));
									op(Action::PerformOperation);
								break;
							}
						} break;

						// Decodes the format used by ABCD and SBCD.
						case Decoder::ABCDSBCD: {
							const int destination_register = (instruction >> 9) & 7;

							if(instruction & 8) {
								storage_.instructions[instruction].source = &storage_.source_bus_data_[0];
								storage_.instructions[instruction].destination = &storage_.destination_bus_data_[0];
								storage_.instructions[instruction].source_address = &storage_.address_[ea_register];
								storage_.instructions[instruction].destination_address = &storage_.address_[destination_register];

								op(	int(Action::Decrement1) | MicroOp::SourceMask | MicroOp::DestinationMask,
									seq("n nr nr np nw", { a(ea_register), a(destination_register), a(destination_register) }, false));
								op(Action::PerformOperation);
							} else {
								storage_.instructions[instruction].source = &storage_.data_[ea_register];
								storage_.instructions[instruction].destination = &storage_.data_[destination_register];

								op(Action::PerformOperation, seq("np n"));
							}
						} break;

						case Decoder::ASLRLSLRROLRROXLRr: {
							storage_.instructions[instruction].set_destination(storage_, 0, ea_register);

							// All further decoding occurs at runtime; that's also when the proper number of
							// no-op cycles will be scheduled.
							if(((instruction >> 6) & 3) == 2) {
								op(Action::None, seq("np nn"));
							} else {
								op(Action::None, seq("np n"));
							}

							// Use a no-op bus cycle that can have its length filled in later.
							op(Action::PerformOperation, seq("r"));
						} break;

						case Decoder::ASLRLSLRROLRROXLRm: {
							storage_.instructions[instruction].set_destination(storage_, ea_mode, ea_register);

							switch(combined_mode(ea_mode, ea_register)) {
								default: continue;

								case 0x02:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (An)
								case 0x03:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (An)+
									op(Action::None, seq("nrd np", { a(ea_register) }));
									op(Action::PerformOperation, seq("nw", { a(ea_register) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment2) | MicroOp::DestinationMask);
									}
								break;

								case 0x04:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w -(An)
									op(int(Action::Decrement2) | MicroOp::DestinationMask, seq("n nrd np", { a(ea_register) }));
									op(Action::PerformOperation, seq("nw", { a(ea_register) }));
								break;

								case 0x05:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (d16, An)
								case 0x06:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (d8, An, Xn)
									op(	calc_action_for_mode(ea_mode) | MicroOp::DestinationMask,
										seq(pseq("np nrd np", ea_mode), { ea(1) }));
									op(Action::PerformOperation, seq("nw", { ea(1) }));
								break;

								case 0x10:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (xxx).w
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }));
									op(Action::PerformOperation, seq("nw", { ea(1) }));
								break;

								case 0x11:	// AS(L/R)/LS(L/R)/RO(L/R)/ROX(L/R).w (xxx).l
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd np", { ea(1) }));
									op(Action::PerformOperation, seq("nw", { ea(1) }));
								break;
							}
						} break;

						case Decoder::CLRNEGNEGXNOT: {
							const bool is_byte_access = !!((instruction >> 6)&3);
							const bool is_long_word_access = ((instruction >> 6)&3) == 2;

							storage_.instructions[instruction].set_destination(storage_, ea_mode, ea_register);

							const int mode = combined_mode(ea_mode, ea_register);
							switch((is_long_word_access ? 0x100 : 0x000) | mode) {
								default: continue;

								case 0x000: // [CLR/NEG/NEGX/NOT].bw Dn
								case 0x100: // [CLR/NEG/NEGX/NOT].l Dn
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x002:	// [CLR/NEG/NEGX/NOT].bw (An)
								case 0x003:	// [CLR/NEG/NEGX/NOT].bw (An)+
									op(Action::None, seq("nrd", { a(ea_register) }, !is_byte_access));
									op(Action::PerformOperation, seq("np nw", { a(ea_register) }, !is_byte_access));
									if(ea_mode == 0x03) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::DestinationMask);
									}
								break;

								case 0x102:	// [CLR/NEG/NEGX/NOT].l (An)
								case 0x103:	// [CLR/NEG/NEGX/NOT].l (An)+
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("nRd+ nrd", { ea(1), ea(1) }));
									op(Action::PerformOperation, seq("np nw- nW", { ea(1), ea(1) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment4) | MicroOp::DestinationMask);
									}
								break;

								case 0x004:	// [CLR/NEG/NEGX/NOT].bw -(An)
									op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::DestinationMask,
										seq("nrd", { a(ea_register) }, !is_byte_access));
									op(Action::PerformOperation, seq("np nw", { a(ea_register) }, !is_byte_access));
								break;

								case 0x104:	// [CLR/NEG/NEGX/NOT].l -(An)
									op(int(Action::Decrement4) | MicroOp::DestinationMask);
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask,
										seq("n nRd+ nrd", { ea(1), ea(1) }));
									op(Action::PerformOperation, seq("np nw- nW", { ea(1), ea(1) }));
								break;

								case 0x005:	// [CLR/NEG/NEGX/NOT].bw (d16, An)
								case 0x006:	// [CLR/NEG/NEGX/NOT].bw (d8, An, Xn)
									op(	calc_action_for_mode(ea_mode) | MicroOp::DestinationMask,
										seq(pseq("np nrd", ea_mode), { ea(1) },
										!is_byte_access));
									op(Action::PerformOperation, seq("np nw", { ea(1) }, !is_byte_access));
								break;

								case 0x105:	// [CLR/NEG/NEGX/NOT].l (d16, An)
								case 0x106:	// [CLR/NEG/NEGX/NOT].l (d8, An, Xn)
									op(	calc_action_for_mode(ea_mode) | MicroOp::DestinationMask,
										seq(pseq("np nRd+ nrd", ea_mode), { ea(1), ea(1) }));
									op(Action::PerformOperation, seq("np nw- nW", { ea(1), ea(1) }));
								break;

								case 0x010:	// [CLR/NEG/NEGX/NOT].bw (xxx).w
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd", { ea(1) }, !is_byte_access));
									op(Action::PerformOperation,
										seq("np nw", { ea(1) }, !is_byte_access));
								break;

								case 0x110:	// [CLR/NEG/NEGX/NOT].l (xxx).w
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nRd+ nrd", { ea(1), ea(1) }));
									op(Action::PerformOperation,
										seq("np nw- nW", { ea(1), ea(1) }));
								break;

								case 0x011:	// [CLR/NEG/NEGX/NOT].bw (xxx).l
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nrd", { ea(1) }, !is_byte_access));
									op(Action::PerformOperation,
										seq("np nw", { ea(1) }, !is_byte_access));
								break;

								case 0x111:	// [CLR/NEG/NEGX/NOT].l (xxx).l
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask,
										seq("np nRd+ nrd", { ea(1), ea(1) }));
									op(Action::PerformOperation,
										seq("np nw- nW", { ea(1), ea(1) }));
								break;
							}
						} break;

						case Decoder::CMP: {
							const auto source_register = (instruction >> 9) & 7;

							storage_.instructions[instruction].destination = &storage_.data_[source_register];
							storage_.instructions[instruction].set_source(storage_, ea_mode, ea_register);

							const bool is_long_word_access = mapping.operation == Operation::CMPl;
							const bool is_byte_access = mapping.operation == Operation::CMPb;

							// Byte accesses are not allowed with address registers.
							if(is_byte_access && ea_mode == 1) {
								continue;
							}

							const int mode = (is_long_word_access ? 0x100 : 0) | combined_mode(ea_mode, ea_register);
							switch(mode) {
								default: continue;

								case 0x000:	// CMP.bw Dn, Dn
								case 0x100:	// CMP.l Dn, Dn
								case 0x001:	// CMP.w An, Dn
								case 0x101:	// CMP.l An, Dn
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x002:	// CMP.bw (An), Dn
								case 0x003:	// CMP.bw (An)+, Dn
									op(Action::None, seq("nr np", { a(ea_register) }, !is_byte_access));
									if(ea_mode == 0x03) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x102:	// CMP.l (An), Dn
								case 0x103:	// CMP.l (An)+, Dn
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("nR+ nr np n", { ea(0), ea(0) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment4) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x004:	// CMP.bw -(An), Dn
									op(	int(is_byte_access ? Action::Decrement1 : Action::Decrement2) | MicroOp::SourceMask,
										seq("n nr np", { a(ea_register) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x104:	// CMP.l -(An), Dn
									op(int(Action::Decrement4) | MicroOp::SourceMask);
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("n nR+ nr np n", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x005:	// CMP.bw (d16, An), Dn
								case 0x006:	// CMP.bw (d8, An, Xn), Dn
								case 0x012:	// CMP.bw (d16, PC), Dn
								case 0x013:	// CMP.bw (d8, PC, Xn), Dn
									op(	calc_action_for_mode(ea_mode) | MicroOp::SourceMask,
										seq(pseq("np nr np", ea_mode), { ea(0) },
										!is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x105:	// CMP.l (d16, An), Dn
								case 0x106:	// CMP.l (d8, An, Xn), Dn
								case 0x112:	// CMP.l (d16, PC), Dn
								case 0x113:	// CMP.l (d8, PC, Xn), Dn
									op(	calc_action_for_mode(ea_mode) | MicroOp::SourceMask,
										seq(pseq("np nR+ nr np n", ea_mode), { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x010:	// CMP.bw (xxx).w, Dn
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x110:	// CMP.l (xxx).w, Dn
									op(	int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr np n", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x011:	// CMP.bw (xxx).l, Dn
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x111:	// CMP.l (xxx).l, Dn
									op(Action::None, seq("np"));
									op(	int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr np n", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x014:	// CMP.br #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::PerformOperation, seq("np np"));
								break;

								case 0x114:	// CMP.l #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::None, seq("np"));
									op(Action::PerformOperation, seq("np np n"));
								break;
							}
						} break;

						case Decoder::CMPA: {
							const int destination_register = (instruction >> 9) & 7;

							storage_.instructions[instruction].set_source(storage_, ea_mode, ea_register);
							storage_.instructions[instruction].destination = &storage_.address_[destination_register];

							const int mode = combined_mode(ea_mode, ea_register);
							const bool is_long_word_access = mapping.operation == Operation::CMPl;
							switch((is_long_word_access ? 0x100 : 0) | mode) {
								default: continue;

								case 0x000: // CMPA.w Dn, An
								case 0x001: // CMPA.w An, An
								case 0x100: // CMPA.l Dn, An
								case 0x101: // CMPA.l An, An
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x002:	// CMPA.w (An), An
								case 0x003:	// CMPA.w (An)+, An
									op(Action::None, seq("nr", { a(ea_register) }));
									op(Action::PerformOperation, seq("np n"));
									if(ea_mode == 0x03) {
										op(int(Action::Increment2) | MicroOp::SourceMask);
									}
								break;

								case 0x102:	// CMPA.l (An), An
								case 0x103:	// CMPA.l (An)+, An
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np n"));
									if(ea_mode == 0x03) {
										op(int(Action::Increment4) | MicroOp::SourceMask);
									}
								break;

								case 0x004:	// CMPA.w -(An), An
									op(int(Action::Decrement2) | MicroOp::SourceMask, seq("n nr", { a(ea_register) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x104:	// CMPA.l -(An), An
									op(int(Action::Decrement4) | MicroOp::SourceMask);
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x012:	// CMPA.w (d16, PC), An
								case 0x013:	// CMPA.w (d8, PC, Xn), An
								case 0x005:	// CMPA.w (d16, An), An
								case 0x006:	// CMPA.w (d8, An, Xn), An
									op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
										seq(pseq("np nr", ea_mode), { ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x112:	// CMPA.l (d16, PC), An
								case 0x113:	// CMPA.l (d8, PC, Xn), An
								case 0x105:	// CMPA.l (d16, An), An
								case 0x106:	// CMPA.l (d8, An, Xn), An
									op(	calc_action_for_mode(mode) | MicroOp::SourceMask,
										seq(pseq("np nR+ nr", ea_mode), { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x010:	// CMPA.w (xxx).w, An
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr",  { ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x110:	// CMPA.l (xxx).w, An
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nR+ nr",  { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x011:	// CMPA.w (xxx).l, An
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr",  { ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x111:	// CMPA.l (xxx).l, An
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nR+ nr",  { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np n"));
								break;

								case 0x014:	// CMPA.w #, An
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::PerformOperation, seq("np np n"));
								break;

								case 0x114:	// CMPA.l #, An
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::None, seq("np"));
									op(Action::PerformOperation, seq("np np n"));
								break;
							}
						} break;

						case Decoder::CMPI: {
							if(ea_mode == 1) continue;

							const auto destination_mode = ea_mode;
							const auto destination_register = ea_register;

							storage_.instructions[instruction].source = &storage_.source_bus_data_[0];
							storage_.instructions[instruction].set_destination(storage_, destination_mode, destination_register);

							const bool is_byte_access = mapping.operation == Operation::CMPb;
							const bool is_long_word_access = mapping.operation == Operation::CMPl;
							const int mode = (is_long_word_access ? 0x100 : 0) | combined_mode(destination_mode, destination_register);
							switch(mode) {
								case 0x000:	// CMPI.bw #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::PerformOperation, seq("np np"));
								break;

								case 0x100:	// CMPI.l #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::None, seq("np"));
									op(Action::PerformOperation, seq("np np n"));
								break;

								case 0x002:	// CMPI.bw #, (An)
								case 0x003:	// CMPI.bw #, (An)+
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np nrd np", { a(destination_register) }, !is_byte_access));
									if(mode == 0x03) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::DestinationMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x102:	// CMPI.l #, (An)
								case 0x103:	// CMPI.l #, (An)+
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np nRd+ nrd np", { ea(1), ea(1) }));
									if(mode == 0x103) {
										op(int(Action::Increment4) | MicroOp::DestinationMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x004:	// CMPI.bw #, -(An)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np n"));
									op(int(is_byte_access ? Action::Decrement1 : Action::Decrement1) | MicroOp::DestinationMask, seq("nrd np", { a(destination_register) }));
									op(Action::PerformOperation);
								break;

								case 0x104:	// CMPI.l #, -(An)
									op(int(Action::Decrement4) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np n"));
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("nRd+ nrd np", { ea(1), ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x012:	// CMPI.bw #, (d16, PC)
								case 0x013:	// CMPI.bw #, (d8, PC, Xn)
								case 0x005:	// CMPI.bw #, (d16, An)
								case 0x006:	// CMPI.bw #, (d8, An, Xn)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
										seq(pseq("nrd np", mode), { ea(1) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x112:	// CMPI.l #, (d16, PC)
								case 0x113:	// CMPI.l #, (d8, PC, Xn)
								case 0x105:	// CMPI.l #, (d16, An)
								case 0x106:	// CMPI.l #, (d8, An, Xn)
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(	calc_action_for_mode(mode) | MicroOp::DestinationMask,
										seq(pseq("np nRd+ nrd np", mode), { ea(1), ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x010:	// CMPI.bw #, (xxx).w
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nrd np",  { ea(1) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x110:	// CMPI.l #, (xxx).w
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nRd+ nrd np",  { ea(1), ea(1) }));
									op(Action::PerformOperation);
								break;

								case 0x011:	// CMPI.bw #, (xxx).l
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nrd np",  { ea(1) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x111:	// CMPI.l #, (xxx).l
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nRd+ nrd np",  { ea(1), ea(1) }));
									op(Action::PerformOperation);
								break;

								default: continue;
							}
						} break;

						case Decoder::CMPM: {
							const int source_register = (instruction >> 9)&7;
							const int destination_register = ea_register;

							storage_.instructions[instruction].set_source(storage_, 1, source_register);
							storage_.instructions[instruction].set_destination(storage_, 1, destination_register);

							const bool is_byte_operation = operation == Operation::CMPw;

							switch(operation) {
								default: continue;

								case Operation::CMPb:	// CMPM.b, (An)+, (An)+
								case Operation::CMPw:	// CMPM.w, (An)+, (An)+
									op(Action::None, seq("nr nr np", {a(source_register), a(destination_register)}, !is_byte_operation));
									op(Action::PerformOperation);
									op(int(is_byte_operation ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask | MicroOp::DestinationMask);
								break;

								case Operation::CMPl:
									op(	int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask | MicroOp::DestinationMask,
										seq("nR+ nr nRd+ nrd np", {ea(0), ea(0), ea(1), ea(1)}));
									op(Action::PerformOperation);
									op(int(Action::Increment4) | MicroOp::SourceMask | MicroOp::DestinationMask);
								break;
							}
						} break;

						case Decoder::DBcc: {
							storage_.instructions[instruction].source = &storage_.data_[ea_register];

 							// Jump straight into deciding what steps to take next,
 							// which will be selected dynamically.
							op(Action::PerformOperation);
							op();
						} break;

						case Decoder::JMP: {
							storage_.instructions[instruction].source = &storage_.effective_address_[0];
							const int mode = combined_mode(ea_mode, ea_register);
							switch(mode) {
								default: continue;
								case 0x02:	// JMP (An)
									storage_.instructions[instruction].source = &storage_.address_[ea_register];
									op(Action::PerformOperation, seq("np np"));
								break;

								case 0x12:	// JMP (d16, PC)
								case 0x05:	// JMP (d16, An)
									op(calc_action_for_mode(mode) | MicroOp::SourceMask, seq("n np np"));
									op(Action::PerformOperation);
								break;

								case 0x13:	// JMP (d8, PC, Xn)
								case 0x06:	// JMP (d8, An, Xn)
									op(calc_action_for_mode(mode) | MicroOp::SourceMask, seq("n nn np np"));
									op(Action::PerformOperation);
								break;

								case 0x10:	// JMP (xxx).W
									op(int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask);
									op(Action::PerformOperation, seq("n np np"));
								break;

								case 0x11:	// JMP (xxx).L
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask);
									op(Action::PerformOperation, seq("np np"));
								break;
							}
						} break;

						case Decoder::LEA: {
							const int destination_register = (instruction >> 9) & 7;
							storage_.instructions[instruction].destination = &storage_.address_[destination_register];

							const int mode = combined_mode(ea_mode, ea_register);
							switch(mode) {
								default: continue;
								case 0x04:
									storage_.instructions[instruction].source = &storage_.address_[ea_register];
								break;
								case 0x05:	case 0x06:	case 0x10:
								case 0x11:	case 0x12:	case 0x13:
									storage_.instructions[instruction].source = &storage_.effective_address_[0];
								break;
							}

							switch(mode) {
								default: break;
								case 0x04:	// LEA (An), An		(i.e. MOVEA)
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x05:	// LEA (d16, An), An
								case 0x12:	// LEA (d16, PC), SR
									op(calc_action_for_mode(mode) | MicroOp::SourceMask, seq("np np"));
									op(Action::PerformOperation);
								break;

								case 0x06:	// LEA (d8, An, Xn), SR
								case 0x13:	// LEA (d8, PC, Xn), SR
									op(calc_action_for_mode(mode) | MicroOp::SourceMask, seq("n np n np"));
									op(Action::PerformOperation);
								break;

								case 0x10:	// LEA (xxx).W, An
									op(int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(Action::PerformOperation);
								break;

								case 0x11:	// LEA (xxx).L, An
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np np"));
									op(Action::PerformOperation);
								break;
							}
						} break;

						case Decoder::MOVEtoSRCCR: {
							if(ea_mode == 1) continue;
							storage_.instructions[instruction].set_source(storage_, ea_mode, ea_register);
							storage_.instructions[instruction].requires_supervisor = (operation == Operation::MOVEtoSR);

							/* DEVIATION FROM YACHT.TXT: it has all of these reading an extra word from the PC;
							this looks like a mistake so I've padded with nil cycles in the middle. */
							const int mode = combined_mode(ea_mode, ea_register);
							switch(mode) {
								case 0x00:	// MOVE Dn, SR
									op(Action::PerformOperation, seq("nn np"));
								break;

								case 0x02:	// MOVE (An), SR
								case 0x03:	// MOVE (An)+, SR
									op(Action::None, seq("nr nn nn np", { a(ea_register) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x04:	// MOVE -(An), SR
									op(Action::Decrement2, seq("n nr nn nn np", { a(ea_register) }));
									op(Action::PerformOperation);
								break;

								case 0x12:	// MOVE (d16, PC), SR
								case 0x13:	// MOVE (d8, PC, Xn), SR
								case 0x05:	// MOVE (d16, An), SR
								case 0x06:	// MOVE (d8, An, Xn), SR
									op(calc_action_for_mode(mode) | MicroOp::SourceMask, seq(pseq("np nr nn nn np", mode), { ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x10:	// MOVE (xxx).W, SR
									op(
										int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr nn nn np", { ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x11:	// MOVE (xxx).L, SR
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { ea(0) }));
									op(Action::PerformOperation, seq("nn nn np"));
								break;

								case 0x14:	// MOVE #, SR
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(int(Action::PerformOperation), seq("np nn nn np"));
								break;

								default: continue;
							}
						} break;

						case Decoder::MOVEq: {
							const int destination_register = (instruction >> 9) & 7;
							storage_.instructions[instruction].destination = &storage_.data_[destination_register];
							op(Action::PerformOperation, seq("np"));
						} break;

						// Decodes the format used by most MOVEs and all MOVEAs.
						case Decoder::MOVE: {
							const int destination_mode = (instruction >> 6) & 7;
							const int destination_register = (instruction >> 9) & 7;

							switch(ea_mode) {
								case 0:	// Dn
									storage_.instructions[instruction].source = &storage_.data_[ea_register];
								break;

								case 1:	// An
									storage_.instructions[instruction].source = &storage_.address_[ea_register];
								break;

								default: // (An), (An)+, -(An), (d16, An), (d8, An Xn), (xxx).W, (xxx).L, #
									storage_.instructions[instruction].source = &storage_.source_bus_data_[0];
									storage_.instructions[instruction].source_address = &storage_.address_[ea_register];
								break;
							}

							switch(destination_mode) {
								case 0:	// Dn
									storage_.instructions[instruction].destination = &storage_.data_[destination_register];
								break;

								case 1:	// An
									storage_.instructions[instruction].destination = &storage_.address_[destination_register];
								break;

								default: // (An), (An)+, -(An), (d16, An), (d8, An Xn), (xxx).W, (xxx).L, #
									storage_.instructions[instruction].destination = &storage_.destination_bus_data_[0];
									storage_.instructions[instruction].destination_address = &storage_.address_[destination_register];
								break;
							}

							const bool is_byte_access = mapping.operation == Operation::MOVEb;
							const bool is_long_word_access = mapping.operation == Operation::MOVEl;

							// There are no byte moves to address registers.
							if(is_byte_access && destination_mode == 1) {
								continue;
							}

							const int decrement_action = int(is_long_word_access ? Action::Decrement4 : (is_byte_access ? Action::Decrement1 : Action::Decrement2));
							const int increment_action = int(is_long_word_access ? Action::Increment4 : (is_byte_access ? Action::Increment1 : Action::Increment2));

							// Construct a single word to describe the addressing mode:
							//
							//	0xssdd, where ss or dd =
							//		0n with n a regular addresing mode between 0 and 6; or
							//		1n with n being the nominal 'register' where addressing mode is 7.
							//
							//	i.e.	(see 4-118 / p.222)
							//
							//		00 = Dn
							//		01 = An
							//		02 = (An)
							//		03 = (An)+
							//		04 = -(An)
							//		05 = (d16, An)
							//		06 = (d8, An, Xn)
							//		10 = (xxx).W
							//		11 = (xxx).L
							//		12 = (d16, PC)
							//		13 = (d8, PC, Xn)
							//		14 = #
							//
							// ... for no reason other than to make the switch below easy to read.
							int both_modes =
								(combined_mode(ea_mode, ea_register) << 8) |
								combined_mode(destination_mode, destination_register) |
								(is_long_word_access ? 0x10000 : 0);

							// If the move is to an address register, switch the MOVE to a MOVEA and
							// pretend it's to a data register; if the move is from an address register
							// then just call it a move from a data register.
							if((both_modes & 0xff) == 0x01) {
								both_modes &= ~0x00ff;
								operation = is_long_word_access ? Operation::MOVEAl : Operation::MOVEAw;
							}
							if((both_modes & 0xff00) == 0x0100) {
								both_modes &= ~0xff00;
							}

							switch(both_modes) {

							//
							// MOVE <ea>, Dn
							//

								case 0x10000:	// MOVE.l Dn, Dn
								case 0x00000:	// MOVE.bw Dn, Dn
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x10200:	// MOVE.l (An), Dn
								case 0x10300:	// MOVE.l (An)+, Dn
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("nR+ nr np", { ea(0), ea(0) }));
									if(ea_mode == 0x03) {
										op(int(Action::Increment4) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x00200:	// MOVE.bw (An), Dn
								case 0x00300:	// MOVE.bw (An)+, Dn
									op(Action::None, seq("nr np", { a(ea_register) }, !is_byte_access));
									if(ea_mode == 0x03) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask);
									}
									op(Action::PerformOperation);
								break;

								case 0x10400:	// MOVE.l -(An), Dn
									op(decrement_action | MicroOp::SourceMask);
									op(int(Action::CopyToEffectiveAddress) | MicroOp::SourceMask, seq("n nR+ nr np", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x00400:	// MOVE.bw -(An), Dn
									op(decrement_action | MicroOp::SourceMask, seq("n nr np", { a(ea_register) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x10500:	// MOVE.l (d16, An), Dn
								case 0x10600:	// MOVE.l (d8, An, Xn), Dn
								case 0x11200:	// MOVE.l (d16, PC), Dn
								case 0x11300:	// MOVE.l (d8, PC, Xn), Dn
									op(	calc_action_for_mode(both_modes >> 8) | MicroOp::SourceMask,
										seq(pseq("np nR+ nr np", both_modes >> 8), { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x00500:	// MOVE.bw (d16, An), Dn
								case 0x00600:	// MOVE.bw (d8, An, Xn), Dn
								case 0x01200:	// MOVE.bw (d16, PC), Dn
								case 0x01300:	// MOVE.bw (d8, PC, Xn), Dn
									op(	calc_action_for_mode(both_modes >> 8) | MicroOp::SourceMask,
										seq(pseq("np nr np", both_modes >> 8), { ea(0) },
										!is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x11000:	// MOVE.l (xxx).W, Dn
									op(
										int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr np", { ea(0), ea(0) }));
									op(Action::PerformOperation);
								break;

								case 0x01000:	// MOVE.bw (xxx).W, Dn
									op(
										int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr np", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation);
								break;

								case 0x11100:	// MOVE.l (xxx).L, Dn
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x01100:	// MOVE.bw (xxx).L, Dn
									op(Action::None, seq("np"));
									op(int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation, seq("np"));
								break;

								case 0x11400:	// MOVE.l #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::None, seq("np"));
									op(int(Action::PerformOperation), seq("np np"));
								break;

								case 0x01400:	// MOVE.bw #, Dn
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(int(Action::PerformOperation), seq("np np"));
								break;

							//
							// MOVE <ea>, (An)
							// MOVE <ea>, (An)+
							//

								case 0x10002:	// MOVE.l Dn, (An)
								case 0x10003:	// MOVE.l Dn, (An)+
									op(int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask);
									op(Action::SetMoveFlagsl, seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x00002:	// MOVE.bw Dn, (An)
								case 0x00003:	// MOVE.bw Dn, (An)+
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw, seq("nw np", { a(destination_register) }, !is_byte_access));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x10202:	// MOVE.l (An), (An)
								case 0x10302:	// MOVE.l (An)+, (An)
								case 0x10203:	// MOVE.l (An), (An)+
								case 0x10303:	// MOVE.l (An)+, (An)+
									op(	int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask | MicroOp::SourceMask,
										seq("nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("nW+ nw np", { ea(1), ea(1) }));
									if(ea_mode == 0x03 || destination_mode == 0x03) {
										op(
											increment_action |
											(ea_mode == 0x03 ? MicroOp::SourceMask : 0) |
											(ea_mode == 0x03 ? MicroOp::DestinationMask : 0));
									}
								break;

								case 0x00202:	// MOVE.bw (An), (An)
								case 0x00302:	// MOVE.bw (An)+, (An)
								case 0x00203:	// MOVE.bw (An), (An)+
								case 0x00303:	// MOVE.bw (An)+, (An)+
									op(Action::None, seq("nr", { a(ea_register) }));
									op(Action::PerformOperation, seq("nw np", { a(destination_register) }));
									if(ea_mode == 0x03 || destination_mode == 0x03) {
										op(
											increment_action |
											(ea_mode == 0x03 ? MicroOp::SourceMask : 0) |
											(ea_mode == 0x03 ? MicroOp::DestinationMask : 0));
									}
								break;

								case 0x10402:	// MOVE.l -(An), (An)
								case 0x10403:	// MOVE.l -(An), (An)+
									op(decrement_action | MicroOp::SourceMask);
									op(	int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask | MicroOp::SourceMask,
										seq("n nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x00402:	// MOVE.bw -(An), (An)
								case 0x00403:	// MOVE.bw -(An), (An)+
									op(decrement_action | MicroOp::SourceMask, seq("n nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x10502:	// MOVE.bw (d16, An), (An)
								case 0x10503:	// MOVE.bw (d16, An), (An)+
								case 0x10602:	// MOVE.bw (d8, An, Xn), (An)
								case 0x10603:	// MOVE.bw (d8, An, Xn), (An)+
								case 0x11202:	// MOVE.bw (d16, PC), (An)
								case 0x11203:	// MOVE.bw (d16, PC), (An)+
								case 0x11302:	// MOVE.bw (d8, PC, Xn), (An)
								case 0x11303:	// MOVE.bw (d8, PC, Xn), (An)+
									op( int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask);
									op(	calc_action_for_mode(both_modes >> 8) | MicroOp::SourceMask,
										seq(pseq("np nR+ nr", both_modes >> 8), { ea(0), ea(0) }));
									op(	Action::PerformOperation,
										seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x00502:	// MOVE.bw (d16, An), (An)
								case 0x00503:	// MOVE.bw (d16, An), (An)+
								case 0x00602:	// MOVE.bw (d8, An, Xn), (An)
								case 0x00603:	// MOVE.bw (d8, An, Xn), (An)+
								case 0x01202:	// MOVE.bw (d16, PC), (An)
								case 0x01203:	// MOVE.bw (d16, PC), (An)+
								case 0x01302:	// MOVE.bw (d8, PC, Xn), (An)
								case 0x01303:	// MOVE.bw (d8, PC, Xn), (An)+
									op(	calc_action_for_mode(both_modes >> 8) | MicroOp::SourceMask,
										seq(pseq("np nr", both_modes >> 8), { ea(0) },
										!is_byte_access));
									op(	Action::PerformOperation,
										seq("nw np", { a(destination_register) }, !is_byte_access));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x11002:	// MOVE.l (xxx).W, (An)
								case 0x11003:	// MOVE.l (xxx).W, (An)+
									op(	int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask );
									op(	int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr", { ea(0), ea(0) }));
									op(	Action::PerformOperation,
										seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x01002:	// MOVE.bw (xxx).W, (An)
								case 0x01003:	// MOVE.bw (xxx).W, (An)+
									op(	int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr", { ea(0) }, !is_byte_access));
									op(	Action::PerformOperation,
										seq("nw np", { a(destination_register) }, !is_byte_access));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x11102:	// MOVE.l (xxx).l, (An)
								case 0x11103:	// MOVE.l (xxx).l, (An)+
									op(	int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("np") );
									op(	int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nR+ nr", { ea(0), ea(0) }));
									op(	Action::PerformOperation,
										seq("nW+ nw np", { ea(1), ea(1) }));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x01102:	// MOVE.bw (xxx).l, (An)
								case 0x01103:	// MOVE.bw (xxx).l, (An)+
									op(	Action::None, seq("np"));
									op(	int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask,
										seq("np nr", { ea(0) }, !is_byte_access));
									op(	Action::PerformOperation,
										seq("nw np", { a(destination_register) }, !is_byte_access));
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x11402:	// MOVE.l #, (An)
								case 0x11403:	// MOVE.l #, (An)+
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op( int(Action::CopyToEffectiveAddress) | MicroOp::DestinationMask, seq("np") );
									op(	Action::PerformOperation, seq("np nW+ nw np", { ea(1), ea(1) }) );
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

								case 0x01402:	// MOVE.bw #, (An)
								case 0x01403:	// MOVE.bw #, (An)+
									storage_.instructions[instruction].source = &storage_.prefetch_queue_;
									op(Action::PerformOperation, seq("np nw np", { a(destination_register) }, !is_byte_access) );
									if(destination_mode == 0x03) {
										op(increment_action | MicroOp::DestinationMask);
									}
								break;

							//
							// MOVE <ea>, -(An)
							//

								case 0x0004:	// MOVE Dn, -(An)
									op(	decrement_action | MicroOp::DestinationMask,
										seq("np nw", { a(destination_register) }, !is_byte_access));
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw);
								break;

								case 0x0204:	// MOVE (An), -(An)
								case 0x0304:	// MOVE (An)+, -(An)
									// nr np nw
								continue;

								case 0x0404:	// MOVE -(An), -(An)
									// n nr np nw
								continue;

								case 0x0504:	// MOVE (d16, An), -(An)
								case 0x0604:	// MOVE (d8, An, Xn), -(An)
									// np nr np nw
									// n np nr np nw
								continue;

								case 0x1004:	// MOVE (xxx).W, -(An)
									// np nr np nw
								continue;

							//
							// MOVE <ea>, (d16, An)
							// MOVE <ea>, (d8, An, Xn)
							// MOVE <ea>, (d16, PC)
							// MOVE <ea>, (d8, PC, Xn)
							//

								case 0x0005:	// MOVE.bw Dn, (d16, An)
								case 0x0006:	// MOVE.bw Dn, (d8, An, Xn)
								case 0x0012:	// MOVE.bw Dn, (d16, PC)
								case 0x0013:	// MOVE.bw Dn, (d8, PC, Xn)
									op(calc_action_for_mode(destination_mode) | MicroOp::DestinationMask, seq(pseq("np", destination_mode)));
									op(Action::PerformOperation, seq("nw np", { ea(1) }));
								break;

								case 0x0205:	// MOVE.bw (An), (d16, An)
								case 0x0305:	// MOVE.bw (An)+, (d16, An)
								case 0x0206:	// MOVE.bw (An), (d8, An, Xn)
								case 0x0306:	// MOVE.bw (An)+, (d8, An, Xn)
								case 0x0212:	// MOVE.bw (An), (d16, PC)
								case 0x0312:	// MOVE.bw (An)+, (d16, PC)
								case 0x0213:	// MOVE.bw (An), (d8, PC, Xn)
								case 0x0313:	// MOVE.bw (An)+, (d8, PC, Xn)
									op(calc_action_for_mode(destination_mode) | MicroOp::DestinationMask, seq("nr", { &storage_.address_[ea_register].full }, !is_byte_access));
									op(Action::PerformOperation, seq(pseq("np nw np", destination_mode), { ea(1) }, !is_byte_access));
									if(ea_mode == 0x03) {
										op(increment_action | MicroOp::SourceMask);
									}
								break;

								case 0x0405:	// MOVE -(An), (d16, An)
									// n nr np nw
								continue;

								case 0x0406:	// MOVE -(An), (d8, An, Xn)
									// n nr n np nw np
								continue;

								case 0x0505:	// MOVE (d16, An), (d16, An)
								case 0x0605:	// MOVE (d8, An, Xn), (d16, An)
									// np nr np nw np
									// n np nr np nw np
								continue;

								case 0x0506:	// MOVE (d16, An), (d8, An, Xn)
								case 0x0606:	// MOVE (d8, An, Xn), (d8, An, Xn)
									// np nr n np nw np
									// n np nr n np nw np
								continue;

								case 0x1005:	// MOVE (xxx).W, (d16, An)
									// np nr np nw np
								continue;

								case 0x1006:	// MOVE (xxx).W, (d8, An, Xn)
									// np nr n np nw np
								continue;

								case 0x1405:	// MOVE.bw #, (d16, An)
								case 0x1406:	// MOVE.bw #, (d8, An, Xn)
								case 0x1412:	// MOVE.bw #, (d16, PC)
								case 0x1413:	// MOVE.bw #, (d8, PC, Xn)
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask, seq("np"));
									op(calc_action_for_mode(destination_mode) | MicroOp::DestinationMask, seq(pseq("np nw np", destination_mode), { ea(1) }, !is_byte_access ));
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsl);
								break;

							//
							// MOVE <ea>, (xxx).W
							//

								case 0x0010:	// MOVE Dn, (xxx).W
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np"));
									op(Action::PerformOperation, seq("nw np", { ea(1) }));
								break;

								case 0x0210:	// MOVE (An), (xxx).W
								case 0x0310:	// MOVE (An)+, (xxx).W
									// nr np nw np
								continue;

								case 0x0410:	// MOVE -(An), (xxx).W
									// n nr np nw np
								continue;

								case 0x0510:	// MOVE (d16, An), (xxx).W
								case 0x0610:	// MOVE (d8, An, Xn), (xxx).W
									// np nr np nw np
									// n np nr np nw np
								continue;

								case 0x11410:	// MOVE.l #, (xxx).w
									op(int(Action::None), seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nW+ nw np", { ea(1), ea(1) }));
									op(Action::SetMoveFlagsl);
								break;

								case 0x1010:	// MOVE (xxx).W, (xxx).W
									// np nr np nw np
								continue;

								case 0x01410:	// MOVE.bw #, (xxx).w
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::DestinationMask, seq("np"));
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nw np", { ea(1) }, !is_byte_access));
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw);
								break;

							//
							// MOVE <ea>, (xxx).L
							//

								case 0x00011:	// MOVE.bw Dn, (xxx).L
									op(Action::None, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np"));
									op(Action::PerformOperation, seq("nw np", { ea(1) }, !is_byte_access));
								break;

								case 0x00211:	// MOVE.bw (An), (xxx).L
								case 0x00311:	// MOVE.bw (An)+, (xxx).L
									op(Action::None, seq("nr np", { a(ea_register) }, !is_byte_access));
									op(Action::PerformOperation, seq("nw np np", { &storage_.prefetch_queue_.full }));
									if(ea_mode == 0x03) {
										op(int(is_byte_access ? Action::Increment1 : Action::Increment2) | MicroOp::SourceMask);
									}
								break;

								case 0x0411:	// MOVE -(An), (xxx).L
									// n nr np nw np np
								continue;

								case 0x0511:	// MOVE (d16, An), (xxx).L
								case 0x0611:	// MOVE (d8, An, Xn), (xxx).L
									// np nr np nw np np
									// n np nr np nw np np
								continue;

								case 0x01011:	// MOVE.bw (xxx).W, (xxx).L
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nw np np", { ea(1) }, !is_byte_access));
								continue;

								case 0x01111:	// MOVE.bw (xxx).l, (xxx).l
									op(int(Action::None), seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nr", { ea(0) }, !is_byte_access));
									op(Action::PerformOperation, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nw np np", { ea(1) }, !is_byte_access));
								break;

								case 0x01411:	// MOVE.bw #, (xxx).l
									op(int(Action::AssembleWordDataFromPrefetch) | MicroOp::DestinationMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nw np", { ea(1) }));
									op(is_byte_access ? Action::SetMoveFlagsb : Action::SetMoveFlagsw);
								break;

								case 0x11011:	// MOVE.l (xxx).W, (xxx).L
									op(int(Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nW+ nw np np", { ea(1), ea(1) }));
								break;

								case 0x11111:	// MOVE.l (xxx).l, (xxx).l
									op(int(Action::None), seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask, seq("np nR+ nr", { ea(0), ea(0) }));
									op(Action::PerformOperation, seq("np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("nW+ nw np np", { ea(1), ea(1) }));
								break;

								case 0x11411:	// MOVE.l #, (xxx).l
									op(int(Action::None), seq("np"));
									op(int(Action::AssembleLongWordDataFromPrefetch) | MicroOp::DestinationMask, seq("np np"));
									op(int(Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask, seq("np nW+ nw np", { ea(1), ea(1) }));
									op(Action::SetMoveFlagsl);
								break;

							//
							// Default
							//

								default:
									std::cerr << "Unimplemented MOVE " << std::hex << both_modes << " " << instruction << std::endl;
									// TODO: all other types of mode.
								continue;
							}
						} break;

						case Decoder::RESET:
							storage_.instructions[instruction].requires_supervisor = true;
							op(Action::None, seq("nn _ np"));
						break;

						default:
							std::cerr << "Unhandled decoder " << int(mapping.decoder) << std::endl;
						continue;
					}

					// Add a terminating micro operation if necessary.
					if(!storage_.all_micro_ops_.back().is_terminal()) {
						storage_.all_micro_ops_.emplace_back();
					}

					// Ensure that steps that weren't meant to look terminal aren't terminal.
					for(auto index = micro_op_start; index < storage_.all_micro_ops_.size() - 1; ++index) {
						if(storage_.all_micro_ops_[index].is_terminal()) {
							storage_.all_micro_ops_[index].bus_program = seq("");
						}
					}

					// Install the operation and make a note of where micro-ops begin.
					storage_.instructions[instruction].operation = operation;
					micro_op_pointers[instruction] = micro_op_start;

					// Don't search further through the list of possibilities.
					break;
				}
			}
		}

#undef ea
#undef a
#undef seq
#undef op
#undef pseq

		// Finalise micro-op and program pointers.
		for(size_t instruction = 0; instruction < 65536; ++instruction) {
			if(micro_op_pointers[instruction] != std::numeric_limits<size_t>::max()) {
				storage_.instructions[instruction].micro_operations = &storage_.all_micro_ops_[micro_op_pointers[instruction]];

				auto operation = storage_.instructions[instruction].micro_operations;
				while(!operation->is_terminal()) {
					const auto offset = size_t(operation->bus_program - &arbitrary_base);
					assert(offset >= 0 &&  offset < storage_.all_bus_steps_.size());
					operation->bus_program = &storage_.all_bus_steps_[offset];
					++operation;
				}
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

	// Create the special programs.
	const size_t reset_offset = constructor.assemble_program("n n n n n nn nF nf nV nv np np");

	const size_t branch_taken_offset = constructor.assemble_program("n np np");
	const size_t branch_byte_not_taken_offset = constructor.assemble_program("nn np");
	const size_t branch_word_not_taken_offset = constructor.assemble_program("nn np np");

	const size_t dbcc_condition_true_offset = constructor.assemble_program("nn np np");
	const size_t dbcc_condition_false_no_branch_offset = constructor.assemble_program("n nr np np", { &dbcc_false_address_ });
	const size_t dbcc_condition_false_branch_offset = constructor.assemble_program("n np np");

	// Install operations.
	constructor.install_instructions();

	// Realise the special programs as direct pointers.
	reset_bus_steps_ = &all_bus_steps_[reset_offset];

	branch_taken_bus_steps_ = &all_bus_steps_[branch_taken_offset];
	branch_byte_not_taken_bus_steps_ = &all_bus_steps_[branch_byte_not_taken_offset];
	branch_word_not_taken_bus_steps_ = &all_bus_steps_[branch_word_not_taken_offset];

	dbcc_condition_true_steps_ = &all_bus_steps_[dbcc_condition_true_offset];
	dbcc_condition_false_no_branch_steps_ = &all_bus_steps_[dbcc_condition_false_no_branch_offset];
	dbcc_condition_false_branch_steps_ = &all_bus_steps_[dbcc_condition_false_branch_offset];

	// Set initial state. Largely TODO.
	active_step_ = reset_bus_steps_;
	effective_address_[0] = 0;
	is_supervisor_ = 1;
}

void CPU::MC68000::ProcessorStorage::write_back_stack_pointer() {
	stack_pointers_[is_supervisor_] = address_[7];
}

void CPU::MC68000::ProcessorStorage::set_is_supervisor(bool is_supervisor) {
	const int new_is_supervisor = is_supervisor ? 1 : 0;
	if(new_is_supervisor != is_supervisor_) {
		stack_pointers_[is_supervisor_] = address_[7];
		is_supervisor_ = new_is_supervisor;
		address_[7] = stack_pointers_[is_supervisor_];
	}
}
