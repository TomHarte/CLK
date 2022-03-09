//
//  x86DecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <initializer_list>
#include <optional>
#include <vector>
#include "../../../InstructionSets/x86/Decoder.hpp"
#include "../../../InstructionSets/x86/DataPointerResolver.hpp"

using namespace InstructionSet::x86;

namespace {

// MARK: - Specific instruction asserts.

template <typename InstructionT> void test(const InstructionT &instruction, DataSize size, Operation operation) {
	XCTAssertEqual(instruction.operation_size(), InstructionSet::x86::DataSize(size));
	XCTAssertEqual(instruction.operation, operation);
}

template <typename InstructionT> void test(
	const InstructionT &instruction,
	DataSize size,
	Operation operation,
	InstructionSet::x86::DataPointer source,
	std::optional<InstructionSet::x86::DataPointer> destination = std::nullopt,
	std::optional<typename InstructionT::ImmediateT> operand = std::nullopt,
	std::optional<typename InstructionT::DisplacementT> displacement = std::nullopt) {

	XCTAssertEqual(instruction.operation_size(), InstructionSet::x86::DataSize(size));
	XCTAssertEqual(instruction.operation, operation);
	XCTAssert(instruction.source() == source);
	if(destination) XCTAssert(instruction.destination() == *destination);
	if(operand)	XCTAssertEqual(instruction.operand(), *operand);
	if(displacement) XCTAssertEqual(instruction.displacement(), *displacement);
}

template <typename InstructionT> void test(
	const InstructionT &instruction,
	Operation operation,
	std::optional<typename InstructionT::ImmediateT> operand = std::nullopt,
	std::optional<typename InstructionT::DisplacementT> displacement = std::nullopt) {
	XCTAssertEqual(instruction.operation, operation);
	if(operand)	XCTAssertEqual(instruction.operand(), *operand);
	if(displacement) XCTAssertEqual(instruction.displacement(), *displacement);
}

template <typename InstructionT> void test_far(const InstructionT &instruction, Operation operation, uint16_t segment, uint16_t offset) {
	XCTAssertEqual(instruction.operation, operation);
	XCTAssertEqual(instruction.segment(), segment);
	XCTAssertEqual(instruction.offset(), offset);
}

// MARK: - Decoder

template <Model model>
std::vector<typename InstructionSet::x86::Decoder<model>::InstructionT> decode(const std::initializer_list<uint8_t> &stream, bool set_32_bit = false) {
	// Decode by offering up all data at once.
	std::vector<typename InstructionSet::x86::Decoder<model>::InstructionT> instructions;
	InstructionSet::x86::Decoder<model> decoder;
	decoder.set_32bit_protected_mode(set_32_bit);
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
		InstructionSet::x86::Decoder<model> decoder;
		decoder.set_32bit_protected_mode(set_32_bit);

		auto previous_instruction = instructions.begin();
		for(auto item: stream) {
			const auto [size, next] = decoder.decode(&item, 1);
			if(size > 0) {
				XCTAssert(next == *previous_instruction);
				++previous_instruction;
			}
		}
	}

	return instructions;
}

}

@interface x86DecoderTests : XCTestCase
@end

/*!
	Tests 8086 decoding by throwing a bunch of randomly-generated
	word streams and checking that the result matches what I got from a
	disassembler elsewhere.
*/
@implementation x86DecoderTests

