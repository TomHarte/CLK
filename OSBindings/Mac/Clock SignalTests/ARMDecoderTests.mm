//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/ARM/Executor.hpp"
#include "CSROMFetcher.hpp"

using namespace InstructionSet::ARM;

namespace {

struct Memory {
	std::vector<uint8_t> rom;

	template <typename IntT>
	bool write(uint32_t address, IntT source, Mode mode, bool trans) {
		(void)mode;
		(void)trans;

		printf("W of %08x to %08x [%lu]\n", source, address, sizeof(IntT));

		if(has_moved_rom_ && address < ram_.size()) {
			*reinterpret_cast<IntT *>(&ram_[address]) = source;
		}

		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, Mode mode, bool trans) {
		(void)mode;
		(void)trans;

		if(address >= 0x3800000) {
			has_moved_rom_ = true;
			source = *reinterpret_cast<const IntT *>(&rom[address - 0x3800000]);
		} else if(!has_moved_rom_) {
			// TODO: this is true only very transiently.
			source = *reinterpret_cast<const IntT *>(&rom[address]);
		} else if(address < ram_.size()) {
			source = *reinterpret_cast<const IntT *>(&ram_[address]);
		} else {
			source = 0;
			printf("Unknown read from %08x [%lu]\n", address, sizeof(IntT));
		}

		return true;
	}

	private:
		bool has_moved_rom_ = false;
		std::array<uint8_t, 4*1024*1024> ram_{};
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
	shift<ShiftType::LogicalLeft, true>(value, 1, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by 18 into carry.
	value = 0x0000'4001;
	shift<ShiftType::LogicalLeft, true>(value, 18, carry);
	XCTAssertEqual(value, 0x4'0000);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by 17, not generating carry.
	value = 0x0000'4001;
	shift<ShiftType::LogicalLeft, true>(value, 17, carry);
	XCTAssertEqual(value, 0x8002'0000);
	XCTAssertEqual(carry, 0);
}

- (void)testBarrelShifterLogicalRight {
	uint32_t value;
	uint32_t carry;

	// Test successive shifts by 4; one generating carry and one not.
	value = 0x12345678;
	shift<ShiftType::LogicalRight, true>(value, 4, carry);
	XCTAssertEqual(value, 0x1234567);
	XCTAssertNotEqual(carry, 0);

	shift<ShiftType::LogicalRight, true>(value, 4, carry);
	XCTAssertEqual(value, 0x123456);
	XCTAssertEqual(carry, 0);

	// Test shift by 1.
	value = 0x8003001;
	shift<ShiftType::LogicalRight, true>(value, 1, carry);
	XCTAssertEqual(value, 0x4001800);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by greater than 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 33, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test shifts by 32: result is always 0, carry is whatever was in bit 31.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);

	value = 0x7fff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test that a logical right shift by 0 is the same as a shift by 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 0, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);
}

- (void)testBarrelShifterArithmeticRight {
	uint32_t value;
	uint32_t carry;

	// Test a short negative shift.
	value = 0x8000'0030;
	shift<ShiftType::ArithmeticRight, true>(value, 1, carry);
	XCTAssertEqual(value, 0xc000'0018);
	XCTAssertEqual(carry, 0);

	// Test a medium negative shift without carry.
	value = 0xffff'0000;
	shift<ShiftType::ArithmeticRight, true>(value, 11, carry);
	XCTAssertEqual(value, 0xffff'ffe0);
	XCTAssertEqual(carry, 0);

	// Test a medium negative shift with carry.
	value = 0xffc0'0000;
	shift<ShiftType::ArithmeticRight, true>(value, 23, carry);
	XCTAssertEqual(value, 0xffff'ffff);
	XCTAssertNotEqual(carry, 0);

	// Test a long negative shift.
	value = 0x8000'0000;
	shift<ShiftType::ArithmeticRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0xffff'ffff);
	XCTAssertNotEqual(carry, 0);

	// Test a positive shift.
	value = 0x0123'0031;
	shift<ShiftType::ArithmeticRight, true>(value, 3, carry);
	XCTAssertEqual(value, 0x24'6006);
	XCTAssertEqual(carry, 0);
}

- (void)testBarrelShifterRotateRight {
	uint32_t value;
	uint32_t carry;

	// Test a short rotate by one hex digit.
	value = 0xabcd'1234;
	shift<ShiftType::RotateRight, true>(value, 4, carry);
	XCTAssertEqual(value, 0x4abc'd123);
	XCTAssertEqual(carry, 0);

	// Test a longer rotate, with carry.
	value = 0xa5f9'6342;
	shift<ShiftType::RotateRight, true>(value, 17, carry);
	XCTAssertEqual(value, 0xb1a1'52fc);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate by 32 without carry.
	value = 0x385f'7dce;
	shift<ShiftType::RotateRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0x385f'7dce);
	XCTAssertEqual(carry, 0);

	// Test a rotate by 32 with carry.
	value = 0xfecd'ba12;
	shift<ShiftType::RotateRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0xfecd'ba12);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate through carry, carry not set.
	value = 0x123f'abcf;
	carry = 0;
	shift<ShiftType::RotateRight, true>(value, 0, carry);
	XCTAssertEqual(value, 0x091f'd5e7);
	XCTAssertNotEqual(carry, 0);

	// Test a rotate through carry, carry set.
	value = 0x123f'abce;
	carry = 1;
	shift<ShiftType::RotateRight, true>(value, 0, carry);
	XCTAssertEqual(value, 0x891f'd5e7);
	XCTAssertEqual(carry, 0);
}

// TODO: turn the below into a trace-driven test case.
- (void)testROM319 {
	constexpr ROM::Name rom_name = ROM::Name::AcornRISCOS319;
	ROM::Request request(rom_name);
	const auto roms = CSROMFetcher()(request);

	auto executor = std::make_unique<Executor<Model::ARMv2, Memory>>();
	executor->bus.rom = roms.find(rom_name)->second;

	for(int c = 0; c < 1000; c++) {
		uint32_t instruction;
		executor->bus.read(executor->pc(), instruction, executor->registers().mode(), false);

		printf("%08x: %08x [", executor->pc(), instruction);
		for(int c = 0; c < 15; c++) {
			printf("r%d:%08x ", c, executor->registers().active[c]);
		}
		printf("psr:%08x]\n", executor->registers().status());
		execute<Model::ARMv2>(instruction, *executor);
	}
}

@end
