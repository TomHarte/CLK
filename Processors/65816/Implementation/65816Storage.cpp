//
//  65816Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "../65816.hpp"

#include <cassert>
#include <functional>
#include <map>

using namespace CPU::WDC65816;

struct CPU::WDC65816::ProcessorStorageConstructor {
	// Establish that a storage constructor needs access to ProcessorStorage.
	ProcessorStorage &storage_;
	ProcessorStorageConstructor(ProcessorStorage &storage) : storage_(storage) {}

	enum class AccessType {
		Read, Write
	};

	/// Divides memory-accessing instructions by whether they read or write.
	/// Read-modify-writes are documented with completely distinct bus programs,
	/// so there's no real ambiguity there.
	constexpr static AccessType access_type_for_operation(Operation operation) {
		switch(operation) {
			case ADC:	case AND:	case BIT:	case CMP:
			case CPX:	case CPY:	case EOR:	case ORA:
			case SBC:

			case LDA:	case LDX:	case LDY:

			// The access type for everything else is arbitrary; they're
			// not relevantly either read or write.
			default:
			return AccessType::Read;

			case STA:	case STX:	case STY:	case STZ:
			return AccessType::Write;
		}
	}

	/// Indicates which of the memory-accessing instructions take their cue from the current
	/// size of the index registers, rather than 'memory'[/accumulator].
	constexpr static bool operation_is_index_sized(Operation operation) {
		switch(operation) {
			case CPX:	case CPY:
			case LDX:	case LDY:
			case STX:	case STY:
			return true;

			default:
			return false;
		}
		return false;
	}

	typedef void (* Generator)(AccessType, bool, const std::function<void(MicroOp)>&);
	using GeneratorKey = std::tuple<AccessType, Generator>;
	using PatternTable = std::map<GeneratorKey, std::pair<size_t, size_t>>;
	PatternTable installed_patterns;

	int opcode = 0;
	enum class AccessMode {
		Mixed,
		Always8Bit,
		Always16Bit
	};

	void install(Generator generator, Operation operation, AccessMode access_mode = AccessMode::Mixed) {
		// Determine the access type implied by this operation.
		const AccessType access_type = access_type_for_operation(operation);

		// Install the bus pattern.
		const auto map_entry = install(generator, access_type);
		const size_t micro_op_location_8 = map_entry->second.first;
		const size_t micro_op_location_16 = map_entry->second.second;

		// Fill in the proper table entries and increment the opcode pointer.
		storage_.instructions[opcode].program_offsets[0] = (access_mode == AccessMode::Always8Bit) ? uint16_t(micro_op_location_8) : uint16_t(micro_op_location_16);
		storage_.instructions[opcode].program_offsets[1] = (access_mode == AccessMode::Always16Bit) ? uint16_t(micro_op_location_16) : uint16_t(micro_op_location_8);
		storage_.instructions[opcode].operation = operation;
		storage_.instructions[opcode].size_field = operation_is_index_sized(operation);

		++opcode;
	}