- (void)testSequence1 {
	// Sequences the Online Disassembler believes to exist but The 8086 Book does not:
	//
	// 0x6a 0x65	push $65
	// 0x65 0x6d	gs insw (%dx),%es:(%di)
	// 0x67 0x61	addr32 popa
	// 0x6c			insb (%dx), %es:(%di)
	// 0xc9			leave
	//
	const auto instructions = decode<Model::i8086>({
		0x2d, 0x77, 0xea, 0x72, 0xfc, 0x4b, 0xb5, 0x28, 0xc3, 0xca, 0x26, 0x48, /* 0x65, 0x6d, */ 0x7b, 0x9f,
		0xc2, 0x65, 0x42, 0x4e, 0xef, 0x70, 0x20, 0x94, 0xc4, 0xd4, 0x93, 0x43, 0x3c, 0x8e, /* 0x6a, 0x65, */
		0x1a, 0x78, 0x45, 0x10, 0x7f, 0x3c, 0x19, 0x5a, 0x16, 0x31, 0x64, 0x2c, 0xe7, 0xc6, 0x7d, 0xb0,
		0xb5, 0x49, /* 0x67, 0x61, */ 0xba, 0xc0, 0xcb, 0x14, 0x7e, 0x71, 0xd0, 0x50, 0x78, 0x3d, 0x03, 0x1d,
		0xe5, 0xc9, 0x97, 0xc3, 0x9b, 0xe6, 0xd3, /* 0x6c, */ 0x58, 0x4d, 0x76, 0x80, 0x44, 0xd6, 0x9f, 0xa5,
		0xbd, 0xa1, 0x12, 0xc5, 0x29, /* 0xc9, */ 0x9e, 0xd8, 0xf3, 0xcf, 0x92, 0x39, 0x5d, 0x90, 0x15, 0xc3,
		0xb8, 0xad, 0xe8, 0xc8, 0x16, 0x4a, 0xb0, 0x9e, 0xf9, 0xbf, 0x56, 0xea, 0x4e, 0xfd, 0xe4, 0x5a,
		0x23, 0xaa, 0x2c, 0x5b, 0x2a, 0xd2, 0xf7, 0x5f, 0x18, 0x86, 0x90, 0x25, 0x64, 0xb7, 0xc3
	});

	// 63 instructions are expected.
	XCTAssertEqual(instructions.size(), 63);

	// sub		$0xea77,%ax
	// jb		0x00000001
	// dec		%bx
	// mov		$0x28,%ch
	test(instructions[0], DataSize::Word, Operation::SUB, Source::Immediate, Source::eAX, 0xea77);
	test(instructions[1], Operation::JB, std::nullopt, 0xfffc);
	test(instructions[2], DataSize::Word, Operation::DEC, Source::eBX, Source::eBX);
	test(instructions[3], DataSize::Byte, Operation::MOV, Source::Immediate, Source::CH, 0x28);

	// ret
	// lret		$0x4826
	// [[ omitted: gs insw (%dx),%es:(%di) ]]
	// jnp		0xffffffaf
	// ret		$0x4265
	test(instructions[4], Operation::RETN);
	test(instructions[5], Operation::RETF, 0x4826);
	test(instructions[6], Operation::JNP, std::nullopt, 0xff9f);
	test(instructions[7], Operation::RETN, 0x4265);

	// dec		%si
	// out		%ax,(%dx)
	// jo		0x00000037
	// xchg		%ax,%sp
	test(instructions[8], DataSize::Word, Operation::DEC, Source::eSI, Source::eSI);
	test(instructions[9], DataSize::Word, Operation::OUT, Source::eAX, Source::eDX);
	test(instructions[10], Operation::JO, std::nullopt, 0x20);
	test(instructions[11], DataSize::Word, Operation::XCHG, Source::eAX, Source::eSP);

	// ODA has:
	// 	c4		(bad)
	// 	d4 93	aam		$0x93
	//
	// That assumes that upon discovering that the d4 doesn't make a valid LES,
	// it can become an instruction byte. I'm not persuaded. So I'm taking:
	//
	//	c4 d4	(bad)
	//	93		XCHG AX, BX
	test(instructions[12], Operation::Invalid);
	test(instructions[13], DataSize::Word, Operation::XCHG, Source::eAX, Source::eBX);

	// inc		%bx
	// cmp		$0x8e,%al
	// [[ omitted: push		$0x65 ]]
	// sbb		0x45(%bx,%si),%bh
	// adc		%bh,0x3c(%bx)
	test(instructions[14], DataSize::Word, Operation::INC, Source::eBX, Source::eBX);
	test(instructions[15], DataSize::Byte, Operation::CMP, Source::Immediate, Source::eAX, 0x8e);
	test(instructions[16], DataSize::Byte, Operation::SBB, ScaleIndexBase(Source::eBX, Source::eSI), Source::BH, std::nullopt, 0x45);
	test(instructions[17], DataSize::Byte, Operation::ADC, Source::BH, ScaleIndexBase(Source::eBX), std::nullopt, 0x3c);

	// sbb		%bx,0x16(%bp,%si)
	// xor		%sp,0x2c(%si)
	// out		%ax,$0xc6
	// jge		0xffffffe0
	test(instructions[18], DataSize::Word, Operation::SBB, Source::eBX, ScaleIndexBase(Source::eBP, Source::eSI), std::nullopt, 0x16);
	test(instructions[19], DataSize::Word, Operation::XOR, Source::eSP, ScaleIndexBase(Source::eSI), std::nullopt, 0x2c);
	test(instructions[20], DataSize::Word, Operation::OUT, Source::eAX, Source::DirectAddress, 0xc6);
	test(instructions[21], Operation::JNL, std::nullopt, 0xffb0);

	// mov		$0x49,%ch
	// [[ omitted: addr32	popa ]]
	// mov		$0xcbc0,%dx
	// adc		$0x7e,%al
	// jno		0x0000000b
	test(instructions[22], DataSize::Byte, Operation::MOV, Source::Immediate, Source::CH, 0x49);
	test(instructions[23], DataSize::Word, Operation::MOV, Source::Immediate, Source::eDX, 0xcbc0);
	test(instructions[24], DataSize::Byte, Operation::ADC, Source::Immediate, Source::eAX, 0x7e);
	test(instructions[25], Operation::JNO, std::nullopt, 0xffd0);

	// push		%ax
	// js		0x0000007b
	// add		(%di),%bx
	// in		$0xc9,%ax
	test(instructions[26], DataSize::Word, Operation::PUSH, Source::eAX);
	test(instructions[27], Operation::JS, std::nullopt, 0x3d);
	test(instructions[28], DataSize::Word, Operation::ADD, ScaleIndexBase(Source::eDI), Source::eBX);
	test(instructions[29], DataSize::Word, Operation::IN, Source::DirectAddress, Source::eAX, 0xc9);

	// xchg		%ax,%di
	// ret
	// fwait
	// out		%al,$0xd3
	test(instructions[30], DataSize::Word, Operation::XCHG, Source::eAX, Source::eDI);
	test(instructions[31], Operation::RETN);
	test(instructions[32], Operation::WAIT);
	test(instructions[33], DataSize::Byte, Operation::OUT, Source::eAX, Source::DirectAddress, 0xd3);

	// [[ omitted: insb		(%dx),%es:(%di) ]]
	// pop		%ax
	// dec		%bp
	// jbe		0xffffffcc
	// inc		%sp
	test(instructions[34], DataSize::Word, Operation::POP, Source::eAX);
	test(instructions[35], DataSize::Word, Operation::DEC, Source::eBP, Source::eBP);
	test(instructions[36], Operation::JBE, std::nullopt, 0xff80);
	test(instructions[37], DataSize::Word, Operation::INC, Source::eSP, Source::eSP);

	// (bad)
	// lahf
	// movsw	%ds:(%si),%es:(%di)
	// mov		$0x12a1,%bp
	test(instructions[38], Operation::Invalid);
	test(instructions[39], Operation::LAHF);
	test(instructions[40], DataSize::Word, Operation::MOVS); // Arguments are implicit.
	test(instructions[41], DataSize::Word, Operation::MOV, Source::Immediate, Source::eBP, 0x12a1);

	// lds		(%bx,%di),%bp
	// [[ omitted: leave ]]
	// sahf
	// fdiv		%st(3),%st
	// iret
	test(instructions[42], DataSize::Word, Operation::LDS);
	test(instructions[43], Operation::SAHF);
	test(instructions[44], Operation::ESC);
	test(instructions[45], Operation::IRET);

	// xchg		%ax,%dx
	// cmp		%bx,-0x70(%di)
	// adc		$0xb8c3,%ax
	// lods		%ds:(%si),%ax
	test(instructions[46], DataSize::Word, Operation::XCHG, Source::eAX, Source::eDX);
	test(instructions[47], DataSize::Word, Operation::CMP, Source::eBX, ScaleIndexBase(Source::eDI), std::nullopt, 0xff90);
	test(instructions[48], DataSize::Word, Operation::ADC, Source::Immediate, Source::eAX, 0xb8c3);
	test(instructions[49], DataSize::Word, Operation::LODS);

	// call		0x0000172d
	// dec		%dx
	// mov		$0x9e,%al
	// stc
	test(instructions[50], Operation::CALLD, uint16_t(0x16c8));
	test(instructions[51], DataSize::Word, Operation::DEC, Source::eDX, Source::eDX);
	test(instructions[52], DataSize::Byte, Operation::MOV, Source::Immediate, Source::eAX, 0x9e);
	test(instructions[53], Operation::STC);

	// mov		$0xea56,%di
	// dec		%si
	// std
	// in		$0x5a,%al
	test(instructions[54], DataSize::Word, Operation::MOV, Source::Immediate, Source::eDI, 0xea56);
	test(instructions[55], DataSize::Word, Operation::DEC, Source::eSI, Source::eSI);
	test(instructions[56], Operation::STD);
	test(instructions[57], DataSize::Byte, Operation::IN, Source::DirectAddress, Source::eAX, 0x5a);

	// and		0x5b2c(%bp,%si),%bp
	// sub		%dl,%dl
	// negw		0x18(%bx)
	// xchg		%dl,0x6425(%bx,%si)
	test(instructions[58], DataSize::Word, Operation::AND, ScaleIndexBase(Source::eBP, Source::eSI), Source::eBP, std::nullopt, 0x5b2c);
	test(instructions[59], DataSize::Byte, Operation::SUB, Source::eDX, Source::eDX);
	test(instructions[60], DataSize::Word, Operation::NEG, ScaleIndexBase(Source::eBX), ScaleIndexBase(Source::eBX), std::nullopt, 0x18);
	test(instructions[61], DataSize::Byte, Operation::XCHG, ScaleIndexBase(Source::eBX, Source::eSI), Source::eDX, std::nullopt, 0x6425);

	// mov		$0xc3,%bh
	test(instructions[62], DataSize::Byte, Operation::MOV, Source::Immediate, Source::BH, 0xc3);
}

