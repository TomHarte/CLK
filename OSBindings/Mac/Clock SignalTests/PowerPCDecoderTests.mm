//
//  PowerPCDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/01/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/PowerPC/Decoder.hpp"

namespace {
	using Operation = InstructionSet::PowerPC::Operation;
	using Instruction = InstructionSet::PowerPC::Instruction;
}

@interface PowerPCDecoderTests : XCTestCase
@end

/*!
	Tests PowerPC decoding by throwing a bunch of randomly-generated
	word streams and checking that the result matches what I got from a
	disassembler elsewhere.
*/
@implementation PowerPCDecoderTests {
	Instruction instructions[32];
}

// MARK: - Specific instruction asserts.

- (void)assertUndefined:(Instruction &)instruction {
	XCTAssertEqual(instruction.operation, Operation::Undefined);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation rD:(int)rD rA:(int)rA simm:(int)simm {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.rD(), rD);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.simm(), simm);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation rD:(int)rD rA:(int)rA d:(int)d {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.rD(), rD);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.d(), d);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation rA:(int)rA rS:(int)rS uimm:(int)uimm {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.rS(), rS);
	XCTAssertEqual(instruction.uimm(), uimm);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation rS:(int)rS rA:(int)rA d:(int)d {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.rS(), rS);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.d(), d);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation frD:(int)frD rA:(int)rA d:(int)d {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.frD(), frD);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.d(), d);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation frS:(int)frS rA:(int)rA d:(int)d {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.frS(), frS);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.d(), d);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation li:(uint32_t)li lk:(BOOL)lk aa:(BOOL)aa {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.li(), li);
	XCTAssertEqual(!!instruction.lk(), lk);
	XCTAssertEqual(!!instruction.aa(), aa);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation bo:(int)bo bi:(int)bi bd:(int)bd lk:(BOOL)lk aa:(BOOL)aa {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.bo(), bo);
	XCTAssertEqual(instruction.bi(), bi);
	XCTAssertEqual(instruction.bd(), bd);
	XCTAssertEqual(!!instruction.lk(), lk);
	XCTAssertEqual(!!instruction.aa(), aa);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation rA:(int)rA rS:(int)rS rB:(int)rB mb:(int)mb me:(int)me rc:(BOOL)rc {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.rS(), rS);
	XCTAssertEqual(instruction.rB(), rB);
	XCTAssertEqual(instruction.mb(), mb);
	XCTAssertEqual(instruction.me(), me);
	XCTAssertEqual(!!instruction.rc(), rc);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation to:(int)to rA:(int)rA simm:(int)simm {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.to(), to);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.simm(), simm);
}

- (void)assert:(Instruction &)instruction operation:(Operation)operation crfD:(int)crfD l:(BOOL)l rA:(int)rA uimm:(int)uimm {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.crfD(), crfD);
	XCTAssertEqual(!!instruction.l(), l);
	XCTAssertEqual(instruction.rA(), rA);
	XCTAssertEqual(instruction.uimm(), uimm);
}

// MARK: - Decoder

- (void)decode:(const uint32_t *)stream {
	InstructionSet::PowerPC::Decoder decoder(InstructionSet::PowerPC::Model::MPC601);
	for(int c = 0; c < 32; c++) {
		instructions[c] = decoder.decode(stream[c]);
	}
}

// MARK: - Tests

