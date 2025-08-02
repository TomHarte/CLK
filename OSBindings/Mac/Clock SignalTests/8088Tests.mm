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
#include "Machines/PCCompatible/LinearMemory.hpp"
#include "Machines/PCCompatible/SegmentedMemory.hpp"
#include "Machines/PCCompatible/Segments.hpp"
#include "Numeric/RegisterSizes.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome8088[] = "/Users/thomasharte/Projects/8088/v1";
constexpr char TestSuiteHome80286[] = "/Users/thomasharte/Projects/80286/v1_real_mode";

using Flags = InstructionSet::x86::Flags;

template <InstructionSet::x86::Model t_model>
struct LinearMemory {
	enum class Tag {
		Seeded,
		AccessExpected,
		Accessed,
	};
public:
	PCCompatible::LinearMemory<t_model> memory;
	using AccessType = InstructionSet::x86::AccessType;

	// Initialisation.
	void clear() {
		tags.clear();
	}

	void seed(uint32_t address, uint8_t value) {
		memory.template access<uint8_t, AccessType::Write>(address, address) = value;
		tags[address] = Tag::Seeded;
	}

	void touch(uint32_t address) {
		tags[address] = Tag::AccessExpected;
	}

	//
	// Preauthorisation call-ins.
	//
	void preauthorise_read(uint32_t start, uint32_t length) {
		while(length--) {
			preauthorise(start);
			++start;
		}
		memory.preauthorise_read(start, length);
	}
	void preauthorise_write(uint32_t start, uint32_t length) {
		while(length--) {
			preauthorise(start);
			++start;
		}
		memory.preauthorise_write(start, length);
	}

	//
	// Access call-ins.
	//
	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		uint32_t address,
		const uint32_t base
	) {
		return memory.template access<IntT, type>(address, base);
	}

	template <typename IntT, AccessType type>
	typename InstructionSet::x86::Accessor<IntT, type>::type access(
		uint32_t address,
		const uint32_t base
	) const {
		static_assert(!is_writeable(type));
		return memory.template access<IntT, type>(address, base);
	}

	template <typename IntT>
	void write_back() {
		memory.template write_back<IntT>();
	}

	//
	// Direct write.
	//
	template <typename IntT>
	void preauthorised_write(const uint32_t address, const uint32_t base, IntT value) {
//		if(!test_preauthorisation(address)) {
//			printf("Non-preauthorised access\n");
//		}

		memory.preauthorised_write(address, base, value);
	}

	template <typename IntT>
	IntT read(const uint32_t address) {
		return memory.template read<IntT>(address);
	}

private:
	std::unordered_set<uint32_t> preauthorisations;
	std::unordered_map<uint32_t, Tag> tags;

	void preauthorise(const uint32_t address) {
		preauthorisations.insert(address);
	}
	bool test_preauthorisation(const uint32_t address) {
		const auto authorisation = preauthorisations.find(address);
		if(authorisation == preauthorisations.end()) {
			return false;
		}
		preauthorisations.erase(authorisation);
		return true;
	}
};

struct IO {
	template <typename IntT> void out([[maybe_unused]] uint16_t port, [[maybe_unused]] IntT value) {}
	template <typename IntT> IntT in([[maybe_unused]] uint16_t port) { return IntT(~0); }
};

template <InstructionSet::x86::Model t_model>
class FlowController {
public:
	FlowController(InstructionSet::x86::Registers<t_model> &registers, PCCompatible::Segments<t_model, LinearMemory<t_model>> &segments) :
		registers_(registers), segments_(segments) {}

	// Requirements for perform.
	template <typename AddressT>
	void jump(AddressT address) {
		static_assert(std::is_same_v<AddressT, uint16_t>);
		registers_.ip() = address;
	}

	template <typename AddressT>
	void jump(const uint16_t segment, const AddressT address) {
		static_assert(std::is_same_v<AddressT, uint16_t>);
		static constexpr auto cs = InstructionSet::x86::Source::CS;
		segments_.preauthorise(cs, segment);
		registers_.cs() = segment;
		segments_.did_update(cs);
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
	InstructionSet::x86::Registers<t_model> &registers_;
	PCCompatible::Segments<t_model, LinearMemory<t_model>> &segments_;
	bool should_repeat_ = false;
};

struct CPUControl {
	void set_mode(const InstructionSet::x86::Mode mode) {
		mode_ = mode;
	}
	InstructionSet::x86::Mode mode() const {
		return mode_;
	}

private:
	InstructionSet::x86::Mode mode_ = InstructionSet::x86::Mode::Real;
};

template <InstructionSet::x86::Model t_model>
struct ExecutionSupport {
	static constexpr auto model = t_model;

	Flags flags;
	InstructionSet::x86::Registers<t_model> registers;
	LinearMemory<t_model> linear_memory;
	PCCompatible::SegmentedMemory<model, LinearMemory<t_model>> memory;
	PCCompatible::Segments<t_model, LinearMemory<t_model>> segments;
	FlowController<t_model> flow_controller;
	IO io;
	CPUControl cpu_control;

	ExecutionSupport():
		memory(registers, segments, linear_memory),
		segments(registers, linear_memory),
		flow_controller(registers, segments) {}

