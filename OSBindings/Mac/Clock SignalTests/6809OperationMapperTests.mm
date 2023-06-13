//
//  68000BCDTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 29/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "RangeDispatcher.hpp"
#include "../../../InstructionSets/6809/OperationMapper.hpp"

using namespace InstructionSet::M6809;

namespace {

struct OperationCapture {
	template <Operation operation, AddressingMode mode> void schedule() {
		this->operation = operation;
		this->mode = mode;
	}
	Operation operation;
	AddressingMode mode;
};

template <Page page> OperationCapture capture(uint8_t opcode) {
	OperationCapture catcher;
	OperationMapper<page> mapper;
	Reflection::dispatch(mapper, opcode, catcher);
	return catcher;
}

}

@interface M6809OperationMapperTests : XCTestCase
@end

@implementation M6809OperationMapperTests

- (void)testOpcode:(uint8_t)opcode page:(Page)page isOperation:(Operation)operation addressingMode:(AddressingMode)mode {
	OperationCapture catcher;

	switch(page) {
		case Page::Page0:	catcher = capture<Page::Page0>(opcode);	break;
		case Page::Page1:	catcher = capture<Page::Page1>(opcode);	break;
		case Page::Page2:	catcher = capture<Page::Page2>(opcode);	break;
	}

	XCTAssertEqual(catcher.operation, operation, "Operation didn't match for opcode 0x%02x in page %d", opcode, int(page));
	XCTAssertEqual(catcher.mode, mode, "Mode didn't match for opcode 0x%02x in page %d", opcode, int(page));
}

- (void)testMap {
	// Spot tests only for now; will do for checking that code compiles and acts semi-reasonably, at least.
	[self testOpcode:0x3a page:Page::Page0 isOperation:Operation::ABX addressingMode:AddressingMode::Inherent];
	[self testOpcode:0xd9 page:Page::Page0 isOperation:Operation::ADCB addressingMode:AddressingMode::Direct];
	[self testOpcode:0xab page:Page::Page0 isOperation:Operation::ADDA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xf3 page:Page::Page0 isOperation:Operation::ADDD addressingMode:AddressingMode::Extended];
	[self testOpcode:0xc4 page:Page::Page0 isOperation:Operation::ANDB addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x1c page:Page::Page0 isOperation:Operation::ANDCC addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x08 page:Page::Page0 isOperation:Operation::LSL addressingMode:AddressingMode::Direct];
	[self testOpcode:0x57 page:Page::Page0 isOperation:Operation::ASRB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x24 page:Page::Page0 isOperation:Operation::BCC addressingMode:AddressingMode::Relative];
	[self testOpcode:0xa5 page:Page::Page0 isOperation:Operation::BITA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x20 page:Page::Page0 isOperation:Operation::BRA addressingMode:AddressingMode::Relative];
	[self testOpcode:0x8d page:Page::Page0 isOperation:Operation::BSR addressingMode:AddressingMode::Relative];
	[self testOpcode:0x5f page:Page::Page0 isOperation:Operation::CLRB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x7f page:Page::Page0 isOperation:Operation::CLR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x81 page:Page::Page0 isOperation:Operation::CMPA addressingMode:AddressingMode::Immediate];
	[self testOpcode:0xa3 page:Page::Page1 isOperation:Operation::CMPD addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xa3 page:Page::Page2 isOperation:Operation::CMPU addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x53 page:Page::Page0 isOperation:Operation::COMB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x73 page:Page::Page0 isOperation:Operation::COM addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3c page:Page::Page0 isOperation:Operation::CWAI addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x19 page:Page::Page0 isOperation:Operation::DAA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x4a page:Page::Page0 isOperation:Operation::DECA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x0a page:Page::Page0 isOperation:Operation::DEC addressingMode:AddressingMode::Direct];
	[self testOpcode:0xa8 page:Page::Page0 isOperation:Operation::EORA addressingMode:AddressingMode::Indexed];
	[self testOpcode:0xf8 page:Page::Page0 isOperation:Operation::EORB addressingMode:AddressingMode::Extended];
	[self testOpcode:0x4c page:Page::Page0 isOperation:Operation::INCA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x0c page:Page::Page0 isOperation:Operation::INC addressingMode:AddressingMode::Direct];
	[self testOpcode:0x6e page:Page::Page0 isOperation:Operation::JMP addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x9d page:Page::Page0 isOperation:Operation::JSR addressingMode:AddressingMode::Direct];
	[self testOpcode:0x24 page:Page::Page1 isOperation:Operation::LBCC addressingMode:AddressingMode::Relative];
	[self testOpcode:0xe6 page:Page::Page0 isOperation:Operation::LDB addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x8e page:Page::Page1 isOperation:Operation::LDY addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x32 page:Page::Page0 isOperation:Operation::LEAS addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x74 page:Page::Page0 isOperation:Operation::LSR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3d page:Page::Page0 isOperation:Operation::MUL addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x40 page:Page::Page0 isOperation:Operation::NEGA addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x00 page:Page::Page0 isOperation:Operation::NEG addressingMode:AddressingMode::Direct];
	[self testOpcode:0x12 page:Page::Page0 isOperation:Operation::NOP addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x8a page:Page::Page0 isOperation:Operation::ORA addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x1a page:Page::Page0 isOperation:Operation::ORCC addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x34 page:Page::Page0 isOperation:Operation::PSHS addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x36 page:Page::Page0 isOperation:Operation::PSHU addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x37 page:Page::Page0 isOperation:Operation::PULU addressingMode:AddressingMode::Immediate];
	[self testOpcode:0x59 page:Page::Page0 isOperation:Operation::ROLB addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x69 page:Page::Page0 isOperation:Operation::ROL addressingMode:AddressingMode::Indexed];
	[self testOpcode:0x76 page:Page::Page0 isOperation:Operation::ROR addressingMode:AddressingMode::Extended];
	[self testOpcode:0x3b page:Page::Page0 isOperation:Operation::RTI addressingMode:AddressingMode::Inherent];
	[self testOpcode:0x39 page:Page::Page0 isOperation:Operation::RTS addressingMode:AddressingMode::Inherent];
	[self testOpcode:0xc2 page:Page::Page0 isOperation:Operation::SBCB addressingMode:AddressingMode::Immediate];
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
