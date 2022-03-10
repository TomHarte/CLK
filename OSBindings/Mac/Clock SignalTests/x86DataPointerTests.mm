//
//  x86DataPointerTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2022.
//  Copyright 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/x86/DataPointerResolver.hpp"
#include <map>

using namespace InstructionSet::x86;

@interface x86DataPointerTests : XCTestCase
@end

@implementation x86DataPointerTests

- (void)test16bitSize1 {
	const DataPointer indirectPointer(
		Source::eAX, Source::eDI, 0
	);
	const DataPointer registerPointer(
		Source::eBX
	);

	struct Registers {
		uint16_t ax = 0x1234, di = 0x00ee;
		uint8_t bl = 0xaa;

		template <typename DataT, Register r> DataT read() {
			assert(is_sized<DataT>(r));
			switch(r) {
				case Register::AX:	return ax;
				case Register::BL:	return bl;
				case Register::DI:	return di;
				default: return 0;
			}
		}
		template <typename DataT, Register r> void write(DataT value) {
			assert(is_sized<DataT>(r));
			switch(r) {
				case Register::BL:	bl = value;	break;
				default: assert(false);
			}
		}
	} registers;

	struct Memory {
		std::map<uint32_t, uint8_t> data;

		template<typename DataT> DataT read(Source, uint32_t address) {
			if(address == 0x1234 + 0x00ee) return 0xff;
			return 0;
		}
		template<typename DataT> void write(Source, uint32_t address, DataT value) {
			data[address] = value;
		}
	} memory;

	// TODO: construct this more formally; the code below just assumes size = 1, which is not a contractual guarantee.
	const auto instruction = Instruction<false>();

	using Resolver = DataPointerResolver<Model::i8086, Registers, Memory>;
	const uint8_t memoryValue = Resolver::read<uint8_t>(
			registers,
			memory,
			instruction,
			indirectPointer
		);
	registers.ax = 0x0100;
	Resolver::write<uint8_t>(
			registers,
			memory,
			instruction,
			indirectPointer,
			0xef
		);

	XCTAssertEqual(memoryValue, 0xff);
	XCTAssertEqual(memory.data[0x01ee], 0xef);

	const uint8_t registerValue = Resolver::read<uint8_t>(
			registers,
			memory,
			instruction,
			registerPointer
		);
	Resolver::write<uint8_t>(
			registers,
			memory,
			instruction,
			registerPointer,
			0x93
		);

	XCTAssertEqual(registerValue, 0xaa);
	XCTAssertEqual(registers.bl, 0x93);
}

@end