- (void)testSequence1 {
	const uint32_t sequence[] = {
		0x32eeefa5, 0xc2ee0786, 0x80ce552c, 0x88d5f02a,
		0xf8c2e801, 0xe83d5cdf, 0x7fa51fbb, 0xaacea8b0,
		0x7d4d4d21, 0x1314cf89, 0x47e0014b, 0xdf67566d,
		0xfb29a33e, 0x312cbf53, 0x706f1a15, 0xa87b7011,
		0x2107090c, 0xce04935e, 0x642e464a, 0x931e8eba,
		0xee396d5b, 0x4901183d, 0x31ccaa9a, 0x42b61a86,
		0x1ed4751d, 0x86af76e4, 0x151405a9, 0xca0ac015,
		0x60dd1f9d, 0xecff44f6, 0xf2c1110e, 0x9aa6653b,
	};
	[self decode:sequence];

	// addic	r23,r14,-4187
	// lfs		f23,1926(r14)
	// lwz		r6,21804(r14)
	// lbz		r6,-4054(r21)
	[self assert:instructions[0] operation:Operation::addic rD:23 rA:14 simm:-4187];
	[self assert:instructions[1] operation:Operation::lfs frD:23 rA:14 d:1926];
	[self assert:instructions[2] operation:Operation::lwz rD:6 rA:14 d:21804];
	[self assert:instructions[3] operation:Operation::lbz rD:6 rA:21 d:-4054];

	// .long 0xf8c2e801
	// .long 0xe83d5cdf
	// .long 0x7fa51fbb
	// lha		r22,-22352(r14)
	[self assertUndefined:instructions[4]];
	[self assertUndefined:instructions[5]];
	[self assertUndefined:instructions[6]];
	[self assert:instructions[7] operation:Operation::lha rD:22 rA:14 d:-22352];

	// .long 0x7d4d4d21
	// .long 0x1314cf89
	// .long 0x47e0014b
	[self assertUndefined:instructions[8]];
	[self assertUndefined:instructions[9]];
	// CLK decodes this as sc because it ignores reserved bits; the disassembler
	// I used checks the reserved bits. For now: don't test.
//	XCTAssertEqual(instructions[10].operation, Operation::Undefined);

	// stfdu	f27,22125(r7)
	// .long 0xfb29a33e
	// addic	r9,r12,-16557
	// andi.	r15,r3,6677
	[self assert:instructions[11] operation:Operation::stfdu frS:27 rA:7 d:22125];
	[self assertUndefined:instructions[12]];
	[self assert:instructions[13] operation:Operation::addic rD:9 rA:12 simm:-16557];
	[self assert:instructions[14] operation:Operation::andi_ rA:15 rS:3 uimm:6677];

	// lha		r3,28689(r27)
	// subfic	r8,r7,2316
	// lfdu		f16,-27810(r4)
	// oris		r14,r1,17994
	[self assert:instructions[15] operation:Operation::lha rD:3 rA:27 d:28689];
	[self assert:instructions[16] operation:Operation::subfic rD:8 rA:7 simm:2316];
	[self assert:instructions[17] operation:Operation::lfdu frD:16 rA:4 d:-27810];
	[self assert:instructions[18] operation:Operation::oris rA:14 rS:1 uimm:17994];

	// stw		r24,-28998(r30)
	// .long 0xee396d5b
	// bl		0x01011890		[disassmebled at address 0x54]
	// addic	r14,r12,-21862
	[self assert:instructions[19] operation:Operation::stw rS:24 rA:30 d:-28998];
	[self assertUndefined:instructions[20]];
	[self assert:instructions[21] operation:Operation::bx li:0x01011890 - 0x54 lk:TRUE aa:FALSE];
	[self assert:instructions[22] operation:Operation::addic rD:14 rA:12 simm:-21862];

	// .long 0x42b61a86	[10101]
	// mulli	r22,r20,29981
	// lwzu		r21,30436(r15)
	// .long 0x151405a9
	[self assertUndefined:instructions[23]];
	[self assert:instructions[24] operation:Operation::mulli rD:22 rA:20 simm:29981];
	[self assert:instructions[25] operation:Operation::lwzu rD:21 rA:15 d:30436];
	[self assertUndefined:instructions[26]];

	// lfd		f16,-16363(r10)
	// ori		r29,r6,8093
	// .long 0xecff44f6
	// .long 0xf2c1110e
	// stb		r21,25915(r6)
	[self assert:instructions[27] operation:Operation::lfd frD:16 rA:10 d:-16363];
	[self assert:instructions[28] operation:Operation::ori rA:29 rS:6 uimm:8093];
	[self assertUndefined:instructions[29]];
	[self assertUndefined:instructions[30]];
	[self assert:instructions[31] operation:Operation::stb rS:21 rA:6 d:25915];
}

