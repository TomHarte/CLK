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
}

@interface x86DecoderTests : XCTestCase
@end

/*!
	Tests PowerPC decoding by throwing a bunch of randomly-generated
	word streams and checking that the result matches what I got from a
	disassembler elsewhere.
*/
@implementation x86DecoderTests {
	std::vector<Instruction> instructions;
}

// MARK: - Specific instruction asserts.

/* ... TODO ... */

// MARK: - Decoder

- (void)decode:(const std::initializer_list<uint8_t> &)stream {
	CPU::Decoder::x86::Decoder decoder(CPU::Decoder::x86::Model::i8086);

	// TODO: test that byte-at-a-time decoding gives the same results, as a freebie.
//	instructions.clear();
//	for(auto item: stream) {
//		const auto next = decoder.decode(&item, 1);
//		if(next.size() > 0) {
//			instructions.push_back(next);
//		}
//	}

	instructions.clear();
	const uint8_t *byte = stream.begin();
	while(byte != stream.end()) {
		const auto [size, next] = decoder.decode(byte, stream.end() - byte);
		if(size <= 0) break;
		instructions.push_back(next);
		byte += size;
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

	// 68 instructions are expected.
	XCTAssertEqual(instructions.size(), 63);

	// sub    $0xea77,%ax
	// jb     0x00000001
	// dec    %bx
	// mov    $0x28,%ch
	// ret
	// lret   $0x4826
	// gs insw (%dx),%es:(%di)
	// jnp    0xffffffaf
	// ret    $0x4265
	// dec    %si
	// out    %ax,(%dx)
	// jo     0x00000037
	// xchg   %ax,%sp
	// (bad)
	// aam    $0x93
	// inc    %bx
	// cmp    $0x8e,%al
	// push   $0x65
	// sbb    0x45(%bx,%si),%bh
	// adc    %bh,0x3c(%bx)
	// sbb    %bx,0x16(%bp,%si)
	// xor    %sp,0x2c(%si)
	// out    %ax,$0xc6
	// jge    0xffffffe0
	// mov    $0x49,%ch
	// addr32 popa
	// mov    $0xcbc0,%dx
	// adc    $0x7e,%al
	// jno    0x0000000b
	// push   %ax
	// js     0x0000007b
	// add    (%di),%bx
	// in     $0xc9,%ax
	// xchg   %ax,%di
	// ret
	// fwait
	// out    %al,$0xd3
	// insb   (%dx),%es:(%di)
	// pop    %ax
	// dec    %bp
	// jbe    0xffffffcc
	// inc    %sp
	// (bad)
	// lahf
	// movsw  %ds:(%si),%es:(%di)
	// mov    $0x12a1,%bp
	// lds    (%bx,%di),%bp
	// leave
	// sahf
	// fdiv   %st(3),%st
	// iret
	// xchg   %ax,%dx
	// cmp    %bx,-0x70(%di)
	// adc    $0xb8c3,%ax
	// lods   %ds:(%si),%ax
	// call   0x0000172d
	// dec    %dx
	// mov    $0x9e,%al
	// stc
	// mov    $0xea56,%di
	// dec    %si
	// std
	// in     $0x5a,%al
	// and    0x5b2c(%bp,%si),%bp
	// sub    %dl,%dl
	// negw   0x18(%bx)
	// xchg   %dl,0x6425(%bx,%si)
	// mov    $0xc3,%bh
}

@end
