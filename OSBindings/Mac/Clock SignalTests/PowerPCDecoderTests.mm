//
//  PowerPCDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/01/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Processors/Decoders/PowerPC/PowerPC.hpp"

namespace {
	using Operation = CPU::Decoder::PowerPC::Operation;
}

@interface PowerPCDecoderTests : XCTestCase
@end

/*!
	Tests PowerPC decoding by throwing a bunch of randomly-generated
	word streams and checking that the result matches what I got from a
	disassembler elsewhere.
*/
@implementation PowerPCDecoderTests {
	CPU::Decoder::PowerPC::Instruction instructions[32];
}

- (void)decode:(const uint32_t *)stream {
	CPU::Decoder::PowerPC::Decoder decoder(CPU::Decoder::PowerPC::Model::MPC601);
	for(int c = 0; c < 32; c++) {
		instructions[c] = decoder.decode(stream[c]);
	}
}

- (void)testStream1 {
	const uint32_t stream[] = {
		0x32eeefa5, 0xc2ee0786, 0x80ce552c, 0x88d5f02a,
		0xf8c2e801, 0xe83d5cdf, 0x7fa51fbb, 0xaacea8b0,
		0x7d4d4d21, 0x1314cf89, 0x47e0014b, 0xdf67566d,
		0xfb29a33e, 0x312cbf53, 0x706f1a15, 0xa87b7011,
		0x2107090c, 0xce04935e, 0x642e464a, 0x931e8eba,
		0xee396d5b, 0x4901183d, 0x31ccaa9a, 0x42b61a86,
		0x1ed4751d, 0x86af76e4, 0x151405a9, 0xca0ac015,
		0x60dd1f9d, 0xecff44f6, 0xf2c1110e, 0x9aa6653b,
	};
	[self decode:stream];

	// addic   r23,r14,-4187
	XCTAssertEqual(instructions[0].operation, Operation::addic);
	XCTAssertEqual(instructions[0].rD(), 23);
	XCTAssertEqual(instructions[0].rA(), 14);
	XCTAssertEqual(instructions[0].simm(), -4187);

	// lfs     f23,1926(r14)
	XCTAssertEqual(instructions[1].operation, Operation::lfs);
	XCTAssertEqual(instructions[1].frD(), 23);
	XCTAssertEqual(instructions[1].rA(), 14);
	XCTAssertEqual(instructions[1].d(), 1926);

	// lwz     r6,21804(r14)
	XCTAssertEqual(instructions[2].operation, Operation::lwz);
	XCTAssertEqual(instructions[2].rD(), 6);
	XCTAssertEqual(instructions[2].rA(), 14);
	XCTAssertEqual(instructions[2].d(), 21804);

	// lbz     r6,-4054(r21)
	XCTAssertEqual(instructions[3].operation, Operation::lbz);
	XCTAssertEqual(instructions[3].rD(), 6);
	XCTAssertEqual(instructions[3].rA(), 21);
	XCTAssertEqual(instructions[3].d(), -4054);

	// .long 0xf8c2e801
	// .long 0xe83d5cdf
	// .long 0x7fa51fbb
	XCTAssertEqual(instructions[4].operation, Operation::Undefined);
	XCTAssertEqual(instructions[5].operation, Operation::Undefined);
	XCTAssertEqual(instructions[6].operation, Operation::Undefined);

	// lha     r22,-22352(r14)
	XCTAssertEqual(instructions[7].operation, Operation::lha);
	XCTAssertEqual(instructions[7].rD(), 22);
	XCTAssertEqual(instructions[7].rA(), 14);
	XCTAssertEqual(instructions[7].d(), -22352);

	// .long 0x7d4d4d21
	// .long 0x1314cf89
	// .long 0x47e0014b
	XCTAssertEqual(instructions[8].operation, Operation::Undefined);
	XCTAssertEqual(instructions[9].operation, Operation::Undefined);
	// CLK decodes this as sc because it ignores reserved bits; the disassembler
	// I used checks the reserved bits. For now: don't test.
//	XCTAssertEqual(instructions[10].operation, Operation::Undefined);

	// stfdu   f27,22125(r7)
	XCTAssertEqual(instructions[11].operation, Operation::stfdu);
	XCTAssertEqual(instructions[11].frS(), 27);
	XCTAssertEqual(instructions[11].rA(), 7);
	XCTAssertEqual(instructions[11].d(), 22125);

	// .long 0xfb29a33e
	XCTAssertEqual(instructions[12].operation, Operation::Undefined);

	// addic   r9,r12,-16557
	XCTAssertEqual(instructions[13].operation, Operation::addic);
	XCTAssertEqual(instructions[13].rD(), 9);
	XCTAssertEqual(instructions[13].rA(), 12);
	XCTAssertEqual(instructions[13].simm(), -16557);

	// andi.   r15,r3,6677
	XCTAssertEqual(instructions[14].operation, Operation::andi_);
	XCTAssertEqual(instructions[14].rA(), 15);
	XCTAssertEqual(instructions[14].rS(), 3);
	XCTAssertEqual(instructions[14].uimm(), 6677);

	// lha     r3,28689(r27)
	XCTAssertEqual(instructions[15].operation, Operation::lha);
	XCTAssertEqual(instructions[15].rD(), 3);
	XCTAssertEqual(instructions[15].rA(), 27);
	XCTAssertEqual(instructions[15].d(), 28689);

	// subfic  r8,r7,2316
	XCTAssertEqual(instructions[16].operation, Operation::subfic);
	XCTAssertEqual(instructions[16].rD(), 8);
	XCTAssertEqual(instructions[16].rA(), 7);
	XCTAssertEqual(instructions[16].simm(), 2316);

	// lfdu    f16,-27810(r4)
	XCTAssertEqual(instructions[17].operation, Operation::lfdu);
	XCTAssertEqual(instructions[17].frD(), 16);
	XCTAssertEqual(instructions[17].rA(), 4);
	XCTAssertEqual(instructions[17].d(), -27810);

	// oris    r14,r1,17994
	XCTAssertEqual(instructions[18].operation, Operation::oris);
	XCTAssertEqual(instructions[18].rA(), 14);
	XCTAssertEqual(instructions[18].rS(), 1);
	XCTAssertEqual(instructions[18].uimm(), 17994);

	// stw     r24,-28998(r30)
	XCTAssertEqual(instructions[19].operation, Operation::stw);
	XCTAssertEqual(instructions[19].rS(), 24);
	XCTAssertEqual(instructions[19].rA(), 30);
	XCTAssertEqual(instructions[19].d(), -28998);

	// .long 0xee396d5b
	XCTAssertEqual(instructions[20].operation, Operation::Undefined);

	// bl      0x01011890		[disassmebled at address 0x54]
	XCTAssertEqual(instructions[21].operation, Operation::bx);
	XCTAssertEqual(instructions[21].li() + 0x54, 0x01011890);
	XCTAssertTrue(instructions[21].lk());
	XCTAssertFalse(instructions[21].aa());

	// addic   r14,r12,-21862
	XCTAssertEqual(instructions[22].operation, Operation::addic);
	XCTAssertEqual(instructions[22].rD(), 14);
	XCTAssertEqual(instructions[22].rA(), 12);
	XCTAssertEqual(instructions[22].simm(), -21862);

	// .long 0x42b61a86	[10101]
	XCTAssertEqual(instructions[23].operation, Operation::Undefined);

	// mulli   r22,r20,29981
	XCTAssertEqual(instructions[24].operation, Operation::mulli);
	XCTAssertEqual(instructions[24].rD(), 22);
	XCTAssertEqual(instructions[24].rA(), 20);
	XCTAssertEqual(instructions[24].simm(), 29981);

	// lwzu    r21,30436(r15)
	XCTAssertEqual(instructions[25].operation, Operation::lwzu);
	XCTAssertEqual(instructions[25].rD(), 21);
	XCTAssertEqual(instructions[25].rA(), 15);
	XCTAssertEqual(instructions[25].d(), 30436);

	// .long 0x151405a9
	XCTAssertEqual(instructions[26].operation, Operation::Undefined);

	// lfd     f16,-16363(r10)
	XCTAssertEqual(instructions[27].operation, Operation::lfd);
	XCTAssertEqual(instructions[27].frD(), 16);
	XCTAssertEqual(instructions[27].rA(), 10);
	XCTAssertEqual(instructions[27].d(), -16363);

	// ori     r29,r6,8093
	XCTAssertEqual(instructions[28].operation, Operation::ori);
	XCTAssertEqual(instructions[28].rA(), 29);
	XCTAssertEqual(instructions[28].rS(), 6);
	XCTAssertEqual(instructions[28].uimm(), 8093);

	// .long 0xecff44f6
	// .long 0xf2c1110e
	XCTAssertEqual(instructions[29].operation, Operation::Undefined);
	XCTAssertEqual(instructions[30].operation, Operation::Undefined);

	// stb     r21,25915(r6)
	XCTAssertEqual(instructions[31].operation, Operation::stb);
	XCTAssertEqual(instructions[31].rS(), 21);
	XCTAssertEqual(instructions[31].rA(), 6);
	XCTAssertEqual(instructions[31].d(), 25915);
}

@end