- (void)test83 {
	const auto instructions = decode<Model::i8086>({
		0x83, 0x10, 0x80,	// adcw		$0xff80,(%bx,%si)
		0x83, 0x3b, 0x04,	// cmpw		$0x4,(%bp,%di)
		0x83, 0x2f, 0x09,	// subw		$0x9,(%bx)
	});

	XCTAssertEqual(instructions.size(), 3);
	test(instructions[0], DataSize::Word, Operation::ADC, Source::Immediate, ScaleIndexBase(Source::eBX, Source::eSI), 0xff80);
	test(instructions[1], DataSize::Word, Operation::CMP, Source::Immediate, ScaleIndexBase(Source::eBP, Source::eDI), 0x4);
	test(instructions[2], DataSize::Word, Operation::SUB, Source::Immediate, ScaleIndexBase(Source::eBX), 0x9);
}

- (void)testFar {
	const auto instructions = decode<Model::i8086>({
		0x9a, 0x12, 0x34, 0x56, 0x78,	// lcall 0x7856, 0x3412
	});

	XCTAssertEqual(instructions.size(), 1);
	test_far(instructions[0], Operation::CALLF, 0x7856, 0x3412);
}

- (void)testLDSLESEtc {
	auto run_test = [](bool is_32, DataSize size) {
		const auto instructions = decode<Model::i80386>({
			0xc5, 0x33,			// 16-bit: lds si, (bp, di);	32-bit: lds esi, (ebx)
			0xc4, 0x17,			// 16-bit: les dx, (bx);		32-bit: les edx, (edi)
			0x0f, 0xb2, 0x17,	// 16-bit: lss dx, (bx);		32-bit: lss edx, (edi)
		}, is_32);

		XCTAssertEqual(instructions.size(), 3);
		if(is_32) {
			test(instructions[0], size, Operation::LDS, ScaleIndexBase(Source::eBX), Source::eSI);
			test(instructions[1], size, Operation::LES, ScaleIndexBase(Source::eDI), Source::eDX);
			test(instructions[2], size, Operation::LSS, ScaleIndexBase(Source::eDI), Source::eDX);
		} else {
			test(instructions[0], size, Operation::LDS, ScaleIndexBase(Source::eBP, Source::eDI), Source::eSI);
			test(instructions[1], size, Operation::LES, ScaleIndexBase(Source::eBX), Source::eDX);
			test(instructions[2], size, Operation::LSS, ScaleIndexBase(Source::eBX), Source::eDX);
		}
	};

	run_test(false, DataSize::Word);
	run_test(true, DataSize::DWord);
}