- (void)testSequence2 {
	const uint32_t sequence[] = {
		0x90252dae, 0x7429ee14, 0x618935bc, 0xd6c94af0,
		0xba1d295f, 0x649e3869, 0x6def742c, 0x5c64cdce,
		0x762d59ee, 0x565c8189, 0xc7c59f81, 0xce1157fd,
		0xc86aef59, 0x81325882, 0x1336fad6, 0xe1ddfa2b,
		0x18c60357, 0x4c122cb5, 0xccb1f749, 0xdbdcebc3,
		0x0fc60187, 0x117eb911, 0x80334c43, 0xe65371e8,
		0xa047c94d, 0xe671dd0b, 0xe07992bb, 0x6a332fe8,
		0xfc361c6b, 0x5e8b5a28, 0xb2b64a22, 0x045dd156,
	};
	[self decode:sequence];

	// stw		r1,11694(r5)
	// andis.	r9,r1,60948
	// ori		r9,r12,13756
	// stfsu	f22,19184(r9)
	[self assert:instructions[0] operation:Operation::stw rS:1 rA:5 d:11694];
	[self assert:instructions[1] operation:Operation::andis_ rA:9 rS:1 uimm:60948];
	[self assert:instructions[2] operation:Operation::ori rA:9 rS:12 uimm:13756];
	[self assert:instructions[3] operation:Operation::stfsu frS:22 rA:9 d:19184];

	// lmw		r16,10591(r29)
	// oris		r30,r4,14441
	// xoris	r15,r15,29740
	// rlwnm	r4,r3,r25,23,7
	[self assert:instructions[4] operation:Operation::lmw rD:16 rA:29 d:10591];
	[self assert:instructions[5] operation:Operation::oris rA:30 rS:4 uimm:14441];
	[self assert:instructions[6] operation:Operation::xoris rA:15 rS:15 uimm:29740];
	[self assert:instructions[7] operation:Operation::rlwnmx rA:4 rS:3 rB:25 mb:23 me:7 rc:FALSE];

	// andis.	r13,r17,23022
	// rlwinm.	r28,r18,16,6,4
	// lfsu		f30,-24703(r5)
	// lfdu		f16,22525(r17)
	[self assert:instructions[8] operation:Operation::andis_ rA:13 rS:17 uimm:23022];
	[self assert:instructions[9] operation:Operation::rlwinmx rA:28 rS:18 rB:16 mb:6 me:4 rc:TRUE];
	[self assert:instructions[10] operation:Operation::lfsu frD:30 rA:5 d:-24703];
	[self assert:instructions[11] operation:Operation::lfdu frD:16 rA:17 d:22525];

	// lfd		f3,-4263(r10)
	// lwz		r9,22658(r18)
	// .long 0x1336fad6
	// .long 0xe1ddfa2b
	[self assert:instructions[12] operation:Operation::lfd frD:3 rA:10 d:-4263];
	[self assert:instructions[13] operation:Operation::lwz rD:9 rA:18 d:22658];
	[self assertUndefined:instructions[14]];
	[self assertUndefined:instructions[15]];

	// .long 0x18c60357
	// .long 0x4c122cb5
	// lfdu		f5,-2231(r17)
	// stfd		f30,-5181(r28)
	[self assertUndefined:instructions[16]];
	[self assertUndefined:instructions[17]];
	[self assert:instructions[18] operation:Operation::lfdu frD:5 rA:17 d:-2231];
	[self assert:instructions[19] operation:Operation::stfd frD:30 rA:28 d:-5181];

	// twi		30,r6,391
	// .long 0x117eb911
	// lwz		r1,19523(r19)
	// .long 0xe65371e8
	[self assert:instructions[20] operation:Operation::twi to:30 rA:6 simm:391];
	[self assertUndefined:instructions[21]];
	[self assert:instructions[22] operation:Operation::lwz rD:1 rA:19 d:19523];
	[self assertUndefined:instructions[23]];

	// lhz		r2,-14003(r7)
	// .long 0xe671dd0b
	// .long 0xe07992bb
	// xori		r19,r17,12264
	[self assert:instructions[24] operation:Operation::lhz rD:2 rA:7 d:-14003];
	[self assertUndefined:instructions[25]];
	[self assertUndefined:instructions[26]];
	[self assert:instructions[27] operation:Operation::xori rA:19 rS:17 uimm:12264];

	// .long 0xfc361c6b
	// rlwnm	r11,r20,r11,8,20
	// sth		r21,18978(r22)
	// .long 0x45dd156
//	[self assertUndefined:instructions[28]];	// Disabled due to reserved field; I'm decoding this as faddx.
	[self assert:instructions[29] operation:Operation::rlwnmx rA:11 rS:20 rB:11 mb:8 me:20 rc:FALSE];
	[self assert:instructions[30] operation:Operation::sth rS:21 rA:22 d:18978];
	[self assertUndefined:instructions[31]];
}