	void set_exception_generator(Generator generator, Generator reset_tail_generator) {
		const auto map_entry = install(generator);
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::Exception)].program_offsets[0] =
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::Exception)].program_offsets[1] = uint16_t(map_entry->second.first);
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::Exception)].operation = JMPind;

		const auto reset_tail_entry = install(reset_tail_generator);
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::ResetTail)].program_offsets[0] =
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::ResetTail)].program_offsets[1] = uint16_t(reset_tail_entry->second.first);
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::ResetTail)].operation = JMPind;
	}

	void install_fetch_decode_execute() {
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::FetchDecodeExecute)].program_offsets[0] =
		storage_.instructions[size_t(ProcessorStorage::OperationSlot::FetchDecodeExecute)].program_offsets[1] = uint16_t(storage_.micro_ops_.size());
		storage_.micro_ops_.push_back(CycleFetchOpcode);
		storage_.micro_ops_.push_back(OperationDecode);
	}

	private:

	PatternTable::iterator install(Generator generator, AccessType access_type = AccessType::Read) {
		// Check whether this access type + addressing mode generator has already been generated.
		const auto key = std::make_pair(access_type, generator);
		const auto map_entry = installed_patterns.find(key);

		// If it wasn't found, generate it now in both 8- and 16-bit variants.
		// Otherwise, get the location of the existing entries.
		if(map_entry != installed_patterns.end()) {
			return map_entry;
		}

		// Generate 8-bit steps.
		const size_t micro_op_location_8 = storage_.micro_ops_.size();
		(*generator)(access_type, true, [this] (MicroOp op) {
			this->storage_.micro_ops_.push_back(op);
		});
		storage_.micro_ops_.push_back(OperationMoveToNextProgram);

		// Generate 16-bit steps.
		size_t micro_op_location_16 = storage_.micro_ops_.size();
		(*generator)(access_type, false, [this] (MicroOp op) {
			this->storage_.micro_ops_.push_back(op);
		});
		storage_.micro_ops_.push_back(OperationMoveToNextProgram);

		// Minor optimisation: elide the steps if 8- and 16-bit steps are equal.
		bool are_equal = true;
		size_t c = 0;
		while(true) {
			if(storage_.micro_ops_[micro_op_location_8 + c] != storage_.micro_ops_[micro_op_location_16 + c]) {
				are_equal = false;
				break;
			}
			if(storage_.micro_ops_[micro_op_location_8 + c] == OperationMoveToNextProgram) break;
			++c;
		}

		if(are_equal) {
			storage_.micro_ops_.resize(micro_op_location_16);
			micro_op_location_16 = micro_op_location_8;
		}

		// Insert into the map.
		auto [iterator, _] = installed_patterns.insert(std::make_pair(key, std::make_pair(micro_op_location_8, micro_op_location_16)));
		return iterator;
	}

	public:

	/*
		Code below is structured to ease translation from Table 5-7 of the 2018
		edition of the WDC 65816 datasheet.

		In each case the relevant addressing mode is described here via a generator
		function that will spit out the correct MicroOps based on access type
		(i.e. read, write or read-modify-write) and data size (8- or 16-bit).

		That leads up to being able to declare the opcode map by addressing mode
		and operation alone.

		Things the generators can assume before they start:

			1) the opcode has already been fetched and decoded, and the program counter incremented;
			2) the data buffer is empty; and
			3) the data address is undefined.
	*/

	// Performs the closing 8- or 16-bit read or write common to many modes below.
	static void read_write(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		if(type == AccessType::Write) {
			target(OperationPerform);						// Perform operation to fill the data buffer.
			if(!is8bit) target(CycleStoreIncrementData);	// Data low.
			target(CycleStoreData);							// Data [high].
		} else {
			if(!is8bit) target(CycleFetchIncrementData);	// Data low.
			target(CycleFetchData);							// Data [high].
			target(OperationPerform);						// Perform operation from the data buffer.
		}
	}

	static void read_modify_write(bool is8bit, const std::function<void(MicroOp)> &target) {
		target(OperationSetMemoryLock);					// Set the memory lock output until the end of this instruction.

		if(!is8bit)	target(CycleFetchIncrementData);	// Data low.
		target(CycleFetchData);							// Data [high].

		if(!is8bit)	target(CycleFetchDataThrowaway);	// 16-bit: reread final byte of data.
		else target(CycleStoreDataThrowaway);			// 8-bit rewrite final byte of data.

		target(OperationPerform);						// Perform operation within the data buffer.

		if(!is8bit)	target(CycleStoreDecrementData);	// Data high.
		target(CycleStoreData);							// Data [low].
	}

	// 1a. Absolute; a.
	static void absolute(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// AAL.
		target(CycleFetchIncrementPC);					// AAH.
		target(OperationConstructAbsolute);				// Calculate data address.

		read_write(type, is8bit, target);
	}

	// 1b. Absolute; a, JMP.
	static void absolute_jmp(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// New PCL.
		target(CycleFetchPC);					// New PCH.
		target(OperationPerform);				// [JMP]
	}

	// 1c. Absolute; a, JSR.
	static void absolute_jsr(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// New PCL.
		target(CycleFetchPC);					// New PCH.
		target(CycleFetchPCThrowaway);			// IO.
		target(OperationPerform);				// [JSR].
		target(CyclePush);						// PCH.
		target(CyclePush);						// PCL.
	}

	// 1d. Absolute; a, read-modify-write.
	static void absolute_rmw(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// AAL.
		target(CycleFetchIncrementPC);			// AAH.
		target(OperationConstructAbsolute);		// Calculate data address.

		read_modify_write(is8bit, target);
	}

	// 2a. Absolute Indexed Indirect; (a, x), JMP.
	static void absolute_indexed_indirect_jmp(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);						// AAL.
		target(CycleFetchPC);								// AAH.
		target(CycleFetchPCThrowaway);						// IO.
		target(OperationConstructAbsoluteIndexedIndirect);	// Calculate data address.
		target(CycleFetchIncrementData);					// New PCL
		target(CycleFetchData);								// New PCH.
		target(OperationPerform);							// [JMP]
	}

	// 2b. Absolute Indexed Indirect; (a, x), JSR.
	static void absolute_indexed_indirect_jsr(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);						// AAL.

		target(OperationCopyPCToData);						// Prepare to push.
		target(CyclePush);									// PCH.
		target(CyclePush);									// PCL.

		target(CycleFetchPC);								// AAH.
		target(CycleFetchPCThrowaway);						// IO.

		target(OperationConstructAbsoluteIndexedIndirect);	// Calculate data address.
		target(CycleFetchIncrementData);					// New PCL
		target(CycleFetchData);								// New PCH.
		target(OperationPerform);							// ['JSR' (actually: JMPind will do)]
	}

	// 3a. Absolute Indirect; (a), JML.
	static void absolute_indirect_jml(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// New AAL.
		target(CycleFetchPC);					// New AAH.

		target(OperationConstructAbsolute16);	// Calculate data address.
		target(CycleFetchIncrementData);		// New PCL.
		target(CycleFetchIncrementData);		// New PCH.
		target(CycleFetchData);					// New PBR.

		target(OperationPerform);				// [JML]
	}

	// 3b. Absolute Indirect; (a), JMP.
	static void absolute_indirect_jmp(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// New AAL.
		target(CycleFetchPC);					// New AAH.

		target(OperationConstructAbsolute16);	// Calculate data address.
		target(CycleFetchIncrementData);		// New PCL.
		target(CycleFetchData);					// New PCH.

		target(OperationPerform);				// [JMP]
	}

	// 4a. Absolute long; al.
	static void absolute_long(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);				// AAL.
		target(CycleFetchIncrementPC);				// AAH.
		target(CycleFetchIncrementPC);				// AAB.

		target(OperationConstructAbsoluteLong);		// Calculate data address.

		read_write(type, is8bit, target);
	}

	// 4b. Absolute long; al, JMP.
	static void absolute_long_jmp(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);				// New PCL.
		target(CycleFetchIncrementPC);				// New PCH.
		target(CycleFetchPC);						// New PBR.

		target(OperationPerform);					// ['JMP' (though it's JML in internal terms)]
	}

	// 4c. Absolute long al, JSL.
	static void absolute_long_jsl(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// New PCL.
		target(CycleFetchIncrementPC);			// New PCH.

		target(OperationCopyPBRToData);			// Copy PBR to the data register.
		target(CyclePush);						// PBR.
		target(CycleAccessStack);				// IO.

		target(CycleFetchPC);					// New PBR.

		target(OperationConstructAbsolute);		// Calculate data address.
		target(OperationPerform);				// [JSL]

		target(CyclePush);						// PCH.
		target(CyclePush);						// PCL.
	}

	// 5. Absolute long, X;	al, x.
	static void absolute_long_x(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// AAL.
		target(CycleFetchIncrementPC);			// AAH.
		target(CycleFetchIncrementPC);			// AAB.

		target(OperationConstructAbsoluteLongX);		// Calculate data address.

		read_write(type, is8bit, target);
	}

	// 6a. Absolute, X;	a, x.
	static void absolute_x(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// AAL.
		target(CycleFetchIncrementPC);			// AAH.

		if(type == AccessType::Read) {
			target(OperationConstructAbsoluteXRead);	// Calculate data address, potentially skipping the next fetch.
		} else {
			target(OperationConstructAbsoluteX);		// Calculate data address.
		}
		target(CycleFetchIncorrectDataAddress);	// Do the dummy read if necessary; OperationConstructAbsoluteXRead
												// will skip this if it isn't required.

		read_write(type, is8bit, target);
	}

	// 6b. Absolute, X;	a, x, read-modify-write.
	static void absolute_x_rmw(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// AAL.
		target(CycleFetchIncrementPC);					// AAH.

		target(OperationConstructAbsoluteX);			// Calculate data address.
		target(CycleFetchIncorrectDataAddress);			// Perform dummy read.

		read_modify_write(is8bit, target);
	}

	// 7. Absolute, Y;	a, y.
	static void absolute_y(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);			// AAL.
		target(CycleFetchIncrementPC);			// AAH.

		if(type == AccessType::Read) {
			target(OperationConstructAbsoluteYRead);	// Calculate data address, potentially skipping the next fetch.
		} else {
			target(OperationConstructAbsoluteY);		// Calculate data address.
		}
		target(CycleFetchIncorrectDataAddress);	// Do the dummy read if necessary; OperationConstructAbsoluteYRead
												// will skip this if it isn't required.

		read_write(type, is8bit, target);
	}

	// 8. Accumulator; A.
	static void accumulator(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);			// IO.

		// TODO: seriously consider a-specific versions of all relevant operations;
		// the cost of interpreting three things here is kind of silly.
		target(OperationCopyAToData);
		target(OperationPerform);
		target(OperationCopyDataToA);
	}

	// 9a. Block Move Negative [and]
	// 9b. Block Move Positive.
	//
	// These don't fit the general model very well at all, hence the specialised fetch and store cycles.
	static void block_move(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);	// DBA.
		target(CycleFetchIncrementPC);	// SBA.

		target(CycleFetchBlockX);		// SRC Data.
		target(CycleStoreBlockY);		// Dest Data.

		target(CycleFetchBlockY);		// IO.
		target(CycleFetchBlockY);		// IO.

		target(OperationPerform);		// [MVN or MVP]
	}

	// 10a. Direct; d.
	// (That's zero page in 6502 terms)
	static void direct(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);		// IO.

		read_write(type, is8bit, target);
	}

	// 10b. Direct; d, read-modify-write.
	// (That's zero page in 6502 terms)
	static void direct_rmw(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);					// IO.

		read_modify_write(is8bit, target);
	}

	// 11. Direct Indexed Indirect; (d, x).
	static void direct_indexed_indirect(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirectIndexedIndirect);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchIncrementData);				// AAL.
		target(CycleFetchData);							// AAH.

		target(OperationCopyDataToInstruction);
		target(OperationConstructAbsolute);

		read_write(type, is8bit, target);
	}

	// 12. Direct Indirect; (d).
	static void direct_indirect(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchIncrementData);				// AAL.
		target(CycleFetchData);							// AAH.

		target(OperationConstructDirectIndirect);

		read_write(type, is8bit, target);
	}

	// 13. Direct Indirect Indexed; (d), y.
	static void direct_indirect_indexed(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchIncrementData);				// AAL.
		target(CycleFetchData);							// AAH.

		target(OperationCopyDataToInstruction);
		target(OperationConstructAbsoluteYRead);
		target(CycleFetchIncorrectDataAddress);			// IO.

		read_write(type, is8bit, target);
	}

	// 14. Direct Indirect Indexed Long; [d], y.
	static void direct_indirect_indexed_long(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchIncrementData);				// AAL.
		target(CycleFetchIncrementData);				// AAH.
		target(CycleFetchData);							// AAB.

		target(OperationConstructDirectIndirectIndexedLong);

		read_write(type, is8bit, target);
	}

	// 15. Direct Indirect Long; [d].
	static void direct_indirect_long(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirectLong);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchIncrementData);				// AAL.
		target(CycleFetchIncrementData);				// AAH.
		target(CycleFetchData);							// AAB.

		target(OperationConstructDirectIndirectLong);

		read_write(type, is8bit, target);
	}

	// 16a. Direct, X; d, x.
	static void direct_x(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirectX);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchPCThrowaway);					// IO.

		read_write(type, is8bit, target);
	}

	// 16b. Direct, X; d, x, read-modify-write.
	static void direct_x_rmw(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirectX);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchPCThrowaway);					// IO.

		read_modify_write(is8bit, target);
	}

	// 17. Direct, Y; d, y.
	static void direct_y(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);					// DO.

		target(OperationConstructDirectY);
		target(CycleFetchPCThrowaway);					// IO.

		target(CycleFetchPCThrowaway);					// IO.

		read_write(type, is8bit, target);
	}

	// 18. Immediate; #.
	static void immediate(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		if(!is8bit) target(CycleFetchIncrementPC);	// IDL.
		target(CycleFetchIncrementPC);				// ID [H].
		target(OperationCopyInstructionToData);
		target(OperationPerform);
	}

	static void immediate_rep_sep(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);				// IDL.
		target(CycleFetchPCThrowaway);				// "Add 1 cycle for REP and SEP"
		target(OperationPerform);
	}

	// 19a. Implied; i.
	static void implied(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);		// IO.
		target(OperationPerform);
	}

	// 19b. Implied; i; XBA.
	static void implied_xba(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);		// IO.
		target(CycleFetchPCThrowaway);		// IO.
		target(OperationPerform);
	}

	// 19c. Stop the Clock; also
	// 19d. Wait for interrupt.
	static void stp_wai(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(OperationPerform);			// Establishes the termination condition.
		target(CycleFetchPCThrowaway);		// IO.
		target(CycleFetchPCThrowaway);		// IO.
		target(CycleRepeatingNone);			// This will first check whether the STP/WAI exit
											// condition has occurred; if not then it'll issue
											// either a BusOperation::None or ::Ready and then
											// reschedule itself.
	}

	// 20. Relative; r.
	static void relative(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);	// Offset.

		target(OperationPerform);		// The branch instructions will all skip one or three
										// of the next cycles, depending on the effect of
										// the jump. It'll also calculate the correct target
										// address, placing it into the data buffer.

		target(CycleFetchPCThrowaway);	// IO.
		target(CycleFetchPCThrowaway);	// IO.

		target(OperationCopyDataToPC);	// Install the address that was calculated above.
	}

	// 21. Relative long; rl.
	static void relative_long(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// Offset low.
		target(CycleFetchIncrementPC);		// Offset high.
		target(CycleFetchPCThrowaway);		// IO.

		target(OperationPerform);			// [BRL]
	}

	// 22a. Stack; s, abort/irq/nmi/res.
	static void stack_exception(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);		// IO.
		target(CycleFetchPCThrowaway);		// IO.

		target(OperationPrepareException);	// Populates the data buffer; if the exception is a
											// reset then switches to the reset tail program.
											// Otherwise skips a micro-op if in emulation mode.

		target(CyclePush);					// PBR	[skipped in emulation mode].
		target(CyclePush);					// PCH.
		target(CyclePush);					// PCL.
		target(CyclePush);					// P.

		target(CycleFetchIncrementVector);	// AAVL.
		target(CycleFetchVector);			// AAVH.

		target(OperationPerform);			// Jumps to the vector address.
	}

	static void reset_tail(AccessType, bool, const std::function<void(MicroOp)> &target) {
		// The reset program still walks through three stack accesses as if it were doing
		// the usual exception stack activity, but forces them to reads that don't modify
		// the stack pointer. Here they are:
		target(CycleAccessStack);			// PCH.
		target(CycleAccessStack);			// PCL.
		target(CycleAccessStack);			// P.

		target(CycleFetchIncrementVector);	// AAVL.
		target(CycleFetchVector);			// AAVH.

		target(OperationPerform);			// Jumps to the vector address.
	}

	// 22b. Stack; s, PLx.
	static void stack_pull(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);	// IO.
		target(CycleFetchPCThrowaway);	// IO.

		if(!is8bit) target(CyclePull);	// REG low.
		target(CyclePull);				// REG [high].

		target(OperationPerform);
	}

	// 22c. Stack; s, PHx.
	static void stack_push(AccessType, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);	// IO.

		target(OperationPerform);

		if(!is8bit) target(CyclePush);	// REG high.
		target(CyclePush);				// REG [low].
	}

	// 22d. Stack; s, PEA.
	static void stack_pea(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);	// AAL.
		target(CycleFetchIncrementPC);	// AAH.

		target(OperationCopyInstructionToData);

		target(CyclePush);				// AAH.
		target(CyclePush);				// AAL.
	}

	// 22e. Stack; s, PEI.
	static void stack_pei(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// DO.

		target(OperationConstructDirect);
		target(CycleFetchPCThrowaway);		// IO.

		target(CycleFetchIncrementData);	// AAL.
		target(CycleFetchData);				// AAH.
		target(CyclePush);					// AAH.
		target(CyclePush);					// AAL.
	}

	// 22f. Stack; s, PER.
	static void stack_per(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// Offset low.
		target(CycleFetchIncrementPC);		// Offset high.
		target(CycleFetchPCThrowaway);		// IO.

		target(OperationConstructPER);

		target(CyclePush);					// AAH.
		target(CyclePush);					// AAL.
	}

	// 22g. Stack; s, RTI.
	static void stack_rti(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);		// IO.
		target(CycleFetchPCThrowaway);		// IO.

		target(CyclePull);					// P.
		target(CyclePull);					// New PCL.
		target(CyclePull);					// New PCH.
		target(CyclePullIfNotEmulation);	// PBR.

		target(OperationPerform);			// [RTI] — to unpack the fields above.
	}

	// 22h. Stack; s, RTS.
	static void stack_rts(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchPCThrowaway);	// IO.
		target(CycleFetchPCThrowaway);	// IO.

		target(CyclePull);				// PCL.
		target(CyclePull);				// PCH.
		target(CycleAccessStack);		// IO.

		target(OperationPerform);		// [RTS]
	}

	// 22i. Stack; s, RTL.
	static void stack_rtl(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);	// IO.
		target(CycleFetchIncrementPC);	// IO.

		target(CyclePull);				// New PCL.
		target(CyclePull);				// New PCH.
		target(CyclePull);				// New PBR.

		target(OperationPerform);		// [RTL]
	}

	// 22j. Stack; s, BRK/COP.
	static void brk_cop(AccessType, bool, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// Signature.

		target(OperationPrepareException);	// Populates the data buffer; this skips a micro-op if
											// in emulation mode.

		target(CyclePush);					// PBR	[skipped in emulation mode].
		target(CyclePush);					// PCH.
		target(CyclePush);					// PCL.
		target(CyclePush);					// P.

		target(CycleFetchIncrementVector);	// AAVL.
		target(CycleFetchVector);			// AAVH.

		target(OperationPerform);			// Jumps to the vector address.
	}

	// 23. Stack Relative; d, s.
	static void stack_relative(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);	// SO.
		target(CycleFetchPCThrowaway);	// IO.

		target(OperationConstructStackRelative);
		read_write(type, is8bit, target);
	}

	// 24. Stack Relative Indirect Indexed (d, s), y.
	static void stack_relative_indexed_indirect(AccessType type, bool is8bit, const std::function<void(MicroOp)> &target) {
		target(CycleFetchIncrementPC);		// SO.
		target(CycleFetchPCThrowaway);		// IO.

		target(OperationConstructStackRelative);
		target(CycleFetchIncrementData);	// AAL.
		target(CycleFetchData);				// AAH.
		target(CycleFetchDataThrowaway);	// IO.

		target(OperationConstructStackRelativeIndexedIndirect);
		read_write(type, is8bit, target);
	}
};