- (void)testSIB {
	const auto instructions = decode<Model::i80386>({
		// add edx, -0x7d(ebp + eax*2)
		0x01, 0x54, 0x45, 0x83,

		// add edx, -0x80(si)
		0x67, 0x01, 0x54, 0x80,
	}, true);

	XCTAssertEqual(instructions.size(), 2);
	test(instructions[0], DataSize::DWord, Operation::ADD, Source::eDX, ScaleIndexBase(1, Source::eAX, Source::eBP), 0x00, -125);
	test(instructions[1], DataSize::DWord, Operation::ADD, Source::eDX, ScaleIndexBase(Source::eSI), 0x00, -128);
	XCTAssertEqual(instructions[1].address_size(), AddressSize::b16);
}

- (void)test32bitSequence {
	const auto instructions = decode<Model::i80386>({
		0x2e, 0x42, 0x0c, 0x09, 0x81, 0x47, 0xbe, 0xa9, 0x3a, 0x68, 0x9f, 0xf0, 0x7a, 0xe2, 0x3e, 0xb4,
		0xc1, 0x1f, 0xaa, 0x60, 0xb4, 0xe1, 0x91, 0xdc, 0xf6, 0x62, 0x90, 0x90, 0xdf, 0xcd, 0xf9, 0x0f,
		0xbb, 0x71, 0x4b, 0x58, 0x55, 0x38, 0x2c, 0xf9, 0x50, 0xfe, 0xce, 0xe0, 0xc1, 0xda, 0x83, 0x8c,
		0x19, 0x0c, 0x9b, 0x89, 0x13, 0x34, 0x45, 0xc5, 0x11, 0xa2, 0xd3, 0xa6, 0xdb, 0xe4, 0x1f, 0xa5,
		0x79, 0xf3, 0x7d, 0x1c, 0xb8, 0xda, 0x6b, 0x76, 0x8a, 0x79, 0x28, 0x52, 0xcd, 0xc4, 0xe9, 0xba,
		0x11, 0xcf, 0x29, 0x09, 0x46, 0x1a, 0xc0, 0x5d, 0x88, 0x34, 0xa5, 0x83, 0xe2, 0xd0, 0xf5, 0x44,
		0x9d, 0xa5, 0xc1, 0x5e, 0x4f, 0x07, 0x51, 0xd4, 0xed, 0xb0, 0x69, 0xd7, 0x00, 0xc5, 0x51, 0xfb,
		0x68, 0x85, 0x3a, 0x8b, 0x69, 0x28, 0x0c, 0xec, 0xb1, 0xb7, 0x3b, 0x8d, 0x5f, 0x44, 0x87, 0x2c,
		0xe3, 0x02, 0x9e, 0x74, 0x6e, 0x1b, 0x8f, 0x4d, 0xc5, 0x33, 0x04, 0x9f, 0xac, 0xc0, 0xc9, 0x60,
		0x9a, 0x8a, 0xf5, 0xd0, 0x97, 0x1b, 0xe2, 0x64, 0x60, 0xb0, 0xcf, 0xe3, 0x37,
	}, true);

	XCTAssertEqual(instructions.size(), 64);

	//cs inc edx
	//or     al,0x9
	//add    DWORD PTR [edi-0x42],0x9f683aa9
	//lock jp 0xfffffff0	(from 0000000e)
	test(instructions[0], DataSize::DWord, Operation::INC, Source::eDX);
	XCTAssertEqual(instructions[0].data_segment(), Source::CS);
	test(instructions[1], DataSize::Byte, Operation::OR, Source::Immediate, Source::eAX, 0x9);
	test(instructions[2], DataSize::DWord, Operation::ADD, Source::Immediate, ScaleIndexBase(Source::eDI), 0x9f683aa9, -0x42);
	test(instructions[3], Operation::JP, 0, -30);
	XCTAssert(instructions[3].lock());

	//ds mov ah,0xc1
	//pop    ds
	//stos   BYTE PTR es:[edi],al
	//pusha
	test(instructions[4], DataSize::Byte, Operation::MOV, Source::Immediate, Source::AH, 0xc1);
	XCTAssertEqual(instructions[4].data_segment(), Source::DS);
	test(instructions[5], DataSize::Word, Operation::POP, Source::None, Source::DS);
	test(instructions[6], DataSize::Byte, Operation::STOS);
	test(instructions[7], Operation::PUSHA);

	//mov    ah,0xe1
	//xchg   ecx,eax
	//fdivr  st(6),st
	//bound  edx,QWORD PTR [eax-0x6322070]
	test(instructions[8], DataSize::Byte, Operation::MOV, Source::Immediate, Source::AH, 0xe1);
	test(instructions[9], DataSize::DWord, Operation::XCHG, Source::eAX, Source::eCX);
	test(instructions[10], DataSize::None, Operation::ESC);
	test(instructions[11], DataSize::DWord, Operation::BOUND, ScaleIndexBase(Source::eAX), Source::eDX, 0, -0x6322070);

	//btc    DWORD PTR [ecx+0x4b],esi
	//pop    eax
	//push   ebp
	//cmp    BYTE PTR [ecx+edi*8],ch
	test(instructions[12], DataSize::DWord, Operation::BTC, Source::eSI, ScaleIndexBase(Source::eCX), 0, 0x4b);
	test(instructions[13], DataSize::DWord, Operation::POP, Source::eAX, Source::eAX);
	test(instructions[14], DataSize::DWord, Operation::PUSH, Source::eBP);
	test(instructions[15], DataSize::Byte, Operation::CMP, Source::CH, ScaleIndexBase(3, Source::eDI, Source::eCX));

	// Possibly TODO: pick a lane on whether PUSH/POP duplicate source and destination.
	// It doesn't really matter outside of these tests though.

	//push   eax
	//dec    dh
	//loopne 0xffffffee (from 0x2d)
	//fiadd  DWORD PTR [ebx-0x64f3e674]
	test(instructions[16], DataSize::DWord, Operation::PUSH, Source::eAX);
	test(instructions[17], DataSize::Byte, Operation::DEC, Source::DH);
	test(instructions[18], Operation::LOOPNE, 0, -18);

	//mov    DWORD PTR [ebx],edx
	//xor    al,0x45
	//lds    edx,FWORD PTR [ecx]

	// Note to self: disassembly currently diverges at or immediately after this MOV:
	//mov    ds:0xe4dba6d3,al
	//pop    ds
	//movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
	//jns    0x00000035
	//jge    0x00000060
	//mov    eax,0x8a766bda
	//jns    0x00000073
	//push   edx
	//int    0xc4
	//jmp    0x29cf120d
	//or     DWORD PTR [esi+0x1a],eax
	//rcr    BYTE PTR [ebp-0x78],0x34
	//movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
	//and    edx,0xffffffd0
	//cmc
	//inc    esp
	//popf
	//movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
	//rcr    DWORD PTR [esi+0x4f],0x7
	//push   ecx
	//aam    0xed
	//mov    al,0x69
	//xlat   BYTE PTR ds:[ebx]
	//add    ch,al
	//push   ecx
	//sti
	//push   0x698b3a85
	//sub    BYTE PTR [esp+ebp*8],cl
	//mov    cl,0xb7
	//cmp    ecx,DWORD PTR [ebp+0x2c87445f]
	//jecxz  0x00000084
	//sahf
	//je     0x000000f3
	//sbb    ecx,DWORD PTR [edi+0x433c54d]
	//lahf
	//lods   al,BYTE PTR ds:[esi]
	//ror    cl,0x60
	//call   0xe21b:0x97d0f58a
	//fs pusha
	//mov    al,0xcf
	//jecxz  0x000000d4
}

@end