- (void)testSequence3 {
	const uint32_t sequence[] = {
		0xbcaf3520, 0xfa9df12d, 0xc631efca, 0xa3e7f409,
		0x3ddca273, 0x3cfb234d, 0x551dc325, 0x8c1a0f37,
		0x5b3ca99b, 0xce08cc1e, 0x7b1dfd3a, 0xf19aee7c,
		0x52c852e9, 0xc681c0c1, 0xd3b1fda5, 0xe2b401cb,
		0x433cb83d, 0x54412f41, 0x532d624a, 0x0b3117c5,
		0x988144ba, 0xc7a96ad0, 0x28331474, 0x5620c367,
		0xab0a2607, 0xe826acf4, 0x41969154, 0x6471d09f,
		0x6a25f04f, 0x4a15996d, 0x272c96ef, 0xab3171a9,
	};
	[self decode:sequence];

	// stmw		r5,13600(r15)
	// .long 0xfa9df12d
	// lfsu		f17,-4150(r17)
	// lhz		r31,-3063(r7)
	[self assert:instructions[0] operation:Operation::stmw rS:5 rA:15 d:13600];
	[self assertUndefined:instructions[1]];
	[self assert:instructions[2] operation:Operation::lfsu frD:17 rA:17 d:-4150];
	[self assert:instructions[3] operation:Operation::lhz rD:31 rA:7 d:-3063];

	// addis	r14,r28,-23949
	// addis	r7,r27,9037
	// rlwinm.	r29,r8,24,12,18
	// lbzu		r0,3895(r26)
	[self assert:instructions[4] operation:Operation::addis rD:14 rA:28 simm:-23949];
	[self assert:instructions[5] operation:Operation::addis rD:7 rA:27 simm:9037];
	[self assert:instructions[6] operation:Operation::rlwinmx rA:29 rS:8 rB:24 mb:12 me:18 rc:TRUE];
	[self assert:instructions[7] operation:Operation::lbzu rD:0 rA:26 d:3895];

	// rlmi.	r28,r25,r21,6,13
	// lfdu		f16,-13282(r8)
	// .long 0x7b1dfd3a
	// .long 0xf19aee7c
	[self assert:instructions[8] operation:Operation::rlmix rA:28 rS:25 rB:21 mb:6 me:13 rc:TRUE];
	[self assert:instructions[9] operation:Operation::lfdu frD:16 rA:8 d:-13282];
	[self assertUndefined:instructions[10]];
	[self assertUndefined:instructions[11]];

	// rlwimi.	r8,r22,10,11,20
	// lfsu		f20,-16191(r1)
	// stfs		f29,-603(r17)
	// .long 0xe2b401cb
	[self assert:instructions[12] operation:Operation::rlwimix rA:8 rS:22 rB:10 mb:11 me:20 rc:TRUE];
	[self assert:instructions[13] operation:Operation::lfsu frD:20 rA:1 d:-16191];
	[self assert:instructions[14] operation:Operation::stfs frS:29 rA:17 d:-603];
	[self assertUndefined:instructions[15]];

	// .long 0x433cb83d
	// rlwinm.	r1,r2,5,29,0
	// rlwimi	r13,r25,12,9,5
	// .long 0xb3117c5
	[self assertUndefined:instructions[16]];
	[self assert:instructions[17] operation:Operation::rlwinmx rA:1 rS:2 rB:5 mb:29 me:0 rc:TRUE];
	[self assert:instructions[18] operation:Operation::rlwimix rA:13 rS:25 rB:12 mb:9 me:5 rc:FALSE];
	[self assertUndefined:instructions[19]];

	// stb		r4,17594(r1)
	// lfsu		f29,27344(r9)
	// cmpli	cr0,1,r19,5236
	// rlwinm.	r0,r17,24,13,19
	[self assert:instructions[20] operation:Operation::stb rS:4 rA:1 d:17594];
	[self assert:instructions[21] operation:Operation::lfsu frD:29 rA:9 d:27344];
	[self assert:instructions[22] operation:Operation::cmpli crfD:0 l:TRUE rA:19 uimm:5236];
	[self assert:instructions[23] operation:Operation::rlwinmx rA:0 rS:17 rB:24 mb:13 me:19 rc:TRUE];

	// lha		r24,9735(r10)
	// .long 0xe826acf4
	// beq+		cr5,0xffffffffffff91bc		[at address 0x68]
	// oris		r17,r3,53407
	[self assert:instructions[24] operation:Operation::lha rD:24 rA:10 d:9735];
	[self assertUndefined:instructions[25]];
	[self assert:instructions[26] operation:Operation::bcx bo:12 bi:22 bd:0xffff91bc - 0x68 lk:FALSE aa:FALSE];
	[self assert:instructions[27] operation:Operation::oris rA:17 rS:3 uimm:53407];

	// xori		r5,r17,61519
	// bl		0xfffffffffe1599e0			[at address 0x74]
	// dozi		r25,r12,-26897
	// lha		r25,29097(r17)
	[self assert:instructions[28] operation:Operation::xori rA:5 rS:17 uimm:61519];
	[self assert:instructions[29] operation:Operation::bx li:0xfe1599e0 - 0x74 lk:TRUE aa:FALSE];
	[self assert:instructions[30] operation:Operation::dozi rD:25 rA:12 simm:-26897];
	[self assert:instructions[31] operation:Operation::lha rD:25 rA:17 d:29097];
}

@end
