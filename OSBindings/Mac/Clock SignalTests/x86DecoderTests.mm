//
//  x86DecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <initializer_list>
#include <vector>
#include "../../../Processors/Decoders/x86/x86.hpp"

namespace {
	using Operation = CPU::Decoder::x86::Operation;
	using Instruction = CPU::Decoder::x86::Instruction;
	using Source = CPU::Decoder::x86::Source;
	using Size = CPU::Decoder::x86::Size;
}

@interface x86DecoderTests : XCTestCase
@end

/*!
	Tests 8086 decoding by throwing a bunch of randomly-generated
	word streams and checking that the result matches what I got from a
	disassembler elsewhere.
*/
@implementation x86DecoderTests {
	std::vector<Instruction> instructions;
}

// MARK: - Specific instruction asserts.

- (void)assert:(Instruction &)instruction operation:(Operation)operation {
	XCTAssertEqual(instruction.operation, operation);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operation_size(), CPU::Decoder::x86::Size(size));
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size source:(Source)source destination:(Source)destination displacement:(int16_t)displacement {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operation_size(), CPU::Decoder::x86::Size(size));
	XCTAssertEqual(instruction.source(), source);
	XCTAssertEqual(instruction.destination(), destination);
	XCTAssertEqual(instruction.displacement(), displacement);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size source:(Source)source destination:(Source)destination displacement:(int16_t)displacement operand:(uint16_t)operand {
	[self assert:instruction operation:operation size:size source:source destination:destination displacement:displacement];
	XCTAssertEqual(instruction.operand(), operand);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size source:(Source)source destination:(Source)destination operand:(uint16_t)operand {
	[self assert:instruction operation:operation size:size source:source destination:destination displacement:0 operand:operand];
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size source:(Source)source destination:(Source)destination {
	[self assert:instruction operation:operation size:size source:source destination:destination displacement:0];
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size source:(Source)source {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operation_size(), CPU::Decoder::x86::Size(size));
	XCTAssertEqual(instruction.source(), source);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size destination:(Source)destination {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operation_size(), CPU::Decoder::x86::Size(size));
	XCTAssertEqual(instruction.destination(), destination);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation size:(int)size operand:(uint16_t)operand destination:(Source)destination {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operation_size(), CPU::Decoder::x86::Size(size));
	XCTAssertEqual(instruction.destination(), destination);
	XCTAssertEqual(instruction.source(), Source::Immediate);
	XCTAssertEqual(instruction.operand(), operand);
	XCTAssertEqual(instruction.displacement(), 0);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation displacement:(int16_t)displacement {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.displacement(), displacement);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation operand:(uint16_t)operand {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.operand(), operand);
	XCTAssertEqual(instruction.displacement(), 0);
}

// MARK: - Decoder

- (void)decode:(const std::initializer_list<uint8_t> &)stream {
	// Decode by offering up all data at once.
	CPU::Decoder::x86::Decoder decoder(CPU::Decoder::x86::Model::i8086);
	instructions.clear();
	const uint8_t *byte = stream.begin();
	while(byte != stream.end()) {
		const auto [size, next] = decoder.decode(byte, stream.end() - byte);
		if(size <= 0) break;
		instructions.push_back(next);
		byte += size;
	}

	// Grab a byte-at-a-time decoding and check that it matches the previous.
	{
		CPU::Decoder::x86::Decoder decoder(CPU::Decoder::x86::Model::i8086);

		auto previous_instruction = instructions.begin();
		for(auto item: stream) {
			const auto [size, next] = decoder.decode(&item, 1);
			if(size > 0) {
				XCTAssert(next == *previous_instruction);
				++previous_instruction;
			}
		}
	}
}

// MARK: - Tests

- (void)testSequence1 {
	// Sequences the Online Disassembler believes to exist but The 8086 Book does not:
	//
	// 0x6a 0x65	push $65
	// 0x65 0x6d	gs insw (%dx),%es:(%di)
	// 0x67 0x61	addr32 popa
	// 0x6c			insb (%dx), %es:(%di)
	// 0xc9			leave
	//
	[self decode:{
		0x2d, 0x77, 0xea, 0x72, 0xfc, 0x4b, 0xb5, 0x28, 0xc3, 0xca, 0x26, 0x48, /* 0x65, 0x6d, */ 0x7b, 0x9f,
		0xc2, 0x65, 0x42, 0x4e, 0xef, 0x70, 0x20, 0x94, 0xc4, 0xd4, 0x93, 0x43, 0x3c, 0x8e, /* 0x6a, 0x65, */
		0x1a, 0x78, 0x45, 0x10, 0x7f, 0x3c, 0x19, 0x5a, 0x16, 0x31, 0x64, 0x2c, 0xe7, 0xc6, 0x7d, 0xb0,
		0xb5, 0x49, /* 0x67, 0x61, */ 0xba, 0xc0, 0xcb, 0x14, 0x7e, 0x71, 0xd0, 0x50, 0x78, 0x3d, 0x03, 0x1d,
		0xe5, 0xc9, 0x97, 0xc3, 0x9b, 0xe6, 0xd3, /* 0x6c, */ 0x58, 0x4d, 0x76, 0x80, 0x44, 0xd6, 0x9f, 0xa5,
		0xbd, 0xa1, 0x12, 0xc5, 0x29, /* 0xc9, */ 0x9e, 0xd8, 0xf3, 0xcf, 0x92, 0x39, 0x5d, 0x90, 0x15, 0xc3,
		0xb8, 0xad, 0xe8, 0xc8, 0x16, 0x4a, 0xb0, 0x9e, 0xf9, 0xbf, 0x56, 0xea, 0x4e, 0xfd, 0xe4, 0x5a,
		0x23, 0xaa, 0x2c, 0x5b, 0x2a, 0xd2, 0xf7, 0x5f, 0x18, 0x86, 0x90, 0x25, 0x64, 0xb7, 0xc3
	}];

	// 63 instructions are expected.
	XCTAssertEqual(instructions.size(), 63);

	// sub    $0xea77,%ax
	// jb     0x00000001
	// dec    %bx
	// mov    $0x28,%ch
	[self assert:instructions[0] operation:Operation::SUB size:2 operand:0xea77 destination:Source::AX];
	[self assert:instructions[1] operation:Operation::JB displacement:0xfffc];
	[self assert:instructions[2] operation:Operation::DEC size:2 source:Source::BX destination:Source::BX];
	[self assert:instructions[3] operation:Operation::MOV size:1 operand:0x28 destination:Source::CH];

	// ret
	// lret   $0x4826
	// [[ omitted: gs insw (%dx),%es:(%di) ]]
	// jnp    0xffffffaf
	// ret    $0x4265
	[self assert:instructions[4] operation:Operation::RETN];
	[self assert:instructions[5] operation:Operation::RETF operand:0x4826];
	[self assert:instructions[6] operation:Operation::JNP displacement:0xff9f];
	[self assert:instructions[7] operation:Operation::RETN operand:0x4265];

	// dec    %si
	// out    %ax,(%dx)
	// jo     0x00000037
	// xchg   %ax,%sp
	[self assert:instructions[8] operation:Operation::DEC size:2 source:Source::SI destination:Source::SI];
	[self assert:instructions[9] operation:Operation::OUT size:2 source:Source::AX destination:Source::DX];
	[self assert:instructions[10] operation:Operation::JO displacement:0x20];
	[self assert:instructions[11] operation:Operation::XCHG size:2 source:Source::AX destination:Source::SP];

	// ODA has:
	// 	c4		(bad)
	// 	d4 93	aam    $0x93
	//
	// That assumes that upon discovering that the d4 doesn't make a valid LES,
	// it can become an instruction byte. I'm not persuaded. So I'm taking:
	//
	//	c4 d4	(bad)
	//	93		XCHG AX, BX
	[self assert:instructions[12] operation:Operation::Invalid];
	[self assert:instructions[13] operation:Operation::XCHG size:2 source:Source::AX destination:Source::BX];

	// inc    %bx
	// cmp    $0x8e,%al
	// [[ omitted: push   $0x65 ]]
	// sbb    0x45(%bx,%si),%bh
	// adc    %bh,0x3c(%bx)
	[self assert:instructions[14] operation:Operation::INC size:2 source:Source::BX destination:Source::BX];
	[self assert:instructions[15] operation:Operation::CMP size:1 operand:0x8e destination:Source::AL];
	[self assert:instructions[16] operation:Operation::SBB size:1 source:Source::IndBXPlusSI destination:Source::BH displacement:0x45];
	[self assert:instructions[17] operation:Operation::ADC size:1 source:Source::BH destination:Source::IndBX displacement:0x3c];

	// sbb    %bx,0x16(%bp,%si)
	// xor    %sp,0x2c(%si)
	// out    %ax,$0xc6
	// jge    0xffffffe0
	[self assert:instructions[18] operation:Operation::SBB size:2 source:Source::BX destination:Source::IndBPPlusSI displacement:0x16];
	[self assert:instructions[19] operation:Operation::XOR size:2 source:Source::SP destination:Source::IndSI displacement:0x2c];
	[self assert:instructions[20] operation:Operation::OUT size:2 source:Source::AX destination:Source::DirectAddress operand:0xc6];
	[self assert:instructions[21] operation:Operation::JNL displacement:0xffb0];

	// mov    $0x49,%ch
	// [[ omitted: addr32 popa ]]
	// mov    $0xcbc0,%dx
	// adc    $0x7e,%al
	// jno    0x0000000b
	[self assert:instructions[22] operation:Operation::MOV size:1 operand:0x49 destination:Source::CH];
	[self assert:instructions[23] operation:Operation::MOV size:2 operand:0xcbc0 destination:Source::DX];
	[self assert:instructions[24] operation:Operation::ADC size:1 operand:0x7e destination:Source::AL];
	[self assert:instructions[25] operation:Operation::JNO displacement:0xffd0];

	// push   %ax
	// js     0x0000007b
	// add    (%di),%bx
	// in     $0xc9,%ax
	[self assert:instructions[26] operation:Operation::PUSH size:2 source:Source::AX];
	[self assert:instructions[27] operation:Operation::JS displacement:0x3d];
	[self assert:instructions[28] operation:Operation::ADD size:2 source:Source::IndDI destination:Source::BX];
	[self assert:instructions[29] operation:Operation::IN size:2 source:Source::DirectAddress destination:Source::AX operand:0xc9];

	// xchg   %ax,%di
	// ret
	// fwait
	// out    %al,$0xd3
	[self assert:instructions[30] operation:Operation::XCHG size:2 source:Source::AX destination:Source::DI];
	[self assert:instructions[31] operation:Operation::RETN];
	[self assert:instructions[32] operation:Operation::WAIT];
	[self assert:instructions[33] operation:Operation::OUT size:1 source:Source::AL destination:Source::DirectAddress operand:0xd3];

	// [[ omitted: insb   (%dx),%es:(%di) ]]
	// pop    %ax
	// dec    %bp
	// jbe    0xffffffcc
	// inc    %sp
	[self assert:instructions[34] operation:Operation::POP size:2 destination:Source::AX];
	[self assert:instructions[35] operation:Operation::DEC size:2 source:Source::BP destination:Source::BP];
	[self assert:instructions[36] operation:Operation::JBE displacement:0xff80];
	[self assert:instructions[37] operation:Operation::INC size:2 source:Source::SP destination:Source::SP];

	// (bad)
	// lahf
	// movsw  %ds:(%si),%es:(%di)
	// mov    $0x12a1,%bp
	[self assert:instructions[38] operation:Operation::Invalid];
	[self assert:instructions[39] operation:Operation::LAHF];
	[self assert:instructions[40] operation:Operation::MOVS size:2];
	[self assert:instructions[41] operation:Operation::MOV size:2 operand:0x12a1 destination:Source::BP];

	// lds    (%bx,%di),%bp
	// [[ omitted: leave ]]
	// sahf
	// fdiv   %st(3),%st
	// iret
	[self assert:instructions[42] operation:Operation::LDS size:2];
	[self assert:instructions[43] operation:Operation::SAHF];
	[self assert:instructions[44] operation:Operation::ESC];
	[self assert:instructions[45] operation:Operation::IRET];

	// xchg   %ax,%dx
	// cmp    %bx,-0x70(%di)
	// adc    $0xb8c3,%ax
	// lods   %ds:(%si),%ax
	[self assert:instructions[46] operation:Operation::XCHG size:2 source:Source::AX destination:Source::DX];
	[self assert:instructions[47] operation:Operation::CMP size:2 source:Source::BX destination:Source::IndDI displacement:0xff90];
	[self assert:instructions[48] operation:Operation::ADC size:2 operand:0xb8c3 destination:Source::AX];
	[self assert:instructions[49] operation:Operation::LODS size:2];

	// call   0x0000172d
	// dec    %dx
	// mov    $0x9e,%al
	// stc
	[self assert:instructions[50] operation:Operation::CALLD operand:0x16c8];
	[self assert:instructions[51] operation:Operation::DEC size:2 source:Source::DX destination:Source::DX];
	[self assert:instructions[52] operation:Operation::MOV size:1 operand:0x9e destination:Source::AL];
	[self assert:instructions[53] operation:Operation::STC];

	// mov    $0xea56,%di
	// dec    %si
	// std
	// in     $0x5a,%al
	[self assert:instructions[54] operation:Operation::MOV size:2 operand:0xea56 destination:Source::DI];
	[self assert:instructions[55] operation:Operation::DEC size:2 source:Source::SI destination:Source::SI];
	[self assert:instructions[56] operation:Operation::STD];
	[self assert:instructions[57] operation:Operation::IN size:1 source:Source::DirectAddress destination:Source::AL operand:0x5a];

	// and    0x5b2c(%bp,%si),%bp
	// sub    %dl,%dl
	// negw   0x18(%bx)
	// xchg   %dl,0x6425(%bx,%si)
	[self assert:instructions[58] operation:Operation::AND size:2 source:Source::IndBPPlusSI destination:Source::BP displacement:0x5b2c];
	[self assert:instructions[59] operation:Operation::SUB size:1 source:Source::DL destination:Source::DL];
	[self assert:instructions[60] operation:Operation::NEG size:2 source:Source::IndBX destination:Source::IndBX displacement:0x18];
	[self assert:instructions[61] operation:Operation::XCHG size:1 source:Source::IndBXPlusSI destination:Source::DL displacement:0x6425];

	// mov    $0xc3,%bh
	[self assert:instructions[62] operation:Operation::MOV size:1 operand:0xc3 destination:Source::BH];
}

- (void)test83 {
	[self decode:{
		0x83, 0x10, 0x80,	// adcw   $0xff80,(%bx,%si)
		0x83, 0x3b, 0x04,	// cmpw   $0x4,(%bp,%di)
		0x83, 0x2f, 0x09,	// subw   $0x9,(%bx)
	}];

	// 68 instructions are expected.
	XCTAssertEqual(instructions.size(), 3);

	[self assert:instructions[0] operation:Operation::ADC size:2 source:Source::Immediate destination:Source::IndBXPlusSI operand:0xff80];
	[self assert:instructions[1] operation:Operation::CMP size:2 source:Source::Immediate destination:Source::IndBPPlusDI operand:0x4];
	[self assert:instructions[2] operation:Operation::SUB size:2 source:Source::Immediate destination:Source::IndBX operand:0x9];
}

@end
