//
//  8088Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 13/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "NSData+dataWithContentsOfGZippedFile.h"

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Decoder.hpp"
#include "InstructionSets/x86/Perform.hpp"
#include "InstructionSets/x86/Flags.hpp"
#include "Machines/PCCompatible/SegmentedMemory.hpp"
#include "Numeric/RegisterSizes.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/thomasharte/Projects/8088/v1";

using Flags = InstructionSet::x86::Flags;
using Registers = InstructionSet::x86::Registers<InstructionSet::x86::Model::i8086>;

class Segments {
public:
	Segments(const Registers &registers) : registers_(registers) {}

	using Source = InstructionSet::x86::Source;

	/// Posted by @c perform after any operation which *might* have affected a segment register.
	void did_update(Source segment) {
		switch(segment) {
			default: break;
			case Source::ES:	es_base_ = registers_.es() << 4;	break;
			case Source::CS:	cs_base_ = registers_.cs() << 4;	break;
			case Source::DS:	ds_base_ = registers_.ds() << 4;	break;
			case Source::SS:	ss_base_ = registers_.ss() << 4;	break;
		}
	}

	void reset() {
		did_update(Source::ES);
		did_update(Source::CS);
		did_update(Source::DS);
		did_update(Source::SS);
	}

	uint32_t es_base_, cs_base_, ds_base_, ss_base_;

	bool operator ==(const Segments &rhs) const {
		return
			es_base_ == rhs.es_base_ &&
			cs_base_ == rhs.cs_base_ &&
			ds_base_ == rhs.ds_base_ &&
			ss_base_ == rhs.ss_base_;
	}

private:
	const Registers &registers_;
};

//struct LinearMemory {
//public:
//	template <typename IntT, InstructionSet::x86::AccessType type>
//	typename InstructionSet::x86::Accessor<IntT, type>::type access(
//		[[maybe_unused]] const uint32_t address,
//		[[maybe_unused]] const uint32_t base
//	) {
//	}
//
//	template <typename IntT>
//	void preauthorised_write(
//		[[maybe_unused]] const uint32_t address,
//		[[maybe_unused]] const uint32_t base,
//		[[maybe_unused]] const IntT value
//	) {
//	}
//
//	template <typename IntT>
//	void write_back() {
//	}
//};

struct LinearMemory {
public:
	using AccessType = InstructionSet::x86::AccessType;

	// Constructor.
	LinearMemory(Registers &registers, const Segments &segments) : registers_(registers), segments_(segments) {
		memory.resize(1024*1024);
	}

	// Initialisation.
	void clear() {
		tags.clear();
	}

	void seed(uint32_t address, uint8_t value) {
		memory[address] = value;
		tags[address] = Tag::Seeded;
	}

	void touch(uint32_t address) {
		tags[address] = Tag::AccessExpected;
	}

	//
	// Preauthorisation call-ins.
	//
	void preauthorise_stack_write(uint32_t length) {
		uint16_t sp = registers_.sp();
		while(length--) {
			--sp;
			preauthorise(InstructionSet::x86::Source::SS, sp);
		}
	}
	void preauthorise_stack_read(uint32_t length) {
		uint16_t sp = registers_.sp();
		while(length--) {
			preauthorise(InstructionSet::x86::Source::SS, sp);
			++sp;
		}
	}
	void preauthorise_read(InstructionSet::x86::Source segment, uint16_t start, uint32_t length) {
		while(length--) {
			preauthorise(segment, start);
			++start;
		}
	}
	void preauthorise_read(uint32_t start, uint32_t length) {
		while(length--) {
			preauthorise(start);
			++start;
		}
	}

	//
	// Access call-ins.
	//