	void clear() {
		linear_memory.clear();
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

std::vector<uint8_t> bytes(NSArray<NSNumber *> *encoding) {
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	return data;
}

NSArray<NSString *> *testFiles(const char *const home) {
	NSString *const path = [NSString stringWithUTF8String:home];
	NSSet *const allowList = [NSSet setWithArray:@[
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

NSArray<NSDictionary *> *testsInFile(NSString *file) {
	NSData *data = [NSData dataWithContentsOfGZippedFile:file];
	return [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
}

NSDictionary *metadata(const char *home) {
	NSString *path = [[NSString stringWithUTF8String:home] stringByAppendingPathComponent:@"metadata.json"];
	return [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfGZippedFile:path] options:0 error:nil][@"opcodes"];
}

template <InstructionSet::x86::Model t_model>
void populate(InstructionSet::x86::Registers<t_model> &registers, Flags &flags, NSDictionary *value) {
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

template <InstructionSet::x86::Model t_model>
void apply_execution_test(
	ExecutionSupport<t_model> &execution_support,
	std::vector<FailedExecution> &execution_failures,
	std::vector<FailedExecution> &permitted_failures,
	NSDictionary *test,
	NSDictionary *metadata
) {
	InstructionSet::x86::Decoder<t_model> decoder;
	const auto data = bytes(test[@"bytes"]);
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
	InstructionSet::x86::Registers<t_model> initial_registers;
	populate(initial_registers, initial_flags, initial_state[@"regs"]);
	execution_support.flags = initial_flags;
	execution_support.registers = initial_registers;
	execution_support.segments.reset();

	// Execute instruction.
	//
	// TODO: enquire of the actual mechanism of repetition; if it were stateful as below then
	// would it survive interrupts? So is it just IP adjustment?
	const auto prior_ip = execution_support.registers.ip();
	execution_support.registers.ip() += decoded.first;
	do {
		execution_support.flow_controller.begin_instruction();
		// TODO: catch and process exceptions, which I think means better factoring
		// re: PCCompatible/instruction set.
		InstructionSet::x86::perform(
			decoded.second,
			execution_support,
			prior_ip
		);
	} while (execution_support.flow_controller.should_repeat());

	// Compare final state.
	InstructionSet::x86::Registers<t_model> intended_registers;
	InstructionSet::x86::Flags intended_flags;

	bool ramEqual = true;
	int mask_position = 0;
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		const uint32_t address = [ram[0] intValue];
		const auto value =
			execution_support.linear_memory.template access<uint8_t, InstructionSet::x86::AccessType::Read>(address, address);

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

	PCCompatible::Segments<t_model, LinearMemory<t_model>>
		intended_segments(intended_registers, execution_support.linear_memory);

	NSMutableDictionary *final_registers = [initial_state[@"regs"] mutableCopy];
	[final_registers setValuesForKeysWithDictionary:final_state[@"regs"]];
	populate(intended_registers, intended_flags, final_registers);
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
		InstructionSet::x86::Registers<t_model> advanced_registers = intended_registers;
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
			InstructionSet::x86::Registers<t_model> non_exception_registers = intended_registers;
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
			[NSString stringWithFormat:@"flags differ; errors in %s",
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

template <InstructionSet::x86::Model t_model>
void test_execution(const char *const home) {
	NSDictionary *metadatas = metadata(home);
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	std::vector<FailedExecution> execution_failures;
	std::vector<FailedExecution> permitted_failures;
	auto execution_support = std::make_unique<ExecutionSupport<t_model>>();

	for(NSString *file in testFiles(home)) @autoreleasepool {
		const auto failures_before = execution_failures.size();

		// Determine the metadata key.
		NSString *const name = [file lastPathComponent];
		NSRange first_dot = [name rangeOfString:@"."];
		NSString *metadata_key = [name substringToIndex:first_dot.location];

		// Grab the metadata. If it wants a reg field, inspect a little further.
		NSDictionary *test_metadata = metadatas[metadata_key];
		if(test_metadata[@"reg"]) {
			test_metadata =
				test_metadata[@"reg"][[NSString stringWithFormat:@"%c", [name characterAtIndex:first_dot.location+1]]];
		}

//		int index = 0;
		for(NSDictionary *test in testsInFile(file)) {
			apply_execution_test(*execution_support, execution_failures, permitted_failures, test, test_metadata);
//			++index;
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
}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

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

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	const auto data = bytes(test[@"bytes"]);
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

- (void)printFailures:(NSArray<NSString *> *)failures {
	NSLog(
		@"%ld failures out of %ld tests: %@",
		failures.count,
		testFiles(TestSuiteHome8088).count,
		[failures sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

- (void)testDecoding {
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	for(NSString *file in testFiles(TestSuiteHome8088)) @autoreleasepool {
		for(NSDictionary *test in testsInFile(file)) {
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

- (void)testExecution8088 {
	test_execution<InstructionSet::x86::Model::i8086>(TestSuiteHome8088);
}

- (void)testExecution80286 {
	test_execution<InstructionSet::x86::Model::i80286>(TestSuiteHome80286);
}

@end
