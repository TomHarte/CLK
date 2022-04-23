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

template <int index> NSString *operand(Preinstruction instruction, uint16_t opcode) {
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

		case AddressingMode::Quick:
			return [NSString stringWithFormat:@"%d", quick(instruction.operation, opcode)];
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

			case Operation::MOVEq:		instruction = @"MOVE.q";	break;

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

			case Operation::CMPb:	instruction = @"CMP.b";	break;
			case Operation::CMPw:	instruction = @"CMP.w";	break;
			case Operation::CMPl:	instruction = @"CMP.l";	break;

			case Operation::CMPAw:	instruction = @"CMPA.w";	break;
			case Operation::CMPAl:	instruction = @"CMPA.l";	break;

			case Operation::TSTb:	instruction = @"TST.b";	break;
			case Operation::TSTw:	instruction = @"TST.w";	break;
			case Operation::TSTl:	instruction = @"TST.l";	break;

			case Operation::JMP:	instruction = @"JMP";	break;
			case Operation::JSR:	instruction = @"JSR";	break;
			case Operation::RTS:	instruction = @"RTS";	break;
			case Operation::DBcc:	instruction = @"DBcc";	break;
			case Operation::Scc:	instruction = @"Scc";	break;

			case Operation::Bccb:
			case Operation::Bccl:
			case Operation::Bccw:	instruction = @"Bcc";	break;

			case Operation::BSRb:
			case Operation::BSRl:
			case Operation::BSRw:	instruction = @"BSR";	break;

			case Operation::CLRb:	instruction = @"CLR.b";	break;
			case Operation::CLRw:	instruction = @"CLR.w";	break;
			case Operation::CLRl:	instruction = @"CLR.l";	break;

			case Operation::NEGXb:	instruction = @"NEGX.b";	break;
			case Operation::NEGXw:	instruction = @"NEGX.w";	break;
			case Operation::NEGXl:	instruction = @"NEGX.l";	break;

			case Operation::NEGb:	instruction = @"NEG.b";	break;
			case Operation::NEGw:	instruction = @"NEG.w";	break;
			case Operation::NEGl:	instruction = @"NEG.l";	break;

			case Operation::ASLb:	instruction = @"ASL.b";	break;
			case Operation::ASLw:	instruction = @"ASL.w";	break;
			case Operation::ASLl:	instruction = @"ASL.l";	break;
			case Operation::ASLm:	instruction = @"ASL.w";	break;

			case Operation::ASRb:	instruction = @"ASR.b";	break;
			case Operation::ASRw:	instruction = @"ASR.w";	break;
			case Operation::ASRl:	instruction = @"ASR.l";	break;
			case Operation::ASRm:	instruction = @"ASR.w";	break;

			case Operation::LSLb:	instruction = @"LSL.b";	break;
			case Operation::LSLw:	instruction = @"LSL.w";	break;
			case Operation::LSLl:	instruction = @"LSL.l";	break;
			case Operation::LSLm:	instruction = @"LSL.w";	break;

			case Operation::LSRb:	instruction = @"LSR.b";	break;
			case Operation::LSRw:	instruction = @"LSR.w";	break;
			case Operation::LSRl:	instruction = @"LSR.l";	break;
			case Operation::LSRm:	instruction = @"LSR.w";	break;

			case Operation::ROLb:	instruction = @"ROL.b";	break;
			case Operation::ROLw:	instruction = @"ROL.w";	break;
			case Operation::ROLl:	instruction = @"ROL.l";	break;
			case Operation::ROLm:	instruction = @"ROL.w";	break;

			case Operation::RORb:	instruction = @"ROR.b";	break;
			case Operation::RORw:	instruction = @"ROR.w";	break;
			case Operation::RORl:	instruction = @"ROR.l";	break;
			case Operation::RORm:	instruction = @"ROR.w";	break;

			case Operation::ROXLb:	instruction = @"ROXL.b";	break;
			case Operation::ROXLw:	instruction = @"ROXL.w";	break;
			case Operation::ROXLl:	instruction = @"ROXL.l";	break;
			case Operation::ROXLm:	instruction = @"ROXL.w";	break;

			case Operation::ROXRb:	instruction = @"ROXR.b";	break;
			case Operation::ROXRw:	instruction = @"ROXR.w";	break;
			case Operation::ROXRl:	instruction = @"ROXR.l";	break;
			case Operation::ROXRm:	instruction = @"ROXR.w";	break;

			case Operation::MOVEMl:	instruction = @"MOVEM.l";	break;
			case Operation::MOVEMw:	instruction = @"MOVEM.w";	break;

			case Operation::MOVEPl:	instruction = @"MOVEP.l";	break;
			case Operation::MOVEPw:	instruction = @"MOVEP.w";	break;

			case Operation::ANDb:	instruction = @"AND.b";	break;
			case Operation::ANDw:	instruction = @"AND.w";	break;
			case Operation::ANDl:	instruction = @"AND.l";	break;

			case Operation::EORb:	instruction = @"EOR.b";	break;
			case Operation::EORw:	instruction = @"EOR.w";	break;
			case Operation::EORl:	instruction = @"EOR.l";	break;

			case Operation::NOTb:	instruction = @"NOT.b";	break;
			case Operation::NOTw:	instruction = @"NOT.w";	break;
			case Operation::NOTl:	instruction = @"NOT.l";	break;

			case Operation::ORb:	instruction = @"OR.b";	break;
			case Operation::ORw:	instruction = @"OR.w";	break;
			case Operation::ORl:	instruction = @"OR.l";	break;

			case Operation::MULU:	instruction = @"MULU";	break;
			case Operation::MULS:	instruction = @"MULS";	break;
			case Operation::DIVU:	instruction = @"DIVU";	break;
			case Operation::DIVS:	instruction = @"DIVS";	break;

			case Operation::RTE:	instruction = @"RTE";	break;
			case Operation::RTR:	instruction = @"RTR";	break;

			case Operation::TRAP:	instruction = @"TRAP";	break;
			case Operation::TRAPV:	instruction = @"TRAPV";	break;
			case Operation::CHK:	instruction = @"CHK";	break;

			case Operation::EXG:	instruction = @"EXG";	break;
			case Operation::SWAP:	instruction = @"SWAP";	break;

			case Operation::TAS:	instruction = @"TAS";	break;

			case Operation::EXTbtow:	instruction = @"EXT.w";		break;
			case Operation::EXTwtol:	instruction = @"EXT.l";		break;

			case Operation::LINKw:	instruction = @"LINK";		break;
			case Operation::UNLINK:	instruction = @"UNLINK";	break;

			case Operation::STOP:	instruction = @"STOP";		break;
			case Operation::RESET:	instruction = @"RESET";		break;

			// For now, skip any unmapped operations.
			default:
				XCTAssert(false, @"Operation %d unhandled by test case", int(found.operation));
			continue;
		}

		NSString *const operand1 = operand<0>(found, uint16_t(instr));
		NSString *const operand2 = operand<1>(found, uint16_t(instr));

		if(operand1.length) instruction = [instruction stringByAppendingFormat:@" %@", operand1];
		if(operand2.length) instruction = [instruction stringByAppendingFormat:@", %@", operand2];

		XCTAssertFalse(found.mode<0>() == AddressingMode::None && found.mode<1>() != AddressingMode::None, @"Decoding of %@ provided a second operand but not a first", instrName);
		XCTAssertEqualObjects(instruction, expected, "%@ should decode as %@; got %@", instrName, expected, instruction);
	}
}

@end