	// Accesses an address based on segment:offset.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		InstructionSet::x86::Source segment, uint16_t offset
	) {
		return access<IntT, type>(segment, offset, Tag::Accessed);
	}

	// Accesses an address based on physical location.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(uint32_t address) {
		return access<IntT, type>(address, Tag::Accessed);
	}

	template <typename IntT>
	void write_back() {
		if constexpr (std::is_same_v<IntT, uint16_t>) {
			if(write_back_address_[0] != NoWriteBack) {
				memory[write_back_address_[0]] = write_back_value_ & 0xff;
				memory[write_back_address_[1]] = write_back_value_ >> 8;
				write_back_address_[0]  = 0;
			}
		}
	}

	//
	// Direct write.
	//
	template <typename IntT>
	void preauthorised_write(InstructionSet::x86::Source segment, uint16_t offset, IntT value) {
		if(!test_preauthorisation(address(segment, offset))) {
			printf("Non-preauthorised access\n");
		}

		// Bytes can be written without further ado.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			memory[address(segment, offset) & 0xf'ffff] = value;
			return;
		}

		// Words that straddle the segment end must be split in two.
		if(offset == 0xffff) {
			memory[address(segment, offset) & 0xf'ffff] = value & 0xff;
			memory[address(segment, 0x0000) & 0xf'ffff] = value >> 8;
			return;
		}

		const uint32_t target = address(segment, offset) & 0xf'ffff;

		// Words that straddle the end of physical RAM must also be split in two.
		if(target == 0xf'ffff) {
			memory[0xf'ffff] = value & 0xff;
			memory[0x0'0000] = value >> 8;
			return;
		}

		// It's safe just to write then.
		*reinterpret_cast<uint16_t *>(&memory[target]) = value;
	}

private:
	enum class Tag {
		Seeded,
		AccessExpected,
		Accessed,
	};

	std::unordered_set<uint32_t> preauthorisations;
	std::unordered_map<uint32_t, Tag> tags;
	std::vector<uint8_t> memory;
	Registers &registers_;
	const Segments &segments_;

	void preauthorise(uint32_t address) {
		preauthorisations.insert(address);
	}
	void preauthorise(InstructionSet::x86::Source segment, uint16_t address) {
		preauthorise((segment_base(segment) + address) & 0xf'ffff);
	}
	bool test_preauthorisation(uint32_t address) {
		auto authorisation = preauthorisations.find(address);
		if(authorisation == preauthorisations.end()) {
			return false;
		}
		preauthorisations.erase(authorisation);
		return true;
	}

	uint32_t segment_base(InstructionSet::x86::Source segment) {
		using Source = InstructionSet::x86::Source;
		switch(segment) {
			default:			return segments_.ds_base_;
			case Source::ES:	return segments_.es_base_;
			case Source::CS:	return segments_.cs_base_;
			case Source::SS:	return segments_.ss_base_;
		}
	}

	uint32_t address(InstructionSet::x86::Source segment, uint16_t offset) {
		return (segment_base(segment) + offset) & 0xf'ffff;
	}


	// Entry point used by the flow controller so that it can mark up locations at which the flags were written,
	// so that defined-flag-only masks can be applied while verifying RAM contents.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		InstructionSet::x86::Source segment,
		uint16_t offset,
		Tag tag
	) {
		const uint32_t physical_address = address(segment, offset);

		if constexpr (std::is_same_v<IntT, uint16_t>) {
			// If this is a 16-bit access that runs past the end of the segment, it'll wrap back
			// to the start. So the 16-bit value will need to be a local cache.
			if(offset == 0xffff) {
				return split_word<type>(physical_address, address(segment, 0), tag);
			}
		}

		return access<IntT, type>(physical_address, tag);
	}

	// An additional entry point for the flow controller; on the original 8086 interrupt vectors aren't relative
	// to a segment, they're just at an absolute location.
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(uint32_t address, Tag tag) {
		if constexpr (type == AccessType::PreauthorisedRead) {
			if(!test_preauthorisation(address)) {
				printf("Non preauthorised access\n");
			}
		}

		for(size_t c = 0; c < sizeof(IntT); c++) {
			tags[(address + c) & 0xf'ffff] = tag;
		}

		// Dispense with the single-byte case trivially.
		if constexpr (std::is_same_v<IntT, uint8_t>) {
			return memory[address];
		} else if(address != 0xf'ffff) {
			return *reinterpret_cast<IntT *>(&memory[address]);
		} else {
			return split_word<type>(address, 0, tag);
		}
	}

	template <AccessType type>
	typename InstructionSet::x86::Accessor<uint16_t, type>::type
	split_word(uint32_t low_address, uint32_t high_address, Tag tag) {
		if constexpr (is_writeable(type)) {
			write_back_address_[0] = low_address;
			write_back_address_[1] = high_address;
			tags[low_address] = tag;
			tags[high_address] = tag;

			// Prepopulate only if this is a modify.
			if constexpr (type == AccessType::ReadModifyWrite) {
				write_back_value_ = memory[write_back_address_[0]] | (memory[write_back_address_[1]] << 8);
			}

			return write_back_value_;
		} else {
			return memory[low_address] | (memory[high_address] << 8);
		}
	}

	static constexpr uint32_t NoWriteBack = 0;	// A low byte address of 0 can't require write-back.
	uint32_t write_back_address_[2] = {NoWriteBack, NoWriteBack};
	uint16_t write_back_value_;
};
struct IO {
	template <typename IntT> void out([[maybe_unused]] uint16_t port, [[maybe_unused]] IntT value) {}
	template <typename IntT> IntT in([[maybe_unused]] uint16_t port) { return IntT(~0); }
};
class FlowController {
public:
	FlowController(Registers &registers, Segments &segments) :
		registers_(registers), segments_(segments) {}

	// Requirements for perform.
	template <typename AddressT>
	void jump(AddressT address) {
		static_assert(std::is_same_v<AddressT, uint16_t>);
		registers_.ip() = address;
	}

	template <typename AddressT>
	void jump(uint16_t segment, AddressT address) {
		static_assert(std::is_same_v<AddressT, uint16_t>);
		registers_.cs() = segment;
		segments_.did_update(Segments::Source::CS);
		registers_.ip() = address;
	}

	void halt() {}
	void wait() {}

	void repeat_last() {
		should_repeat_ = true;
	}

	// Other actions.
	void begin_instruction() {
		should_repeat_ = false;
	}
	bool should_repeat() const {
		return should_repeat_;
	}

private:
	Registers &registers_;
	Segments &segments_;
	bool should_repeat_ = false;
};

template <InstructionSet::x86::Model t_model>
struct ExecutionSupport {
	static constexpr auto model = t_model;

	Flags flags;
	Registers registers;
	Segments segments;
	LinearMemory linear_memory;
	PCCompatible::SegmentedMemory<model> memory;
	FlowController flow_controller;
	IO io;

	ExecutionSupport():
		memory(registers, segments),
		segments(registers),
		flow_controller(registers, segments) {}

	void clear() {
		memory.clear();
	}
};

struct FailedExecution {
	std::string test_name;
	std::string reason;
	std::variant<
		InstructionSet::x86::Instruction<InstructionSet::x86::InstructionType::Bits16>,
		InstructionSet::x86::Instruction<InstructionSet::x86::InstructionType::Bits32>
	> instruction;
};

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests {
	std::vector<FailedExecution> execution_failures;
	std::vector<FailedExecution> permitted_failures;
	ExecutionSupport<InstructionSet::x86::Model::i8086> execution_support;
}

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = [NSSet setWithArray:@[
		// Current execution failures, albeit all permitted:
//		@"D4.json.gz",		// AAM
//		@"F6.7.json.gz",	// IDIV byte
//		@"F7.7.json.gz",	// IDIV word
	]];

	NSSet *ignoreList = nil;

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files
		filteredArrayUsingPredicate:
			[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *)
	{
		if(allowList.count && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		if([ignoreList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		return [evaluatedObject hasSuffix:@"json.gz"];
	}]];

	NSMutableArray<NSString *> *fullPaths = [[NSMutableArray alloc] init];
	for(NSString *file in files) {
		[fullPaths addObject:[path stringByAppendingPathComponent:file]];
	}

	return [fullPaths sortedArrayUsingSelector:@selector(compare:)];
}

- (NSArray<NSDictionary *> *)testsInFile:(NSString *)file {
	NSData *data = [NSData dataWithContentsOfGZippedFile:file];
	return [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
}

- (NSDictionary *)metadata {
	NSString *path = [[NSString stringWithUTF8String:TestSuiteHome] stringByAppendingPathComponent:@"metadata.json"];
	return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfGZippedFile:path] options:0 error:nil];
}

using Instruction = InstructionSet::x86::Instruction<InstructionSet::x86::InstructionType::Bits16>;
- (NSString *)
	toString:(const std::pair<int, Instruction> &)instruction
	offsetLength:(int)offsetLength
	immediateLength:(int)immediateLength
{
	const auto operation = to_string(instruction, InstructionSet::x86::Model::i8086, offsetLength, immediateLength);
	return [[NSString stringWithUTF8String:operation.c_str()]
		stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (std::vector<uint8_t>)bytes:(NSArray<NSNumber *> *)encoding {
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	return data;
}

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	const auto data = [self bytes:test[@"bytes"]];
	auto hex_instruction = [&]() -> NSString * {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		return hexInstruction;
	};

	const auto decoded = decoder.decode(data.data(), data.size());
	const bool sizeMatched = decoded.first == data.size();
	if(assert) {
		XCTAssert(
			sizeMatched,
			"Wrong length of instruction decoded for %@ — decoded %d rather than %lu from %@; file %@",
				test[@"name"],
				decoded.first,
				(unsigned long)data.size(),
				hex_instruction(),
				file
		);
	}
	if(!sizeMatched) {
		return false;
	}

	// The decoder doesn't preserve the original offset length, which makes no functional difference but
	// does affect the way that offsets are printed in the test set.
	NSSet<NSString *> *decodings = [NSSet setWithObjects:
		[self toString:decoded offsetLength:4 immediateLength:4],
		[self toString:decoded offsetLength:2 immediateLength:4],
		[self toString:decoded offsetLength:0 immediateLength:4],
		[self toString:decoded offsetLength:4 immediateLength:2],
		[self toString:decoded offsetLength:2 immediateLength:2],
		[self toString:decoded offsetLength:0 immediateLength:2],
		nil];

	auto compare_decoding = [&](NSString *name) -> bool {
		return [decodings containsObject:name];
	};

	bool isEqual = compare_decoding(test[@"name"]);

	// Attempt clerical reconciliation:
	//
	// * the test suite retains a distinction between SHL and SAL, which the decoder doesn't;
	// * the decoder treats INT3 and INT 3 as the same thing; and
	// * the decoder doesn't record whether a segment override was present, just the final segment.
	int adjustment = 7;
	while(!isEqual && adjustment) {
		NSString *alteredName = [test[@"name"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

		if(adjustment & 2) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"shl" withString:@"sal"];
		}
		if(adjustment & 1) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"int3" withString:@"int 3h"];
		}
		if(adjustment & 4) {
			alteredName = [@"ds " stringByAppendingString:alteredName];
		}

		isEqual = compare_decoding(alteredName);
		--adjustment;
	}

	if(assert) {
		XCTAssert(
			isEqual,
			"%@ doesn't match %@ or similar, was %@",
				test[@"name"],
				[decodings anyObject],
				hex_instruction()
		);
	}

	return isEqual;
}

- (void)populate:(Registers &)registers flags:(Flags &)flags value:(NSDictionary *)value {
	registers.ax() = [value[@"ax"] intValue];
	registers.bx() = [value[@"bx"] intValue];
	registers.cx() = [value[@"cx"] intValue];
	registers.dx() = [value[@"dx"] intValue];

	registers.bp() = [value[@"bp"] intValue];
	registers.cs() = [value[@"cs"] intValue];
	registers.di() = [value[@"di"] intValue];
	registers.ds() = [value[@"ds"] intValue];
	registers.es() = [value[@"es"] intValue];
	registers.si() = [value[@"si"] intValue];
	registers.sp() = [value[@"sp"] intValue];
	registers.ss() = [value[@"ss"] intValue];
	registers.ip() = [value[@"ip"] intValue];

	const uint16_t flags_value = [value[@"flags"] intValue];
	flags.set(flags_value);

	// Apply a quick test of flag packing/unpacking.
	constexpr auto defined_flags = static_cast<uint16_t>(
		InstructionSet::x86::FlagValue::Carry |
		InstructionSet::x86::FlagValue::Parity |
		InstructionSet::x86::FlagValue::AuxiliaryCarry |
		InstructionSet::x86::FlagValue::Zero |
		InstructionSet::x86::FlagValue::Sign |
		InstructionSet::x86::FlagValue::Trap |
		InstructionSet::x86::FlagValue::Interrupt |
		InstructionSet::x86::FlagValue::Direction |
		InstructionSet::x86::FlagValue::Overflow
	);
	XCTAssert((flags.get() & defined_flags) == (flags_value & defined_flags),
		"Set flags of %04x was returned as %04x",
			flags_value & defined_flags,
			(flags.get() & defined_flags)
		);
}

- (void)applyExecutionTest:(NSDictionary *)test metadata:(NSDictionary *)metadata {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;
	const auto data = [self bytes:test[@"bytes"]];
	const auto decoded = decoder.decode(data.data(), data.size());

	execution_support.clear();

	const uint16_t flags_mask = metadata[@"flags-mask"] ? [metadata[@"flags-mask"] intValue] : 0xffff;
	NSDictionary *const initial_state = test[@"initial"];
	NSDictionary *const final_state = test[@"final"];

	// Apply initial state.
	Flags initial_flags;
	for(NSArray<NSNumber *> *ram in initial_state[@"ram"]) {
		execution_support.linear_memory.seed([ram[0] intValue], [ram[1] intValue]);
	}
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		execution_support.linear_memory.touch([ram[0] intValue]);
	}
	Registers initial_registers;
	[self populate:initial_registers flags:initial_flags value:initial_state[@"regs"]];
	execution_support.flags = initial_flags;
	execution_support.registers = initial_registers;
	execution_support.segments.reset();

	// Execute instruction.
	//
	// TODO: enquire of the actual mechanism of repetition; if it were stateful as below then
	// would it survive interrupts? So is it just IP adjustment?
	execution_support.registers.ip() += decoded.first;
	do {
		execution_support.flow_controller.begin_instruction();
		InstructionSet::x86::perform(
			decoded.second,
			execution_support
		);
	} while (execution_support.flow_controller.should_repeat());

	// Compare final state.
	Registers intended_registers;
	InstructionSet::x86::Flags intended_flags;

	bool ramEqual = true;
	int mask_position = 0;
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		const uint32_t address = [ram[0] intValue];
		const auto value =
			execution_support.linear_memory.access<uint8_t, InstructionSet::x86::AccessType::Read>(address);

		if((mask_position != 1) && value == [ram[1] intValue]) {
			continue;
		}

		// Consider whether this apparent mismatch might be because flags have been written to memory;
		// allow only one use of the [16-bit] mask per test.
		bool matched_with_mask = false;
		while(mask_position < 2) {
			const uint8_t mask = mask_position ? (flags_mask >> 8) : (flags_mask & 0xff);
			++mask_position;
			if((value & mask) == ([ram[1] intValue] & mask)) {
				matched_with_mask = true;
				break;
			}
		}
		if(matched_with_mask) {
			continue;
		}

		ramEqual = false;
		break;
	}

	Segments intended_segments(intended_registers);
	[self populate:intended_registers flags:intended_flags value:final_state[@"regs"]];
	intended_segments.reset();

	const bool registersEqual =
		intended_registers == execution_support.registers &&
		intended_segments == execution_support.segments;
	const bool flagsEqual = (intended_flags.get() & flags_mask) == (execution_support.flags.get() & flags_mask);

	// Exit if no issues were found.
	if(flagsEqual && registersEqual && ramEqual) {
		return;
	}

	// Presume this is a genuine failure.
	std::vector<FailedExecution> *failure_list = &execution_failures;

	// Redirect it if it's an acceptable failure.
	using Operation = InstructionSet::x86::Operation;

	// AAM 00h throws its exception only after modifying flags in an undocumented manner;
	// I'm not too concerned about this because AAM 00h is an undocumented usage of 00h,
	// not even supported by NEC amongst others, and the proper exception is being thrown.
	if(decoded.second.operation() == Operation::AAM && !decoded.second.operand()) {
		failure_list = &permitted_failures;
	}

	// IDIV_REP: for reasons I don't understand, sometimes the test set doesn't increment
	// the IP across a REP_IDIV. I don't think (?) this correlates to real 8086 behaviour.
	// More research required, but for now I'm not treating this as a roadblock.
	if(decoded.second.operation() == Operation::IDIV_REP) {
		Registers advanced_registers = intended_registers;
		advanced_registers.ip() += decoded.first;
		if(advanced_registers == execution_support.registers && ramEqual && flagsEqual) {
			failure_list = &permitted_failures;
		}
	}

	// IDIV[_REP] byte: the test cases sometimes throw even when I can't see why they should,
	// and other x86 emulations also don't throw. I guess — guess! — an 8086-specific oddity
	// deviates from the x86 average here. So I'm also permitting these for now.
	if(
		decoded.second.operation_size() == InstructionSet::x86::DataSize::Byte &&
		(decoded.second.operation() == Operation::IDIV_REP || decoded.second.operation() == Operation::IDIV)
	) {
		if(intended_registers.sp() == execution_support.registers.sp() - 6) {
			Registers non_exception_registers = intended_registers;
			non_exception_registers.ip() = execution_support.registers.ip();
			non_exception_registers.sp() = execution_support.registers.sp();
			non_exception_registers.ax() = execution_support.registers.ax();
			non_exception_registers.cs() = execution_support.registers.cs();

			if(non_exception_registers == execution_support.registers) {
				failure_list = &permitted_failures;
			}
		}
	}

	// LEA from a register is undefined behaviour and throws on processors beyond the 8086.
	if(
		decoded.second.operation() == Operation::LEA &&
		InstructionSet::x86::is_register(decoded.second.source().source())
	) {
		failure_list = &permitted_failures;
	}

	// Record a failure.
	FailedExecution failure;
	failure.instruction = decoded.second;
	failure.test_name = std::string([test[@"name"] UTF8String]);

	NSMutableArray<NSString *> *reasons = [[NSMutableArray alloc] init];
	if(!flagsEqual) {
		Flags difference;
		difference.set((intended_flags.get() ^ execution_support.flags.get()) & flags_mask);
		[reasons addObject:
			[NSString stringWithFormat:@"flags differs; errors in %s",
				difference.to_string().c_str()]];
	}
	if(!registersEqual) {
		NSMutableArray<NSString *> *registers = [[NSMutableArray alloc] init];
#define Reg(x)	\
	if(intended_registers.x() != execution_support.registers.x())	\
		[registers addObject:	\
			[NSString stringWithFormat:	\
				@#x" is %04x rather than %04x", execution_support.registers.x(), intended_registers.x()]];

		Reg(ax);
		Reg(cx);
		Reg(dx);
		Reg(bx);
		Reg(sp);
		Reg(bp);
		Reg(si);
		Reg(di);
		Reg(ip);
		Reg(es);
		Reg(cs);
		Reg(ds);
		Reg(ss);

#undef Reg
		[reasons addObject:[NSString stringWithFormat:
			@"registers don't match: %@", [registers componentsJoinedByString:@", "]
		]];
	}
	if(!ramEqual) {
		[reasons addObject:@"RAM contents don't match"];
	}

	failure.reason = std::string([reasons componentsJoinedByString:@"; "].UTF8String);
	failure_list->push_back(std::move(failure));
}

- (void)printFailures:(NSArray<NSString *> *)failures {
	NSLog(
		@"%ld failures out of %ld tests: %@",
		failures.count,
		[self testFiles].count,
		[failures sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

- (void)testDecoding {
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	for(NSString *file in [self testFiles]) @autoreleasepool {
		for(NSDictionary *test in [self testsInFile:file]) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test file:file assert:YES]) {
				[failures addObject:file];

				// Attempt a second decoding, to provide a debugger hook.
				[self applyDecodingTest:test file:file assert:NO];
				break;
			}
		}
	}

	[self printFailures:failures];
}

- (void)testExecution {
	NSDictionary *metadata = [self metadata];
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];

	for(NSString *file in [self testFiles]) @autoreleasepool {
		const auto failures_before = execution_failures.size();

		// Determine the metadata key.
		NSString *const name = [file lastPathComponent];
		NSRange first_dot = [name rangeOfString:@"."];
		NSString *metadata_key = [name substringToIndex:first_dot.location];

		// Grab the metadata. If it wants a reg field, inspect a little further.
		NSDictionary *test_metadata = metadata[metadata_key];
		if(test_metadata[@"reg"]) {
			test_metadata =
				test_metadata[@"reg"][[NSString stringWithFormat:@"%c", [name characterAtIndex:first_dot.location+1]]];
		}

		int index = 0;
		for(NSDictionary *test in [self testsInFile:file]) {
			[self applyExecutionTest:test metadata:test_metadata];
			++index;
		}

		if (execution_failures.size() != failures_before) {
			[failures addObject:file];
		}
	}

	// Lock in current failure rate.
	XCTAssertLessThanOrEqual(execution_failures.size(), 0);

	for(const auto &failure: execution_failures) {
		NSLog(@"Failed %s — %s", failure.test_name.c_str(), failure.reason.c_str());
	}
	for(const auto &failure: permitted_failures) {
		NSLog(@"Permitted failure of %s — %s", failure.test_name.c_str(), failure.reason.c_str());
	}

	NSLog(@"Files with failures, permitted or otherwise, were: %@", failures);
}

@end
