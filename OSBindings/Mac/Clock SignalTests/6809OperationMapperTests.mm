//
//  68000BCDTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 29/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "Dispatcher.hpp"
#include "InstructionSets/6809/OperationMapper.hpp"

#include <unordered_map>

using namespace InstructionSet::M6809;

namespace {

template <Page page> std::pair<Operation, AddressingMode> capture(const uint8_t opcode) {
	OperationReturner catcher;
	OperationMapper<page> mapper;
	return Reflection::dispatch(mapper, opcode, catcher);
}

}

@interface M6809OperationMapperTests : XCTestCase
@end

@implementation M6809OperationMapperTests

- (void)testOpcodeLengths {
	const std::unordered_map<uint16_t, int> lengths = {
		{0x00, 2},	{0x01, 1},	{0x02, 1}, 	{0x03, 2},	{0x04, 2},	{0x05, 1},	{0x06, 2}, 	{0x07, 2},
		{0x08, 2},	{0x09, 2},	{0x0a, 2}, 	{0x0b, 1},	{0x0c, 2},	{0x0d, 2},	{0x0e, 2}, 	{0x0f, 2},

		{0x10, 1},	{0x11, 1},	{0x12, 1}, 	{0x13, 1},	{0x14, 1},	{0x15, 1},	{0x16, 3}, 	{0x17, 3},
		{0x18, 1},	{0x19, 1},	{0x1a, 2}, 	{0x1b, 1},	{0x1c, 2},	{0x1d, 1},	{0x1e, 2}, 	{0x1f, 2},

		{0x20, 2},	{0x21, 2},	{0x22, 2}, 	{0x23, 2},	{0x24, 2},	{0x25, 2},	{0x26, 2}, 	{0x27, 2},
		{0x28, 2},	{0x29, 2},	{0x2a, 2}, 	{0x2b, 2},	{0x2c, 2},	{0x2d, 2},	{0x2e, 2}, 	{0x2f, 2},

		{0x30, 2},	{0x31, 2},	{0x32, 2}, 	{0x33, 2},	{0x34, 2},	{0x35, 2},	{0x36, 2}, 	{0x37, 2},
		{0x38, 1},	{0x39, 1},	{0x3a, 1}, 	{0x3b, 1},	{0x3c, 2},	{0x3d, 1},	{0x3e, 1}, 	{0x3f, 1},

		{0x40, 1},	{0x41, 1},	{0x42, 1}, 	{0x43, 1},	{0x44, 1},	{0x45, 1},	{0x46, 1}, 	{0x47, 1},
		{0x48, 1},	{0x49, 1},	{0x4a, 1}, 	{0x4b, 1},	{0x4c, 1},	{0x4d, 1},	{0x4e, 1}, 	{0x4f, 1},

		{0x50, 1},	{0x51, 1},	{0x52, 1}, 	{0x53, 1},	{0x54, 1},	{0x55, 1},	{0x56, 1}, 	{0x57, 1},
		{0x58, 1},	{0x59, 1},	{0x5a, 1}, 	{0x5b, 1},	{0x5c, 1},	{0x5d, 1},	{0x5e, 1}, 	{0x5f, 1},

		{0x60, 2},	{0x61, 1},	{0x62, 1}, 	{0x63, 2},	{0x64, 2},	{0x65, 1},	{0x66, 2}, 	{0x67, 2},
		{0x68, 2},	{0x69, 2},	{0x6a, 2}, 	{0x6b, 1},	{0x6c, 2},	{0x6d, 2},	{0x6e, 2}, 	{0x6f, 2},

		{0x70, 3},	{0x71, 1},	{0x72, 1}, 	{0x73, 3},	{0x74, 3},	{0x75, 1},	{0x76, 3}, 	{0x77, 3},
		{0x78, 3},	{0x79, 3},	{0x7a, 3}, 	{0x7b, 1},	{0x7c, 3},	{0x7d, 3},	{0x7e, 3}, 	{0x7f, 3},

		{0x80, 2},	{0x81, 2},	{0x82, 2}, 	{0x83, 3},	{0x84, 2},	{0x85, 2},	{0x86, 2}, 	{0x87, 1},
		{0x88, 2},	{0x89, 2},	{0x8a, 2}, 	{0x8b, 2},	{0x8c, 3},	{0x8d, 2},	{0x8e, 3}, 	{0x8f, 1},

		{0x90, 2},	{0x91, 2},	{0x92, 2}, 	{0x93, 2},	{0x94, 2},	{0x95, 2},	{0x96, 2}, 	{0x97, 2},
		{0x98, 2},	{0x99, 2},	{0x9a, 2}, 	{0x9b, 2},	{0x9c, 2},	{0x9d, 2},	{0x9e, 2}, 	{0x9f, 2},

		{0xa0, 2},	{0xa1, 2},	{0xa2, 2}, 	{0xa3, 2},	{0xa4, 2},	{0xa5, 2},	{0xa6, 2}, 	{0xa7, 2},
		{0xa8, 2},	{0xa9, 2},	{0xaa, 2}, 	{0xab, 2},	{0xac, 2},	{0xad, 2},	{0xae, 2}, 	{0xaf, 2},

		{0xb0, 3},	{0xb1, 3},	{0xb2, 3}, 	{0xb3, 3},	{0xb4, 3},	{0xb5, 3},	{0xb6, 3}, 	{0xb7, 3},
		{0xb8, 3},	{0xb9, 3},	{0xba, 3}, 	{0xbb, 3},	{0xbc, 3},	{0xbd, 3},	{0xbe, 3}, 	{0xbf, 3},

		{0xc0, 2},	{0xc1, 2},	{0xc2, 2}, 	{0xc3, 3},	{0xc4, 2},	{0xc5, 2},	{0xc6, 2}, 	{0xc7, 1},
		{0xc8, 2},	{0xc9, 2},	{0xca, 2}, 	{0xcb, 2},	{0xcc, 3},	{0xcd, 1},	{0xce, 3}, 	{0xcf, 1},

		{0xd0, 2},	{0xd1, 2},	{0xd2, 2}, 	{0xd3, 2},	{0xd4, 2},	{0xd5, 2},	{0xd6, 2}, 	{0xd7, 2},
		{0xd8, 2},	{0xd9, 2},	{0xda, 2}, 	{0xdb, 2},	{0xdc, 2},	{0xdd, 2},	{0xde, 2}, 	{0xdf, 2},

		{0xe0, 2},	{0xe1, 2},	{0xe2, 2}, 	{0xe3, 2},	{0xe4, 2},	{0xe5, 2},	{0xe6, 2}, 	{0xe7, 2},
		{0xe8, 2},	{0xe9, 2},	{0xea, 2}, 	{0xeb, 2},	{0xec, 2},	{0xed, 2},	{0xee, 2}, 	{0xef, 2},

		{0xf0, 3},	{0xf1, 3},	{0xf2, 3}, 	{0xf3, 3},	{0xf4, 3},	{0xf5, 3},	{0xf6, 3}, 	{0xf7, 3},
		{0xf8, 3},	{0xf9, 3},	{0xfa, 3}, 	{0xfb, 3},	{0xfc, 3},	{0xfd, 3},	{0xfe, 3}, 	{0xff, 3},
	};

	for(const auto &pair : lengths) {
		const auto decoded = [&] {
			switch(pair.first >> 8) {
				default: return capture<Page::Page0>(pair.first & 0xff);
				case 0x10: return capture<Page::Page1>(pair.first & 0xff);
				case 0x11: return capture<Page::Page2>(pair.first & 0xff);
			}
		} ();

		const auto expected_length = [&] {
			switch(decoded.second) {
				case AddressingMode::Direct:		return 2;
				case AddressingMode::Extended:		return 3;
				case AddressingMode::Illegal:		return 1;
				case AddressingMode::Inherent:		return 1;
				case AddressingMode::Relative8:		return 2;
				case AddressingMode::Relative16:	return 3;
				case AddressingMode::Immediate8:	return 2;
				case AddressingMode::Immediate16:	return 3;
				case AddressingMode::Variant:		return 1;
				case AddressingMode::Indexed:		return 2;
				default: __builtin_unreachable();
			}
		} () + bool(pair.first >> 8);

		XCTAssertEqual(expected_length, pair.second, " for opcode %02x", pair.first);
	}
};