ProcessorStorage::ProcessorStorage() {
	set_reset_state();
	micro_ops_.reserve(1024);

	ProcessorStorageConstructor constructor(*this);
	using AccessMode = ProcessorStorageConstructor::AccessMode;

	// Install the instructions.
#define op(x, ...) constructor.install(&ProcessorStorageConstructor::x, __VA_ARGS__)

	/* 0x00 BRK s */			op(brk_cop, JMPind);
	/* 0x01 ORA (d, x) */		op(direct_indexed_indirect, ORA);
	/* 0x02 COP s */			op(brk_cop, JMPind);
	/* 0x03 ORA d, s */			op(stack_relative, ORA);
	/* 0x04 TSB d */			op(direct_rmw, TSB);
	/* 0x05 ORA d */			op(direct, ORA);
	/* 0x06 ASL d */			op(direct_rmw, ASL);
	/* 0x07 ORA [d] */			op(direct_indirect_long, ORA);
	/* 0x08 PHP s */			op(stack_push, PHP, AccessMode::Always8Bit);
	/* 0x09 ORA # */			op(immediate, ORA);
	/* 0x0a ASL A */			op(accumulator, ASL);
	/* 0x0b PHD s */			op(stack_push, PHD, AccessMode::Always16Bit);
	/* 0x0c TSB a */			op(absolute_rmw, TSB);
	/* 0x0d ORA a */			op(absolute, ORA);
	/* 0x0e ASL a */			op(absolute_rmw, ASL);
	/* 0x0f ORA al */			op(absolute_long, ORA);

	/* 0x10 BPL r */			op(relative, BPL);
	/* 0x11 ORA (d), y */		op(direct_indirect_indexed, ORA);
	/* 0x12 ORA (d) */			op(direct_indirect, ORA);
	/* 0x13 ORA (d, s), y */	op(stack_relative_indexed_indirect, ORA);
	/* 0x14 TRB d */			op(direct_rmw, TRB);
	/* 0x15 ORA d, x */			op(direct_x, ORA);
	/* 0x16 ASL d, x */			op(direct_x_rmw, ASL);
	/* 0x17 ORA [d], y */		op(direct_indirect_indexed_long, ORA);
	/* 0x18 CLC i */			op(implied, CLC);
	/* 0x19 ORA a, y */			op(absolute_y, ORA);
	/* 0x1a INC A */			op(accumulator, INC);
	/* 0x1b TCS i */			op(implied, TCS);
	/* 0x1c TRB a */			op(absolute_rmw, TRB);
	/* 0x1d ORA a, x */			op(absolute_x, ORA);
	/* 0x1e ASL a, x */			op(absolute_x_rmw, ASL);
	/* 0x1f ORA al, x */		op(absolute_long_x, ORA);

	/* 0x20 JSR a */			op(absolute_jsr, JSR);
	/* 0x21 AND (d, x) */		op(direct_indexed_indirect, AND);
	/* 0x22 JSL al */			op(absolute_long_jsl, JSL);
	/* 0x23 AND d, s */			op(stack_relative, AND);
	/* 0x24 BIT d */			op(direct, BIT);
	/* 0x25 AND d */			op(direct, AND);
	/* 0x26 ROL d */			op(direct_rmw, ROL);
	/* 0x27 AND [d] */			op(direct_indirect_long, AND);
	/* 0x28 PLP s */			op(stack_pull, PLP, AccessMode::Always8Bit);
	/* 0x29 AND # */			op(immediate, AND);
	/* 0x2a ROL A */			op(accumulator, ROL);
	/* 0x2b PLD s */			op(stack_pull, PLD, AccessMode::Always16Bit);
	/* 0x2c BIT a */			op(absolute, BIT);
	/* 0x2d AND a */			op(absolute, AND);
	/* 0x2e ROL a */			op(absolute_rmw, ROL);
	/* 0x2f AND al */			op(absolute_long, AND);

	/* 0x30 BMI r */			op(relative, BMI);
	/* 0x31 AND (d), y */		op(direct_indirect_indexed, AND);
	/* 0x32 AND (d) */			op(direct_indirect, AND);
	/* 0x33 AND (d, s), y */	op(stack_relative_indexed_indirect, AND);
	/* 0x34 BIT d, x */			op(direct_x, BIT);
	/* 0x35 AND d, x */			op(direct_x, AND);
	/* 0x36 ROL d, x */			op(direct_x_rmw, ROL);
	/* 0x37 AND [d], y */		op(direct_indirect_indexed_long, AND);
	/* 0x38 SEC i */			op(implied, SEC);
	/* 0x39 AND a, y */			op(absolute_y, AND);
	/* 0x3a DEC A */			op(accumulator, DEC);
	/* 0x3b TSC i */			op(implied, TSC);
	/* 0x3c BIT a, x */			op(absolute_x, BIT);
	/* 0x3d AND a, x */			op(absolute_x, AND);
	/* 0x3e ROL a, x */			op(absolute_x_rmw, ROL);
	/* 0x3f AND al, x */		op(absolute_long_x, AND);

	/* 0x40 RTI s */			op(stack_rti, RTI);
	/* 0x41	EOR (d, x) */		op(direct_indexed_indirect, EOR);
	/* 0x42	WDM i */			op(implied, NOP);
	/* 0x43	EOR d, s */			op(stack_relative, EOR);
	/* 0x44	MVP xyc */			op(block_move, MVP);
	/* 0x45	EOR d */			op(direct, EOR);
	/* 0x46	LSR d */			op(direct_rmw, LSR);
	/* 0x47	EOR [d] */			op(direct_indirect_long, EOR);
	/* 0x48	PHA s */			op(stack_push, STA);
	/* 0x49	EOR # */			op(immediate, EOR);
	/* 0x4a	LSR A */			op(accumulator, LSR);
	/* 0x4b	PHK s */			op(stack_push, PHK, AccessMode::Always8Bit);
	/* 0x4c	JMP a */			op(absolute_jmp, JMP);
	/* 0x4d	EOR a */			op(absolute, EOR);
	/* 0x4e	LSR a */			op(absolute_rmw, LSR);
	/* 0x4f	EOR al */			op(absolute_long, EOR);

	/* 0x50 BVC r */			op(relative, BVC);
	/* 0x51 EOR (d), y */		op(direct_indirect_indexed, EOR);
	/* 0x52 EOR (d) */			op(direct_indirect, EOR);
	/* 0x53 EOR (d, s), y */	op(stack_relative_indexed_indirect, EOR);
	/* 0x54 MVN xyc */			op(block_move, MVN);
	/* 0x55 EOR d, x */			op(direct_x, EOR);
	/* 0x56 LSR d, x */			op(direct_x_rmw, LSR);
	/* 0x57 EOR [d], y */		op(direct_indirect_indexed_long, EOR);
	/* 0x58 CLI i */			op(implied, CLI);
	/* 0x59 EOR a, y */			op(absolute_y, EOR);
	/* 0x5a PHY s */			op(stack_push, STY);
	/* 0x5b TCD i */			op(implied, TCD);
	/* 0x5c JMP al */			op(absolute_long_jmp, JML);	// [sic]; this updates PBR so it's JML.
	/* 0x5d EOR a, x */			op(absolute_x, EOR);
	/* 0x5e LSR a, x */			op(absolute_x_rmw, LSR);
	/* 0x5f EOR al, x */		op(absolute_long_x, EOR);

	/* 0x60 RTS s */			op(stack_rts, RTS);
	/* 0x61 ADC (d, x) */		op(direct_indexed_indirect, ADC);
	/* 0x62 PER s */			op(stack_per, NOP, AccessMode::Always16Bit);
	/* 0x63 ADC d, s */			op(stack_relative, ADC);
	/* 0x64 STZ d */			op(direct, STZ);
	/* 0x65 ADC d */			op(direct, ADC);
	/* 0x66 ROR d */			op(direct_rmw, ROR);
	/* 0x67 ADC [d] */			op(direct_indirect_long, ADC);
	/* 0x68 PLA s */			op(stack_pull, LDA);
	/* 0x69 ADC # */			op(immediate, ADC);
	/* 0x6a ROR A */			op(accumulator, ROR);
	/* 0x6b RTL s */			op(stack_rtl, RTL);
	/* 0x6c JMP (a) */			op(absolute_indirect_jmp, JMPind);
	/* 0x6d ADC a */			op(absolute, ADC);
	/* 0x6e ROR a */			op(absolute_rmw, ROR);
	/* 0x6f ADC al */			op(absolute_long, ADC);

	/* 0x70 BVS r */			op(relative, BVS);
	/* 0x71 ADC (d), y */		op(direct_indirect_indexed, ADC);
	/* 0x72 ADC (d) */			op(direct_indirect, ADC);
	/* 0x73 ADC (d, s), y */	op(stack_relative_indexed_indirect, ADC);
	/* 0x74 STZ d, x */			op(direct_x, STZ);
	/* 0x75 ADC d, x */			op(direct_x, ADC);
	/* 0x76 ROR d, x */			op(direct_x_rmw, ROR);
	/* 0x77 ADC [d], y */		op(direct_indirect_indexed_long, ADC);
	/* 0x78 SEI i */			op(implied, SEI);
	/* 0x79 ADC a, y */			op(absolute_y, ADC);
	/* 0x7a PLY s */			op(stack_pull, LDY);
	/* 0x7b TDC i */			op(implied, TDC);
	/* 0x7c JMP (a, x) */		op(absolute_indexed_indirect_jmp, JMPind);
	/* 0x7d ADC a, x */			op(absolute_x, ADC);
	/* 0x7e ROR a, x */			op(absolute_x_rmw, ROR);
	/* 0x7f ADC al, x */		op(absolute_long_x, ADC);

	/* 0x80 BRA r */			op(relative, BRA);
	/* 0x81 STA (d, x) */		op(direct_indexed_indirect, STA);
	/* 0x82 BRL rl */			op(relative_long, BRL);
	/* 0x83 STA d, s */			op(stack_relative, STA);
	/* 0x84 STY d */			op(direct, STY);
	/* 0x85 STA d */			op(direct, STA);
	/* 0x86 STX d */			op(direct, STX);
	/* 0x87 STA [d] */			op(direct_indirect_long, STA);
	/* 0x88 DEY i */			op(implied, DEY);
	/* 0x89 BIT # */			op(immediate, BITimm);
	/* 0x8a TXA i */			op(implied, TXA);
	/* 0x8b PHB s */			op(stack_push, PHB, AccessMode::Always8Bit);
	/* 0x8c STY a */			op(absolute, STY);
	/* 0x8d STA a */			op(absolute, STA);
	/* 0x8e STX a */			op(absolute, STX);
	/* 0x8f STA al */			op(absolute_long, STA);

	/* 0x90 BCC r */			op(relative, BCC);
	/* 0x91 STA (d), y */		op(direct_indirect_indexed, STA);
	/* 0x92 STA (d) */			op(direct_indirect, STA);
	/* 0x93 STA (d, s), y */	op(stack_relative_indexed_indirect, STA);
	/* 0x94 STY d, x */			op(direct_x, STY);
	/* 0x95 STA d, x */			op(direct_x, STA);
	/* 0x96 STX d, y */			op(direct_y, STX);
	/* 0x97 STA [d], y */		op(direct_indirect_indexed_long, STA);
	/* 0x98 TYA i */			op(implied, TYA);
	/* 0x99 STA a, y */			op(absolute_y, STA);
	/* 0x9a TXS i */			op(implied, TXS);
	/* 0x9b TXY i */			op(implied, TXY);
	/* 0x9c STZ a */			op(absolute, STZ);
	/* 0x9d STA a, x */			op(absolute_x, STA);
	/* 0x9e STZ a, x */			op(absolute_x, STZ);
	/* 0x9f STA al, x */		op(absolute_long_x, STA);

	/* 0xa0 LDY # */			op(immediate, LDY);
	/* 0xa1 LDA (d, x) */		op(direct_indexed_indirect, LDA);
	/* 0xa2 LDX # */			op(immediate, LDX);
	/* 0xa3 LDA d, s */			op(stack_relative, LDA);
	/* 0xa4 LDY d */			op(direct, LDY);
	/* 0xa5 LDA d */			op(direct, LDA);
	/* 0xa6 LDX d */			op(direct, LDX);
	/* 0xa7 LDA [d] */			op(direct_indirect_long, LDA);
	/* 0xa8 TAY i */			op(implied, TAY);
	/* 0xa9 LDA # */			op(immediate, LDA);
	/* 0xaa TAX i */			op(implied, TAX);
	/* 0xab PLB s */			op(stack_pull, PLB, AccessMode::Always8Bit);
	/* 0xac LDY a */			op(absolute, LDY);
	/* 0xad LDA a */			op(absolute, LDA);
	/* 0xae LDX a */			op(absolute, LDX);
	/* 0xaf LDA al */			op(absolute_long, LDA);

	/* 0xb0 BCS r */			op(relative, BCS);
	/* 0xb1 LDA (d), y */		op(direct_indirect_indexed, LDA);
	/* 0xb2 LDA (d) */			op(direct_indirect, LDA);
	/* 0xb3 LDA (d, s), y */	op(stack_relative_indexed_indirect, LDA);
	/* 0xb4 LDY d, x */			op(direct_x, LDY);
	/* 0xb5 LDA d, x */			op(direct_x, LDA);
	/* 0xb6 LDX d, y */			op(direct_y, LDX);
	/* 0xb7 LDA [d], y */		op(direct_indirect_indexed_long, LDA);
	/* 0xb8 CLV i */			op(implied, CLV);
	/* 0xb9 LDA a, y */			op(absolute_y, LDA);
	/* 0xba TSX i */			op(implied, TSX);
	/* 0xbb TYX i */			op(implied, TYX);
	/* 0xbc LDY a, x */			op(absolute_x, LDY);
	/* 0xbd LDA a, x */			op(absolute_x, LDA);
	/* 0xbe LDX a, y */			op(absolute_y, LDX);
	/* 0xbf LDA al, x */		op(absolute_long_x, LDA);

	/* 0xc0 CPY # */			op(immediate, CPY);
	/* 0xc1 CMP (d, x) */		op(direct_indexed_indirect, CMP);
	/* 0xc2 REP # */			op(immediate_rep_sep, REP);
	/* 0xc3 CMP d, s */			op(stack_relative, CMP);
	/* 0xc4 CPY d */			op(direct, CPY);
	/* 0xc5 CMP d */			op(direct, CMP);
	/* 0xc6 DEC d */			op(direct_rmw, DEC);
	/* 0xc7 CMP [d] */			op(direct_indirect_long, CMP);
	/* 0xc8 INY i */			op(implied, INY);
	/* 0xc9 CMP # */			op(immediate, CMP);
	/* 0xca DEX i */			op(implied, DEX);
	/* 0xcb WAI i */			op(stp_wai, WAI);
	/* 0xcc CPY a */			op(absolute, CPY);
	/* 0xcd CMP a */			op(absolute, CMP);
	/* 0xce DEC a */			op(absolute_rmw, DEC);
	/* 0xcf CMP al */			op(absolute_long, CMP);

	/* 0xd0 BNE r */			op(relative, BNE);
	/* 0xd1 CMP (d), y */		op(direct_indirect_indexed, CMP);
	/* 0xd2 CMP (d) */			op(direct_indirect, CMP);
	/* 0xd3 CMP (d, s), y */	op(stack_relative_indexed_indirect, CMP);
	/* 0xd4 PEI s */			op(stack_pei, NOP, AccessMode::Always16Bit);
	/* 0xd5 CMP d, x */			op(direct_x, CMP);
	/* 0xd6 DEC d, x */			op(direct_x_rmw, DEC);
	/* 0xd7 CMP [d], y */		op(direct_indirect_indexed_long, CMP);
	/* 0xd8 CLD i */			op(implied, CLD);
	/* 0xd9 CMP a, y */			op(absolute_y, CMP);
	/* 0xda PHX s */			op(stack_push, STX);
	/* 0xdb STP i */			op(stp_wai, STP);
	/* 0xdc JML (a) */			op(absolute_indirect_jml, JML);
	/* 0xdd CMP a, x */			op(absolute_x, CMP);
	/* 0xde DEC a, x */			op(absolute_x_rmw, DEC);
	/* 0xdf CMP al, x */		op(absolute_long_x, CMP);

	/* 0xe0 CPX # */			op(immediate, CPX);
	/* 0xe1 SBC (d, x) */		op(direct_indexed_indirect, SBC);
	/* 0xe2 SEP # */			op(immediate_rep_sep, SEP);
	/* 0xe3 SBC d, s */			op(stack_relative, SBC);
	/* 0xe4 CPX d */			op(direct, CPX);
	/* 0xe5 SBC d */			op(direct, SBC);
	/* 0xe6 INC d */			op(direct_rmw, INC);
	/* 0xe7 SBC [d] */			op(direct_indirect_long, SBC);
	/* 0xe8 INX i */			op(implied, INX);
	/* 0xe9 SBC # */			op(immediate, SBC);
	/* 0xea NOP i */			op(implied, NOP);
	/* 0xeb XBA i */			op(implied_xba, XBA);
	/* 0xec CPX a */			op(absolute, CPX);
	/* 0xed SBC a */			op(absolute, SBC);
	/* 0xee INC a */			op(absolute_rmw, INC);
	/* 0xef SBC al */			op(absolute_long, SBC);

	/* 0xf0 BEQ r */			op(relative, BEQ);
	/* 0xf1 SBC (d), y */		op(direct_indirect_indexed, SBC);
	/* 0xf2 SBC (d) */			op(direct_indirect, SBC);
	/* 0xf3 SBC (d, s), y */	op(stack_relative_indexed_indirect, SBC);
	/* 0xf4 PEA s */			op(stack_pea, NOP, AccessMode::Always16Bit);
	/* 0xf5 SBC d, x */			op(direct_x, SBC);
	/* 0xf6 INC d, x */			op(direct_x_rmw, INC);
	/* 0xf7 SBC [d], y */		op(direct_indirect_indexed_long, SBC);
	/* 0xf8 SED i */			op(implied, SED);
	/* 0xf9 SBC a, y */			op(absolute_y, SBC);
	/* 0xfa PLX s */			op(stack_pull, LDX);
	/* 0xfb XCE i */			op(implied, XCE);
	/* 0xfc JSR (a, x) */		op(absolute_indexed_indirect_jsr, JMPind);	// [sic]
	/* 0xfd SBC a, x */			op(absolute_x, SBC);
	/* 0xfe INC a, x */			op(absolute_x_rmw, INC);
	/* 0xff SBC al, x */		op(absolute_long_x, SBC);
#undef op

	constructor.set_exception_generator(&ProcessorStorageConstructor::stack_exception, &ProcessorStorageConstructor::reset_tail);
	constructor.install_fetch_decode_execute();

	// Find any OperationMoveToNextProgram.
	next_op_ = micro_ops_.data();
	while(*next_op_ != OperationMoveToNextProgram) ++next_op_;

	// This is primarily to keep tabs, in case I want to pick a shorter form for the instruction table.
	assert(micro_ops_.size() < 1024);
}

