//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "InstructionSets/ARM/Disassembler.hpp"
#include "InstructionSets/ARM/Executor.hpp"

#include "NSData+dataWithContentsOfGZippedFile.h"

#include <map>
#include <sstream>
#include <vector>

using namespace InstructionSet::ARM;

namespace {

struct MemoryLedger {
	template <typename IntT>
	bool write(uint32_t address, IntT source, Mode, bool) {
		const auto is_faulty = [&](uint32_t address) -> bool {
			return
				write_pointer == writes.size() ||
				writes[write_pointer].size != sizeof(IntT) ||
				writes[write_pointer].address != address ||
				writes[write_pointer].value != source;
		};

		// The test set sometimes worries about write alignment, sometimes not...
		if(is_faulty(address) && is_faulty(address & static_cast<uint32_t>(~3))) {
			return false;
		}
		++write_pointer;
		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, Mode, bool) {
		const auto is_faulty = [&](uint32_t address) -> bool {
			return
				read_pointer == reads.size() ||
				reads[read_pointer].size != sizeof(IntT) ||
				reads[read_pointer].address != address;
		};

		// As per writes; the test set sometimes forces alignment on the record, sometimes not...
		if(is_faulty(address) && is_faulty(address & static_cast<uint32_t>(~3))) {
			return false;
		}
		source = reads[read_pointer].value;
		++read_pointer;
		return true;
	}

	struct Access {
		size_t size;
		uint32_t address;
		uint32_t value;
	};

	template <typename IntT>
	void add_access(bool is_read, uint32_t address, IntT value) {
		auto &read = is_read ? reads.emplace_back() : writes.emplace_back();
		read.address = address;
		read.value = value;
		read.size = sizeof(IntT);
	}

	std::vector<Access> reads;
	std::vector<Access> writes;
	size_t read_pointer = 0;
	size_t write_pointer = 0;
};

}

@interface ARMDecoderTests : XCTestCase
@end

@implementation ARMDecoderTests