- (void)
	testOpcode:(uint8_t)opcode
	page:(Page)page
	isOperation:(Operation)expectedOperation
	addressingMode:(AddressingMode)expectedMode
{
	const auto &[operation, mode] = [&]() {
		switch(page) {
			case Page::Page0:	return capture<Page::Page0>(opcode);
			case Page::Page1:	return capture<Page::Page1>(opcode);
			case Page::Page2:	return capture<Page::Page2>(opcode);
		}
	} ();
	XCTAssertEqual(expectedOperation, operation, "Operation didn't match for opcode 0x%02x in page %d", opcode, int(page));
	XCTAssertEqual(expectedMode, mode, "Mode didn't match for opcode 0x%02x in page %d", opcode, int(page));
}

- (void)testMap {
	// Spot tests only for now; will do for checking that code compiles and acts semi-reasonably, at least.
	[self testOpcode:0x3a page:Page::Page0 isOperation:Operation::ABX addressingMode:AddressingMode::Inherent];
	[self testOpcode:0xd9 page:Page::Page0 isOperation:Operation::ADCB addressingMode:AddressingMode::Direct];
	[self testOpcode:0xab page:Page::Page0 isOperation:Operation::ADDA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xf3 page:Page::Page0 isOperation:Operation::ADDD addressingMode:AddressingMode::Extended];
	[self testOpcode:0xc4 page:Page::Page0 isOperation:Operation::ANDB addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x1c page:Page::Page0 isOperation:Operation::ANDCC addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x08 page:Page::Page0 isOperation:Operation::LSL addressingMode:AddressingMode::Direct];
	[self testOpcode:0x57 page:Page::Page0 isOperation:Operation::ASRB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x24 page:Page::Page0 isOperation:Operation::BCC addressingMode:AddressingMode::Relative8];
	[self testOpcode:0xa5 page:Page::Page0 isOperation:Operation::BITA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x20 page:Page::Page0 isOperation:Operation::BRA addressingMode:AddressingMode::Relative8];
	[self testOpcode:0x8d page:Page::Page0 isOperation:Operation::BSR addressingMode:AddressingMode::Relative8];
	[self testOpcode:0x5f page:Page::Page0 isOperation:Operation::CLRB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x7f page:Page::Page0 isOperation:Operation::CLR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x81 page:Page::Page0 isOperation:Operation::CMPA addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0xa3 page:Page::Page1 isOperation:Operation::CMPD addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xa3 page:Page::Page2 isOperation:Operation::CMPU addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x53 page:Page::Page0 isOperation:Operation::COMB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x73 page:Page::Page0 isOperation:Operation::COM addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3c page:Page::Page0 isOperation:Operation::CWAI addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x19 page:Page::Page0 isOperation:Operation::DAA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x4a page:Page::Page0 isOperation:Operation::DECA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x0a page:Page::Page0 isOperation:Operation::DEC addressingMode:AddressingMode::Direct];
	[self testOpcode:0xa8 page:Page::Page0 isOperation:Operation::EORA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xf8 page:Page::Page0 isOperation:Operation::EORB addressingMode:AddressingMode::Extended];
	[self testOpcode:0x4c page:Page::Page0 isOperation:Operation::INCA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x0c page:Page::Page0 isOperation:Operation::INC addressingMode:AddressingMode::Direct];
	[self testOpcode:0x6e page:Page::Page0 isOperation:Operation::JMP addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x9d page:Page::Page0 isOperation:Operation::JSR addressingMode:AddressingMode::Direct];
	[self testOpcode:0x24 page:Page::Page1 isOperation:Operation::LBCC addressingMode:AddressingMode::Relative16];
	[self testOpcode:0xe6 page:Page::Page0 isOperation:Operation::LDB addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x8e page:Page::Page1 isOperation:Operation::LDY addressingMode:AddressingMode::Immediate16];
	[self testOpcode:0x32 page:Page::Page0 isOperation:Operation::LEAS addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x74 page:Page::Page0 isOperation:Operation::LSR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3d page:Page::Page0 isOperation:Operation::MUL addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x40 page:Page::Page0 isOperation:Operation::NEGA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x00 page:Page::Page0 isOperation:Operation::NEG addressingMode:AddressingMode::Direct];
	[self testOpcode:0x12 page:Page::Page0 isOperation:Operation::NOP addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x8a page:Page::Page0 isOperation:Operation::ORA addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x1a page:Page::Page0 isOperation:Operation::ORCC addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x34 page:Page::Page0 isOperation:Operation::PSHS addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x36 page:Page::Page0 isOperation:Operation::PSHU addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x37 page:Page::Page0 isOperation:Operation::PULU addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x59 page:Page::Page0 isOperation:Operation::ROLB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x69 page:Page::Page0 isOperation:Operation::ROL addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x76 page:Page::Page0 isOperation:Operation::ROR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3b page:Page::Page0 isOperation:Operation::RTI addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x39 page:Page::Page0 isOperation:Operation::RTS addressingMode:AddressingMode::Inherent];
	[self testOpcode:0xc2 page:Page::Page0 isOperation:Operation::SBCB addressingMode:AddressingMode::Immediate8];
	[self testOpcode:0x1d page:Page::Page0 isOperation:Operation::SEX addressingMode:AddressingMode::Inherent];
	[self testOpcode:0xe7 page:Page::Page0 isOperation:Operation::STB addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xbf page:Page::Page1 isOperation:Operation::STY addressingMode:AddressingMode::Extended];
	[self testOpcode:0xe0 page:Page::Page0 isOperation:Operation::SUBB addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x93 page:Page::Page0 isOperation:Operation::SUBD addressingMode:AddressingMode::Direct];
	[self testOpcode:0x3f page:Page::Page0 isOperation:Operation::SWI addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x3f page:Page::Page1 isOperation:Operation::SWI2 addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x3f page:Page::Page2 isOperation:Operation::SWI3 addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x13 page:Page::Page0 isOperation:Operation::SYNC addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x4d page:Page::Page0 isOperation:Operation::TSTA addressingMode:AddressingMode::Inherent];
}

@end