void ProcessorStorage::set_reset_state() {
	registers_.data_bank = 0;
	registers_.program_bank = 0;
	registers_.direct = 0;
	registers_.flags.decimal = 0;
	registers_.flags.inverse_interrupt = 0;
	set_emulation_mode(true);
}

void ProcessorStorage::set_emulation_mode(bool enabled) {
	if(registers_.emulation_flag == enabled) {
		return;
	}
	registers_.emulation_flag = enabled;

	if(enabled) {
		set_m_x_flags(true, true);
		registers_.x.halves.high = registers_.y.halves.high = 0;
		registers_.e_masks[0] = 0xff00;
		registers_.e_masks[1] = 0x00ff;
	} else {
		registers_.e_masks[0] = 0x0000;
		registers_.e_masks[1] = 0xffff;
		registers_.s.halves.high = 1;	// To pretend it was 1 all along; this implementation actually ignores
										// the top byte while in emulation mode.
	}
}

void ProcessorStorage::set_m_x_flags(bool m, bool x) {
	// true/1 => 8bit for both flags.
	registers_.mx_flags[0] = m;
	registers_.mx_flags[1] = x;

	registers_.m_masks[0] = m ? 0xff00 : 0x0000;
	registers_.m_masks[1] = m ? 0x00ff : 0xffff;
	registers_.m_shift = m ? 0 : 8;

	registers_.x_masks[0] = x ? 0xff00 : 0x0000;
	registers_.x_masks[1] = x ? 0x00ff : 0xffff;
	registers_.x_shift = x ? 0 : 8;
}

uint8_t ProcessorStorage::get_flags() const {
	uint8_t result = registers_.flags.get();

	if(!registers_.emulation_flag) {
		result &= ~(Flag::MemorySize | Flag::IndexSize);
		result |= registers_.mx_flags[0] * Flag::MemorySize;
		result |= registers_.mx_flags[1] * Flag::IndexSize;
	}

	return result;
}

void ProcessorStorage::set_flags(uint8_t value) {
	registers_.flags.set(value);

	if(!registers_.emulation_flag) {
		set_m_x_flags(value & Flag::MemorySize, value & Flag::IndexSize);
	}
}