- (void)testBarrelShifterLogicalLeft {
	uint32_t value;
	uint32_t carry;

	// Test a shift by 1 into carry.
	value = 0x8000'0000;
	shift<ShiftType::LogicalLeft, true, true>(value, 1, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by 18 into carry.
	value = 0x0000'4001;
	shift<ShiftType::LogicalLeft, true, true>(value, 18, carry);
	XCTAssertEqual(value, 0x4'0000);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by 17, not generating carry.
	value = 0x0000'4001;
	shift<ShiftType::LogicalLeft, true, true>(value, 17, carry);
	XCTAssertEqual(value, 0x8002'0000);
	XCTAssertEqual(carry, 0);
}

- (void)testBarrelShifterLogicalRight {
	uint32_t value;
	uint32_t carry;

	// Test successive shifts by 4; one generating carry and one not.
	value = 0x12345678;
	shift<ShiftType::LogicalRight, true, true>(value, 4, carry);
	XCTAssertEqual(value, 0x1234567);
	XCTAssertNotEqual(carry, 0);

	shift<ShiftType::LogicalRight, true, true>(value, 4, carry);
	XCTAssertEqual(value, 0x123456);
	XCTAssertEqual(carry, 0);

	// Test shift by 1.
	value = 0x8003001;
	shift<ShiftType::LogicalRight, true, true>(value, 1, carry);
	XCTAssertEqual(value, 0x4001800);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by greater than 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true, true>(value, 33, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test shifts by 32: result is always 0, carry is whatever was in bit 31.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);

	value = 0x7fff'ffff;
	shift<ShiftType::LogicalRight, true, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test that a logical right shift by 0 is the same as a shift by 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true, true>(value, 0, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);
}

- (void)testBarrelShifterArithmeticRight {
	uint32_t value;
	uint32_t carry;

	// Test a short negative shift.
	value = 0x8000'0030;
	shift<ShiftType::ArithmeticRight, true, true>(value, 1, carry);
	XCTAssertEqual(value, 0xc000'0018);
	XCTAssertEqual(carry, 0);

	// Test a medium negative shift without carry.
	value = 0xffff'0000;
	shift<ShiftType::ArithmeticRight, true, true>(value, 11, carry);
	XCTAssertEqual(value, 0xffff'ffe0);
	XCTAssertEqual(carry, 0);

	// Test a medium negative shift with carry.
	value = 0xffc0'0000;
	shift<ShiftType::ArithmeticRight, true, true>(value, 23, carry);
	XCTAssertEqual(value, 0xffff'ffff);
	XCTAssertNotEqual(carry, 0);

	// Test a long negative shift.
	value = 0x8000'0000;
	shift<ShiftType::ArithmeticRight, true, true>(value, 32, carry);
	XCTAssertEqual(value, 0xffff'ffff);
	XCTAssertNotEqual(carry, 0);

	// Test a positive shift.
	value = 0x0123'0031;
	shift<ShiftType::ArithmeticRight, true, true>(value, 3, carry);
	XCTAssertEqual(value, 0x24'6006);
	XCTAssertEqual(carry, 0);
}

- (void)testBarrelShifterRotateRight {
	uint32_t value;
	uint32_t carry;

	// Test a short rotate by one hex digit.
	value = 0xabcd'1234;
	shift<ShiftType::RotateRight, true, true>(value, 4, carry);
	XCTAssertEqual(value, 0x4abc'd123);
	XCTAssertEqual(carry, 0);

	// Test a longer rotate, with carry.
	value = 0xa5f9'6342;
	shift<ShiftType::RotateRight, true, true>(value, 17, carry);
	XCTAssertEqual(value, 0xb1a1'52fc);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate by 32 without carry.
	value = 0x385f'7dce;
	shift<ShiftType::RotateRight, true, true>(value, 32, carry);
	XCTAssertEqual(value, 0x385f'7dce);
	XCTAssertEqual(carry, 0);

	// Test a rotate by 32 with carry.
	value = 0xfecd'ba12;
	shift<ShiftType::RotateRight, true, true>(value, 32, carry);
	XCTAssertEqual(value, 0xfecd'ba12);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate through carry, carry not set.
	value = 0x123f'abcf;
	carry = 0;
	shift<ShiftType::RotateRight, true, true>(value, 0, carry);
	XCTAssertEqual(value, 0x091f'd5e7);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate through carry, carry set.
	value = 0x123f'abce;
	carry = 1;
	shift<ShiftType::RotateRight, true, true>(value, 0, carry);
	XCTAssertEqual(value, 0x891f'd5e7);
	XCTAssertEqual(carry, 0);
}

- (void)testRegisterModes {
	Registers r;

	// Set all user mode registers to their indices.
	r.set_mode(Mode::User);
	for(int c = 0; c < 15; c++) {
		r[c] = c;
	}

	// Set FIQ registers.
	r.set_mode(Mode::FIQ);
	for(int c = 8; c < 15; c++) {
		r[c] = c | 0x100;
	}

	// Set IRQ registers.
	r.set_mode(Mode::IRQ);
	for(int c = 13; c < 15; c++) {
		r[c] = c | 0x200;
	}

	// Set supervisor registers.
	r.set_mode(Mode::FIQ);
	r.set_mode(Mode::User);
	r.set_mode(Mode::Supervisor);
	for(int c = 13; c < 15; c++) {
		r[c] = c | 0x300;
	}

	// Check all results.
	r.set_mode(Mode::User);
	r.set_mode(Mode::FIQ);
	for(int c = 0; c < 8; c++) {
		XCTAssertEqual(r[c], c);
	}
	for(int c = 8; c < 15; c++) {
		XCTAssertEqual(r[c], c | 0x100);
	}

	r.set_mode(Mode::FIQ);
	r.set_mode(Mode::IRQ);
	r.set_mode(Mode::User);
	r.set_mode(Mode::FIQ);
	r.set_mode(Mode::Supervisor);
	for(int c = 0; c < 13; c++) {
		XCTAssertEqual(r[c], c);
	}
	for(int c = 13; c < 15; c++) {
		XCTAssertEqual(r[c], c | 0x300);
	}

	r.set_mode(Mode::FIQ);
	r.set_mode(Mode::User);
	for(int c = 0; c < 15; c++) {
		XCTAssertEqual(r[c], c);
	}

	r.set_mode(Mode::Supervisor);
	r.set_mode(Mode::IRQ);
	for(int c = 0; c < 13; c++) {
		XCTAssertEqual(r[c], c);
	}
	for(int c = 13; c < 15; c++) {
		XCTAssertEqual(r[c], c | 0x200);
	}
}

- (void)testFlags {
	Registers regs;

	for(int c = 0; c < 256; c++) {
		regs.set_mode(Mode::Supervisor);

		const uint32_t status = ((c & 0xfc) << 26) | (c & 0x03);
		regs.set_status(status);
		XCTAssertEqual(status, regs.status());
	}
}

- (void)testMessy {
	NSData *const tests =
		[NSData dataWithContentsOfGZippedFile:
			[[NSBundle bundleForClass:[self class]]
				pathForResource:@"test"
				ofType:@"txt.gz"
				inDirectory:@"Messy ARM"]
		];
	const std::string text((char *)tests.bytes);
	std::istringstream input(text);

	input >> std::hex;

	static constexpr auto model = Model::ARMv2with32bitAddressing;
	using Exec = Executor<model, MemoryLedger>;
	std::unique_ptr<Exec> test;

	struct FailureRecord {
		int count = 0;
		int first = 0;
		NSString *sample;
	};
	std::map<uint32_t, FailureRecord> failures;

	uint32_t opcode = 0;
	bool ignore_opcode = false;
	uint32_t masks[16];
	uint32_t test_pc_offset = 8;

	int test_count = 0;
	while(!input.eof()) {
		std::string label;
		input >> label;

		if(label == "**") {
			memset(masks, 0xff, sizeof(masks));
			ignore_opcode = false;
			test_pc_offset = 8;

			input >> opcode;
			test_count = 0;

			InstructionSet::ARM::Disassembler<model> disassembler;
			InstructionSet::ARM::dispatch<model>(opcode, disassembler);
			static constexpr uint32_t pc_address_mask = 0x03ff'fffc;

			const auto instruction = disassembler.last();
			switch(instruction.operation) {
				case Instruction::Operation::BL:
					// Tests don't multiplex flags into PC for storage to R14.
					masks[14] = pc_address_mask;
				break;

				case Instruction::Operation::SWI:
					// Tested CPU either doesn't switch into supervisor mode, or
					// is sufficiently accurate in its pipeline that register
					// changes haven't happened yet.
					ignore_opcode = true;
				break;

				case Instruction::Operation::MOV:
					// MOV from PC will pick up the address only in the test cases.
					if(
						instruction.operand2.type == Operand::Type::Register &&
						instruction.operand2.value == 15
					) {
						masks[instruction.destination.value] = pc_address_mask;
					}

					// MOV to PC; there are both pipeline capture errors in the test
					// set and its ARM won't change privilege level on a write to R15.
					// Similarly, if the PC is operand 2 then it'll also contain the
					// PSR on an ARM2 but not in the test set.
					if(instruction.destination.value == 15  || instruction.operand2.value == 15) {
						ignore_opcode = true;
					}
				break;

				case Instruction::Operation::TEQ:
				case Instruction::Operation::TST:
				case Instruction::Operation::ORR:
				case Instruction::Operation::BIC:
				case Instruction::Operation::SUB:
				case Instruction::Operation::ADD:
					// Routinely used to change privilege level on an ARM2 but
					// doesn't seem to have that effect on the ARM used to generate
					// the test set.
					if(instruction.destination.value == 15 || instruction.operand2.value == 15) {
						ignore_opcode = true;
					}
				break;

				case Instruction::Operation::STR:
				case Instruction::Operation::LDR:
					// Neither loads nor stores with R15 are matched to ARM2 behaviour by the test source.
					ignore_opcode = instruction.destination.value == 15;
				break;

				case Instruction::Operation::STM:
				case Instruction::Operation::LDM:
					// If the PC is involved, just skip the test; PC/PSR differences abound.
					ignore_opcode = instruction.operand1.value & (1 << 15);
				break;

				case Instruction::Operation::MCR:
				case Instruction::Operation::MRC:
					// The test case doesn't seem to throw on a missing coprocessor.
					ignore_opcode = true;
				break;

				default: break;
			}

			continue;
		}

		if(ignore_opcode) continue;

		if(label == "Before:" || label == "After:") {
			// Read register state.
			uint32_t regs[16];
			for(int c = 0; c < 16; c++) {
				input >> regs[c];
			}

			if(!test) test = std::make_unique<Exec>();
			auto &registers = test->registers();
			if(label == "Before:") {
				// This is the start of a new test.

				// Establish implicit register values.
				for(uint32_t c = 0; c < 15; c++) {
					registers.set_mode(Mode::FIQ);
					registers[c] = 0x200 | c;

					registers.set_mode(Mode::Supervisor);
					registers[c] = 0x300 | c;

					registers.set_mode(Mode::IRQ);
					registers[c] = 0x400 | c;

					registers.set_mode(Mode::User);
					registers[c] = 0x100 | c;
				}

				// Apply provided state.
				registers.set_mode(Mode::Supervisor);	// To make sure the actual mode is applied.
				registers.set_pc(regs[15] - 8);
				registers.set_status(regs[15]);
				for(uint32_t c = 0; c < 15; c++) {
					registers[c] = regs[c];
				}
			} else {
				// Execute test and compare.
				++test_count;

				if(opcode == 0xe1a0ae2f && test_count == 2) {
					printf("");
				}

				execute(opcode, *test);
				NSMutableString *error = nil;

				for(uint32_t c = 0; c < 15; c++) {
					if((regs[c] & masks[c]) != (registers[c] & masks[c])) {
						if(!error) error = [[NSMutableString alloc] init]; else [error appendString:@"; "];
						[error appendFormat:@"R%d %08x v %08x", c, regs[c], registers[c]];
					}
				}
				if((regs[15] & masks[15]) != (registers.pc_status(test_pc_offset) & masks[15])) {
					if(!error) error = [[NSMutableString alloc] init]; else [error appendString:@"; "];
					[error appendFormat:@"; PC/PSR %08x/%08x v %08x/%08x",
						regs[15] & 0x03ff'fffc, regs[15] & ~0x03ff'fffc,
						registers.pc(test_pc_offset), registers.status()];
				}

				if(error) {
					++failures[opcode].count;
					if(failures[opcode].count == 1) {
						failures[opcode].first = test_count;
						failures[opcode].sample = error;
					}
				}

				test.reset();
			}
			continue;
		}

		// TODO: supply information below to ledger, and then use and test it.

		uint32_t address;
		uint32_t value;
		input >> address >> value;

		if(label == "r.b") {
			// Capture a byte read for provision.
			test->bus.add_access<uint8_t>(true, address, value);
			continue;
		}

		if(label == "r.w") {
			// Capture a word read for provision.
			test->bus.add_access<uint32_t>(true, address, value);
			continue;
		}

		if(label == "w.b") {
			// Capture a byte write for comparison.
			test->bus.add_access<uint8_t>(false, address, value);
			continue;
		}

		if(label == "w.w") {
			// Capture a word write for comparison.
			test->bus.add_access<uint32_t>(false, address, value);
			continue;
		}
	}

	XCTAssertTrue(failures.empty());

	if(!failures.empty()) {
		NSLog(@"Failed %zu instructions; examples below", failures.size());
		for(const auto &pair: failures) {
			NSLog(@"%08x, %d total, test %d: %@", pair.first, pair.second.count, pair.second.first, pair.second.sample);
		}
	}


	for(const auto &pair: failures) {
		printf("%08x ", pair.first);
	}
}

@end
