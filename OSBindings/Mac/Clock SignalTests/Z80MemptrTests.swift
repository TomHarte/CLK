//
//  Z80MemptrTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MemptrTester: XCTestCase {
	let machine = CSTestMachineZ80()

	private func test(program : [UInt8], initialValue : UInt16) -> UInt16 {
		// Create a machine and install the supplied program at address 0, setting the PC to run from there
		machine.setValue(0x0000, for: .programCounter)
		machine.setData(Data(_: program), atAddress: 0x0000)

		// Set the initial value of memptr, run for the requested number of cycles,
		// return the new value
		machine.setValue(initialValue, for: .memPtr)
		machine.runForInstruction()
		return machine.value(for: .memPtr)
	}

	private func testPage(_ prefix: UInt8, exclusions: [Int]) {
		for opcode in 0 ..< 256 {
			if exclusions.contains(opcode) {
				continue
			}

			var program: [UInt8] = [
				(prefix != 0) ? prefix : UInt8(opcode), UInt8(opcode), 0, 0
			]

			let argumentPosition = (prefix != 0) ? 2 : 1
			for _ in 0 ..< 10 {
				let random = arc4random_uniform(65536)
				program[argumentPosition + 0] = UInt8(random & 0x00ff)
				program[argumentPosition + 1] = UInt8(random >> 8)

				let expectedResult = UInt16(arc4random_uniform(65536))

				let result = test(program: program, initialValue: expectedResult)
				XCTAssertEqual(result, expectedResult, "Failed for opcode \(String(opcode, radix:16))")

				// One failure will do.
				if result != expectedResult {
					break
				}
			}
		}
	}

	private func insert16(program: inout [UInt8], address: Int, offset: size_t) {
		program[offset] = UInt8(address & 0x00ff)
		program[offset + 1] = UInt8(address >> 8)
	}

	/// Tests that everything not listed in the documentation has no effect upon MEMPTR.
	func testStandardPageOthers() {
		testPage(0, exclusions: [
			0x02,		// LD (BC), A
			0x09,		// ADD HL, BC
			0x0a,		// LD A, (BC)
			0x10,		// DJNZ
			0x12,		// LD (DE), A
			0x18,		// JR
			0x19,		// ADD HL, DE
			0x1a,		// LD A, (DE)
			0x20,		// JR NZ
			0x22,		// LD (nn), HL
			0x28,		// JR Z
			0x29,		// ADD HL, HL
			0x30,		// JR NC
			0x32,		// LD (nn), A
			0x38,		// JR C
			0x39,		// ADD HL, SP
			0x3a,		// LD A, (nn)

			0xcb,		// CB page
			0xdd,		// DD page
			0xed,		// ED page
			0xfd,		// FD page
		])
	}

	func testEDPageOthers() {
		testPage(0xed, exclusions: [
			0x42,		// SBC HL, BC
			0x43,		// LD (nn), HL
			0x45,		// RETN				(??)
			0x4a,		// ADC HL, BC
			0x4d,		// RETI
			0x52,		// SBC HL, DE
			0x53,		// LD (nn), DE
			0x55,		// RETN				(??)
			0x5a,		// ADC HL, DE
			0x5d,		// RETN				(??)
			0x62,		// SBC HL, HL
			0x63,		// LD (nn), HL
			0x65,		// RETN				(??)
			0x67,		// RRD
			0x6a,		// ADC HL, HL
			0x6d,		// RETN				(??)
			0x6f,		// RLD
			0x72,		// SBC HL, SP
			0x73,		// LD (nn), SP
			0x75,		// RETN				(??)
			0x78,		// IN A, (C)
			0x79,		// OUT (C), A
			0x7a,		// ADC HL, SP
			0x7d,		// RETN				(??)
			0xa1,		// CPI
			0xa2,		// INI
			0xa3,		// OUTI
			0xa9,		// CPD
			0xaa,		// IND
			0xab,		// OUTD
			0xb1,		// CPIR
			0xb3,		// OUIR
			0xb9,		// CPDR
			0xbb		// OTDR
		])
//		testPage(0xcb, exclusions: [])
//		testPage(0xdd, exclusions: [])
//		testPage(0xfd, exclusions: [])
	}

	/*
		Re: comments below:
		All the CPU chips tested give the same results except KP1858BM1 and T34BM1 slices noted as "BM1".
	*/

	// LD A, (addr)
	func testLDAnn() {
		// MEMPTR = addr+1
		var program: [UInt8] = [
			0x3a, 0x00, 0x00
		]
		for addr in 0 ..< 65536 {
			program[1] = UInt8(addr & 0x00ff)
			program[2] = UInt8(addr >> 8)
			let expectedResult = UInt16((addr + 1) & 0xffff)

			let result = test(program: program, initialValue: 0xffff)
			XCTAssertEqual(result, expectedResult)
		}
	}

	// LD (bc/de),A, and LD (nn),A
	func testLDrpA() {
		// MEMPTR_low = (addr + 1) & #FF,  MEMPTR_hi = A
		// Note for *BM1: MEMPTR_low = (addr + 1) & #FF,  MEMPTR_hi = 0
		let bcProgram: [UInt8] = [
			0x02
		]
		let deProgram: [UInt8] = [
			0x12
		]
		var nnProgram: [UInt8] = [
			0x32, 0x00, 0x00
		]

		for addr in 0 ..< 256 {
			machine.setValue(UInt16(addr), for: .BC)
			machine.setValue(UInt16(addr), for: .DE)
			insert16(program: &nnProgram, address: addr, offset: 1)

			for a in 0 ..< 256 {
				machine.setValue(UInt16(a), for: .A)

				let expectedResult = UInt16(((addr + 1) & 0xff) + (a << 8))

				let bcResult = test(program: bcProgram, initialValue: 0xffff)
				let deResult = test(program: deProgram, initialValue: 0xffff)
				let nnResult = test(program: nnProgram, initialValue: 0xffff)

				XCTAssertEqual(bcResult, expectedResult)
				XCTAssertEqual(deResult, expectedResult)
				XCTAssertEqual(nnResult, expectedResult)
			}
		}
	}

	// LD A, (rp)
	func testLDArp() {
		// MEMPTR = rp+1
		let bcProgram: [UInt8] = [
			0x0a
		]
		let deProgram: [UInt8] = [
			0x1a
		]
		for addr in 0 ..< 65536 {
			machine.setValue(UInt16(addr), for: .BC)
			machine.setValue(UInt16(addr), for: .DE)

			let expectedResult = UInt16((addr + 1) & 0xffff)

			let bcResult = test(program: bcProgram, initialValue: 0xffff)
			let deResult = test(program: deProgram, initialValue: 0xffff)

			XCTAssertEqual(bcResult, expectedResult)
			XCTAssertEqual(deResult, expectedResult)
		}
	}

	// LD (addr), rp
	func testLDnnrp() {
		// MEMPTR = addr + 1
		var ldnnhlBaseProgram: [UInt8] = [
			0x22, 0x00, 0x00
		]
		var ldnnbcEDProgram: [UInt8] = [
			0xed, 0x43, 0x00, 0x00
		]
		var ldnndeEDProgram: [UInt8] = [
			0xed, 0x53, 0x00, 0x00
		]
		var ldnnhlEDProgram: [UInt8] = [
			0xed, 0x63, 0x00, 0x00
		]
		var ldnnspEDProgram: [UInt8] = [
			0xed, 0x73, 0x00, 0x00
		]

		for addr in 0 ..< 65536 {
			insert16(program: &ldnnhlBaseProgram, address: addr, offset: 1)
			insert16(program: &ldnnbcEDProgram, address: addr, offset: 2)
			insert16(program: &ldnndeEDProgram, address: addr, offset: 2)
			insert16(program: &ldnnhlEDProgram, address: addr, offset: 2)
			insert16(program: &ldnnspEDProgram, address: addr, offset: 2)

			let expectedResult = UInt16((addr + 1) & 0xffff)

			XCTAssertEqual(test(program: ldnnhlBaseProgram, initialValue: expectedResult ^ 1), expectedResult)
			XCTAssertEqual(test(program: ldnnbcEDProgram, initialValue: expectedResult ^ 1), expectedResult)
			XCTAssertEqual(test(program: ldnndeEDProgram, initialValue: expectedResult ^ 1), expectedResult)
			XCTAssertEqual(test(program: ldnnhlEDProgram, initialValue: expectedResult ^ 1), expectedResult)
			XCTAssertEqual(test(program: ldnnspEDProgram, initialValue: expectedResult ^ 1), expectedResult)
		}
	}

	// LD rp, (addr)
	func testLDrpnn() {
		// MEMPTR = addr+1
		var hlBaseProgram: [UInt8] = [
			0x22, 0x00, 0x00
		]

		var bcEDProgram: [UInt8] = [
			0xed, 0x43, 0x00, 0x00
		]
		var deEDProgram: [UInt8] = [
			0xed, 0x53, 0x00, 0x00
		]
		var hlEDProgram: [UInt8] = [
			0xed, 0x63, 0x00, 0x00
		]
		var spEDProgram: [UInt8] = [
			0xed, 0x73, 0x00, 0x00
		]

		var ixProgram: [UInt8] = [
			0xdd, 0x22, 0x00, 0x00
		]
		var iyProgram: [UInt8] = [
			0xfd, 0x22, 0x00, 0x00
		]

		for addr in 0 ..< 65536 {
			hlBaseProgram[1] = UInt8(addr & 0x00ff)
			hlBaseProgram[2] = UInt8(addr >> 8)

			bcEDProgram[2] = UInt8(addr & 0x00ff)
			bcEDProgram[3] = UInt8(addr >> 8)
			deEDProgram[2] = UInt8(addr & 0x00ff)
			deEDProgram[3] = UInt8(addr >> 8)
			hlEDProgram[2] = UInt8(addr & 0x00ff)
			hlEDProgram[3] = UInt8(addr >> 8)
			spEDProgram[2] = UInt8(addr & 0x00ff)
			spEDProgram[3] = UInt8(addr >> 8)

			ixProgram[2] = UInt8(addr & 0x00ff)
			ixProgram[3] = UInt8(addr >> 8)
			iyProgram[2] = UInt8(addr & 0x00ff)
			iyProgram[3] = UInt8(addr >> 8)

			let expectedResult = UInt16((addr + 1) & 0xffff)

			XCTAssertEqual(test(program: hlBaseProgram, initialValue: 0xffff), expectedResult)

			XCTAssertEqual(test(program: bcEDProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: deEDProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: hlEDProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: spEDProgram, initialValue: 0xffff), expectedResult)

			XCTAssertEqual(test(program: ixProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: iyProgram, initialValue: 0xffff), expectedResult)
		}
	}

	// EX (SP), rp
	func testEXSPrp() {
		// MEMPTR = rp at end
		var hlProgram: [UInt8] = [
			0xe3, 0x00, 0x00, 0x00
		]
		var ixProgram: [UInt8] = [
			0xdd, 0xe3, 0x00, 0x00
		]
		var iyProgram: [UInt8] = [
			0xfd, 0xe3, 0x00, 0x00
		]

		machine.setValue(2, for: .stackPointer)

		for addr in 0 ..< 65536 {
			hlProgram[2] = UInt8(addr & 0x00ff)
			hlProgram[3] = UInt8(addr >> 8)
			ixProgram[2] = UInt8(addr & 0x00ff)
			ixProgram[3] = UInt8(addr >> 8)
			iyProgram[2] = UInt8(addr & 0x00ff)
			iyProgram[3] = UInt8(addr >> 8)

			XCTAssertEqual(test(program: hlProgram, initialValue: 0xffff), UInt16(addr))
			XCTAssertEqual(test(program: ixProgram, initialValue: 0xffff), UInt16(addr))
			XCTAssertEqual(test(program: iyProgram, initialValue: 0xffff), UInt16(addr))
		}
	}

	// ADD/ADC/SBC dest, src
	func testADDADCSBCrr() {
		// MEMPTR = dest prior to modification + 1
		let addProgram: [UInt8] = [
			0x09
		]
		let adcProgram: [UInt8] = [
			0xed, 0x4a
		]
		let sbcProgram: [UInt8] = [
			0xed, 0x42
		]

		for addr in 0 ..< 65536 {
			let expectedResult = UInt16((addr + 1) & 0xffff)
			machine.setValue(UInt16(addr), for: .HL)

			XCTAssertEqual(test(program: addProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: adcProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: sbcProgram, initialValue: 0xffff), expectedResult)
		}
	}

	// RLD/RRD
	func testRLDRRD() {
		// MEMPTR = HL + 1
		let rldProgram: [UInt8] = [
			0xed, 0x6f
		]
		let rrdProgram: [UInt8] = [
			0xed, 0x67
		]

		for addr in 0 ..< 65536 {
			let expectedResult = UInt16((addr + 1) & 0xffff)
			machine.setValue(UInt16(addr), for: .HL)

			XCTAssertEqual(test(program: rldProgram, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: rrdProgram, initialValue: 0xffff), expectedResult)
		}
	}

	/* TODO:
		JR/DJNZ/RET/RETI/RST (jumping to addr)
			MEMPTR = addr

		(implemented in principle)
	*/
	func testJR() {
		var jrProgram: [UInt8] = [
			0x18, 0x00
		]

		for offset in 0 ..< 256 {
			jrProgram[1] = UInt8(offset)
			let result = test(program: jrProgram, initialValue: 0xffff)
			XCTAssertEqual(result, machine.value(for: .programCounter))
		}
	}

	func testJRcc() {
		func testJR(instruction: UInt8) {
			var program: [UInt8] = [
				instruction, 0x00
			]

			for offset in 0 ..< 256 {
				program[1] = UInt8(offset)
				let result = test(program: program, initialValue: 0xffff)
				XCTAssertEqual(result, machine.value(for: .programCounter))
			}
		}

		// JR NZ.
		machine.setValue(0x00, for: .AF)
		testJR(instruction: 0x20)

		machine.setValue(0xff, for: .AF)
		testJR(instruction: 0x20)

		// JR NC
		machine.setValue(0x00, for: .AF)
		testJR(instruction: 0x30)

		machine.setValue(0xff, for: .AF)
		testJR(instruction: 0x30)

		// JR Z
		machine.setValue(0x00, for: .AF)
		testJR(instruction: 0x28)

		machine.setValue(0x01, for: .AF)
		testJR(instruction: 0x28)

		// JR C
		machine.setValue(0x00, for: .AF)
		testJR(instruction: 0x38)

		machine.setValue(0x01, for: .AF)
		testJR(instruction: 0x38)
	}

	func testRST() {
		var rstProgram: [UInt8] = [
			0x00
		]

		for offset in 0 ..< 8 {
			rstProgram[0] = UInt8(offset << 3) + 0xc7
			let result = test(program: rstProgram, initialValue: 0xffff)
			XCTAssertEqual(result, machine.value(for: .programCounter))
		}
	}

	func testRET() {
		let retProgram: [UInt8] = [
			0xc9
		]

		for addr in 0 ..< 65536 {
			let stackContents: [UInt8] = [
				UInt8(addr & 0xff), UInt8(addr >> 8)
			]

			machine.setData(Data(stackContents), atAddress: 0xf001)
			machine.setValue(0xf000, for: .stackPointer)
			let result = test(program: retProgram, initialValue: 0xffff)
			XCTAssertEqual(result, machine.value(for: .programCounter))
		}
	}
	func testRETI() {
		let retiProgram: [UInt8] = [
			0xed, 0x4d
		]

		for addr in 0 ..< 65536 {
			let stackContents: [UInt8] = [
				UInt8(addr & 0xff), UInt8(addr >> 8)
			]

			machine.setData(Data(stackContents), atAddress: 0xf001)
			machine.setValue(0xf000, for: .stackPointer)
			let result = test(program: retiProgram, initialValue: 0xffff)
			XCTAssertEqual(result, machine.value(for: .programCounter))
		}
	}

	/* TODO:
		JP(except JP rp)/CALL addr (even in case of conditional call/jp, independently on condition satisfied or not)
			MEMPTR = addr
	*/
	func testCALL() {
		var callProgram: [UInt8] = [
			0xcd, 0x00, 0x00
		]

		for offset in 0 ..< 65536 {
			callProgram[1] = UInt8(offset & 0xff)
			callProgram[2] = UInt8(offset >> 8)
			let result = test(program: callProgram, initialValue: 0xffff)
			XCTAssertEqual(result, machine.value(for: .programCounter))
		}
	}

	/* TODO:
		IN A,(port)
			MEMPTR = (A_before_operation << 8) + port + 1

		(implemented, not tested)
	*/

	/* TODO:
		IN A,(C)
			MEMPTR = BC + 1

		(implemented, not tested)
	*/

	/* TODO:
		OUT (port),A
			MEMPTR_low = (port + 1) & #FF,  MEMPTR_hi = A
			Note for *BM1: MEMPTR_low = (port + 1) & #FF,  MEMPTR_hi = 0
	*/

	/* TODO:
		OUT (C),A
			MEMPTR = BC + 1
	*/

	/* TODO:
		LDIR/LDDR
			when BC == 1: MEMPTR is not changed
			when BC <> 1: MEMPTR = PC + 1, where PC = instruction address
	*/

	// CPI
	func testCPI() {
		// MEMPTR = MEMPTR + 1
		let program: [UInt8] = [
			0xed, 0xa1
		]
		machine.setData(Data(_: program), atAddress: 0x0000)
		machine.setValue(0, for: .memPtr)

		for c in 1 ..< 65536 {
			machine.setValue(0x0000, for: .programCounter)
			machine.runForNumber(ofCycles: 16)
			XCTAssertEqual(UInt16(c), machine.value(for: .memPtr))
		}
	}

	// CPD
	func testCPD() {
		// MEMPTR = MEMPTR - 1
		let program: [UInt8] = [
			0xed, 0xa9
		]
		machine.setData(Data(_: program), atAddress: 0x0000)
		machine.setValue(0, for: .memPtr)

		for c in 1 ..< 65536 {
			machine.setValue(0x0000, for: .programCounter)
			machine.runForNumber(ofCycles: 16)
			XCTAssertEqual(UInt16(65536 - c), machine.value(for: .memPtr))
		}
	}

	/* TODO:
		CPIR
			when BC=1 or A=(HL): exactly as CPI
			In other cases MEMPTR = PC + 1 on each step, where PC = instruction address.
			Note* since at the last execution BC=1 or A=(HL), resulting MEMPTR = PC + 1 + 1
			  (if there were not interrupts during the execution)
	*/

	/* TODO:
		CPDR
			when BC=1 or A=(HL): exactly as CPD
			In other cases MEMPTR = PC + 1 on each step, where PC = instruction address.
			Note* since at the last execution BC=1 or A=(HL), resulting MEMPTR = PC + 1 - 1
			  (if there were not interrupts during the execution)
	*/

	/* TODO:
		INI
			MEMPTR = BC_before_decrementing_B + 1
	*/

	/* TODO:
		IND
			MEMPTR = BC_before_decrementing_B - 1
	*/

	/* TODO:
		INIR
			exactly as INI on each execution.
			I.e. resulting MEMPTR = ((1 << 8) + C) + 1
	*/

	/* TODO:
		INDR
			exactly as IND on each execution.
			I.e. resulting MEMPTR = ((1 << 8) + C) - 1
	*/

	/* TODO:
		OUTI
			MEMPTR = BC_after_decrementing_B + 1
	*/

	/* TODO:
		OUTD
			MEMPTR = BC_after_decrementing_B - 1
	*/

	/* TODO:
		OTIR
			exactly as OUTI on each execution. I.e. resulting MEMPTR = C + 1
	*/

	/* TODO:
		OTDR
			exactly as OUTD on each execution. I.e. resulting MEMPTR = C - 1
	*/

	/* TODO:
		Any instruction with (INDEX+d):
			MEMPTR = INDEX+d
	*/

	/* TODO:
		Interrupt call to addr:
			As usual CALL. I.e. MEMPTR = addr
	*/
}
