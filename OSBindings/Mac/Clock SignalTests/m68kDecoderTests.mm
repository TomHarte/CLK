//
//  m68kDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/04/2022.
//  Copyright 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/68k/Decoder.hpp"

using namespace InstructionSet::M68k;

@interface m68kDecoderTests : XCTestCase
@end

namespace {

template <int index> NSString *operand(Preinstruction instruction) {
	switch(instruction.mode<index>()) {
		default:	return [NSString stringWithFormat:@"[Mode %d?]", int(instruction.mode<index>())];

		case AddressingMode::None:
			return @"";

		case AddressingMode::DataRegisterDirect:
			return [NSString stringWithFormat:@"D%d", instruction.reg<index>()];

		case AddressingMode::AddressRegisterDirect:
			return [NSString stringWithFormat:@"A%d", instruction.reg<index>()];
		case AddressingMode::AddressRegisterIndirect:
			return [NSString stringWithFormat:@"(A%d)", instruction.reg<index>()];
		case AddressingMode::AddressRegisterIndirectWithPostincrement:
			return [NSString stringWithFormat:@"(A%d)+", instruction.reg<index>()];
		case AddressingMode::AddressRegisterIndirectWithPredecrement:
			return [NSString stringWithFormat:@"-(A%d)", instruction.reg<index>()];
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			return [NSString stringWithFormat:@"(d16, A%d)", instruction.reg<index>()];
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			return [NSString stringWithFormat:@"(d8, A%d, Xn)", instruction.reg<index>()];

		case AddressingMode::ProgramCounterIndirectWithDisplacement:
			return @"(d16, PC)";
		case AddressingMode::ProgramCounterIndirectWithIndex8bitDisplacement:
			return @"(d8, PC, Xn)";

		case AddressingMode::AbsoluteShort:
			return @"(xxx).w";
		case AddressingMode::AbsoluteLong:
			return @"(xxx).l";

		case AddressingMode::ImmediateData:
			return @"#";
	}
}

}

@implementation m68kDecoderTests

- (void)testInstructionSpecs {
	NSData *const testData =
		[NSData dataWithContentsOfURL:
			[[NSBundle bundleForClass:[self class]]
				URLForResource:@"68000ops"
				withExtension:@"json"
				subdirectory:@"68000 Decoding"]];

	NSDictionary<NSString *, NSString *> *const decodings = [NSJSONSerialization JSONObjectWithData:testData options:0 error:nil];
	XCTAssertNotNil(decodings);

	Predecoder<Model::M68000> decoder;
	for(int instr = 0; instr < 65536; instr++) {
		NSString *const instrName = [NSString stringWithFormat:@"%04x", instr];
		NSString *const expected = decodings[instrName];
		XCTAssertNotNil(expected);

		const auto found = decoder.decode(uint16_t(instr));

		NSString *instruction;
		switch(found.operation) {
			case Operation::Undefined:	instruction = @"None";		break;
			case Operation::NOP:		instruction = @"NOP";		break;
			case Operation::ABCD:		instruction = @"ABCD";		break;
			case Operation::SBCD:		instruction = @"SBCD";		break;
			case Operation::NBCD:		instruction = @"NBCD";		break;

			case Operation::ADDb:		instruction = @"ADD.b";		break;
			case Operation::ADDw:		instruction = @"ADD.w";		break;
			case Operation::ADDl:		instruction = @"ADD.l";		break;

			case Operation::ADDAw:		instruction = @"ADDA.w";	break;
			case Operation::ADDAl:		instruction = @"ADDA.l";	break;

			case Operation::ADDXb:		instruction = @"ADDX.b";	break;
			case Operation::ADDXw:		instruction = @"ADDX.w";	break;
			case Operation::ADDXl:		instruction = @"ADDX.l";	break;

			case Operation::SUBb:		instruction = @"SUB.b";		break;
			case Operation::SUBw:		instruction = @"SUB.w";		break;
			case Operation::SUBl:		instruction = @"SUB.l";		break;

			case Operation::SUBAw:		instruction = @"SUBA.w";	break;
			case Operation::SUBAl:		instruction = @"SUBA.l";	break;

			case Operation::SUBXb:		instruction = @"SUBX.b";	break;
			case Operation::SUBXw:		instruction = @"SUBX.w";	break;
			case Operation::SUBXl:		instruction = @"SUBX.l";	break;

			case Operation::MOVEb:		instruction = @"MOVE.b";	break;
			case Operation::MOVEw:		instruction = @"MOVE.w";	break;
			case Operation::MOVEl:		instruction = @"MOVE.l";	break;

			case Operation::MOVEAw:		instruction = @"MOVEA.w";	break;
			case Operation::MOVEAl:		instruction = @"MOVEA.l";	break;

			case Operation::MOVEq:		instruction = @"MOVEq";		break;

			case Operation::LEA:		instruction = @"LEA";		break;
			case Operation::PEA:		instruction = @"PEA";		break;

			case Operation::MOVEtoSR:		instruction = @"MOVEtoSR";		break;
			case Operation::MOVEfromSR:		instruction = @"MOVEfromSR";	break;
			case Operation::MOVEtoCCR:		instruction = @"MOVEtoCCR";		break;
			case Operation::MOVEtoUSP:		instruction = @"MOVEtoUSP";		break;
			case Operation::MOVEfromUSP:	instruction = @"MOVEfromUSP";	break;

			case Operation::ORItoSR:	instruction = @"ORItoSR";	break;
			case Operation::ORItoCCR:	instruction = @"ORItoCCR";	break;
			case Operation::ANDItoSR:	instruction = @"ANDItoSR";	break;
			case Operation::ANDItoCCR:	instruction = @"ANDItoCCR";	break;
			case Operation::EORItoSR:	instruction = @"EORItoSR";	break;
			case Operation::EORItoCCR:	instruction = @"EORItoCCR";	break;

			case Operation::BTST:	instruction = @"BTST";	break;
			case Operation::BCLR:	instruction = @"BCLR";	break;
			case Operation::BCHG:	instruction = @"BCHG";	break;
			case Operation::BSET:	instruction = @"BSET";	break;

			// For now, skip any unmapped operations.
			default: continue;
		}

		NSString *const operand1 = operand<0>(found);
		NSString *const operand2 = operand<1>(found);

		if(operand1.length) instruction = [instruction stringByAppendingFormat:@" %@", operand1];
		if(operand2.length) instruction = [instruction stringByAppendingFormat:@", %@", operand2];

		// Quick decoding not yet supported. TODO.
		if(found.mode<0>() == AddressingMode::Quick || found.mode<1>() == AddressingMode::Quick) {
			continue;
		}

		XCTAssertFalse(found.mode<0>() == AddressingMode::None && found.mode<1>() != AddressingMode::None, @"Decoding of %@ provided a second operand but not a first", instrName);
		XCTAssertEqualObjects(instruction, expected, "%@ should decode as %@; got %@", instrName, expected, instruction);
	}

}

@end
