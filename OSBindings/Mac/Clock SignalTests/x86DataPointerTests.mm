//
//  x86DataPointerTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 27/02/2022.
//  Copyright 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/x86/DataPointerResolver.hpp"

using namespace InstructionSet::x86;

@interface x86DataPointerTests : XCTestCase
@end

@implementation x86DataPointerTests

//- (InstructionSet::x86::Instruction<false>)instruction16WithSourceDataPointer:(DataPointer)pointer {
//	return x86::Instruction<false>{
//		InstructionSet::x86::Operation::AAA,
//		S
//	};
//}


- (void)testX {
	const DataPointer pointer(
		Source::eAX, Source::eDI, 0
	);

	struct Registers {
		uint16_t ax = 0x1234, di = 0x00ee;

		template <typename DataT, Register r> void write(DataT) {
			assert(false);
		}
		template <typename DataT, Register r> DataT read() {
			switch(r) {
				case Register::AX:	return ax;
				case Register::DI:	return di;
				default: return 0;
			}
		}
	} registers;

	struct Memory {
		template<typename DataT> DataT read(Source, uint32_t address) {
			if(address == 01234 + 0x00ee) return 0xff;
			return 0;
		}
		template<typename DataT> void write(Source, uint32_t, DataT) {
			assert(false);
		}

	} memory;

	const auto instruction = Instruction<false>();
	const uint8_t value = DataPointerResolver<
		Model::i8086, Registers, Memory>::read<uint8_t>(
			registers,
			memory,
			instruction,
			pointer
		);

	XCTAssertEqual(value, 0xff);
}

@end
