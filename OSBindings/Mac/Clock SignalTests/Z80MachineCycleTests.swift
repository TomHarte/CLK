//
//  Z80MachineCycleTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 15/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MachineCycleTests: XCTestCase {

	private struct MachineCycle: CustomDebugStringConvertible {
		var operation: CSTestMachineZ80BusOperationCaptureOperation
		var length: Int32

		public var debugDescription: String {
			get {
				var opName = ""
				switch operation {
					case .readOpcode:			opName = "ro"
					case .read:					opName = "r"
					case .write:				opName = "w"
					case .portRead:				opName = "i"
					case .portWrite:			opName = "o"
					case .internalOperation:	opName = "iop"
					default:					opName = "?"
				}
				return "\(opName) \(length)"
			}
		}

	}

	private func test(program : [UInt8], busCycles : [MachineCycle]) {
		// Create a machine and install the supplied program at address 0, setting the PC to run from there
		let machine = CSTestMachineZ80()
		machine.setValue(0x0000, for: .programCounter)
		machine.setData(Data(_: program), atAddress: 0x0000)

		// Figure out the total number of cycles implied by the bus cycles
		var totalCycles: Int32 = 0
		for cycle in busCycles {
			totalCycles += Int32(cycle.length)
		}

		// Run the machine, capturing bus activity
		machine.captureBusActivity = true
		machine.runForNumber(ofCycles: totalCycles)

		// Check the results
		totalCycles = 0
		var index = 0
		var matches = true
		for cycle in machine.busOperationCaptures {
			let length = cycle.timeStamp - totalCycles
			totalCycles += length

			if index >= busCycles.count {
				// this can't be reached without one of the asserts failing;
				// it's to prevent an unintended exeception via out-of-bounds
				// array access
				break
			} else {
				XCTAssert(length & Int32(1) == 0, "While performing \(machine.busOperationCaptures) Z80 ended on a half cycle")

				if length != busCycles[index].length*2 || cycle.operation != busCycles[index].operation {
					matches = false
					break;
				}
			}

			index += 1
		}

		XCTAssert(matches, "Z80 performed \(machine.busOperationCaptures); was expected to perform \(busCycles)")
	}

	// LD r, r
	func testLDrs() {
		test(
			program: [0x40],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4)
			]
		)
	}

	// LD r, n
	func testLDrn() {
		test(
			program: [0x3e, 0x00],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD r, (HL)
	func testLDrHL() {
		test(
			program: [0x46],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (HL), r
	func testLDHLr() {
		test(
			program: [0x70],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD r, (IX+d)
	func testLDrIXd() {
		test(
			program: [0xdd, 0x7e, 0x10],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (IX+d), r
	func testLDIXdr() {
		test(
			program: [0xdd, 0x70, 0x10],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD (HL), n
	func testLDHLn() {
		test(
			program: [0x36, 0x10],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD (IX+d), n
	func testLDIXdn() {
		test(
			program: [0xdd, 0x36, 0x10, 0x80],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 5),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD A, (DE)
	func testLDADE() {
		test(
			program: [0x1a],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (DE), A
	func testLDDEA() {
		test(
			program: [0x12],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD A, (nn)
	func testLDAinn() {
		test(
			program: [0x3a, 0x23, 0x45],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (nn), A
	func testLDinnA() {
		test(
			program: [0x32, 0x23, 0x45],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD A, I
	func testLDAI() {
		test(
			program: [0xed, 0x57],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
			]
		)
	}

	// LD I, A
	func testLDIA() {
		test(
			program: [0xed, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
			]
		)
	}

	// LD dd, nn
	func testLDddnn() {
		test(
			program: [0x01, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD IX, nn
	func testLDIXnn() {
		test(
			program: [0xdd, 0x21, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD HL, (nn)
	func testLDHLinn() {
		test(
			program: [0x2a, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (nn), HL
	func testLDinnHL() {
		test(
			program: [0x22, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD dd, (nn)
	func testLDddinn() {
		test(
			program: [0xed, 0x4b, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (nn), dd
	func testLDinndd() {
		test(
			program: [0xed, 0x43, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD IX, (nn)
	func testLDIXinn() {
		test(
			program: [0xdd, 0x2a, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// LD (nn), IX
	func testLDinnIX() {
		test(
			program: [0xdd, 0x22, 0x12, 0x47],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// LD SP, HL
	func testLDSPHL() {
		test(
			program: [0xf9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 6),
			]
		)
	}

	// LD SP, IX
	func testLDSPIX() {
		test(
			program: [0xdd, 0xf9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 6),
			]
		)
	}

	// PUSH qq
	func testPUSHqq() {
		test(
			program: [0xc5],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// PUSH IX
	func testPUSHIX() {
		test(
			program: [0xdd, 0xe5],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// POP qq
	func testPOPqq() {
		test(
			program: [0xe1],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// POP IX
	func testPOPIX() {
		test(
			program: [0xdd, 0xe1],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// EX DE, HL
	func testEXDEHL() {
		test(
			program: [0xeb],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// EX AF, AF'
	func testEXAFAFDd() {
		test(
			program: [0x08],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// EXX
	func testEXX() {
		test(
			program: [0xd9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// EX (SP), HL
	func testEXSPHL() {
		test(
			program: [0xe3],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 5),
			]
		)
	}

	// EX (SP), IX
	func testEXSPIX() {
		test(
			program: [0xdd, 0xe3],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 5),
			]
		)
	}

	// LDI
	func testLDI() {
		test(
			program: [0xed, 0xa0],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 5),
			]
		)
	}

	// CPI (NB: I've diverted from the documentation by assuming the five-cycle 'write' is an internal operation)
	func testCPI() {
		test(
			program: [0xed, 0xa1],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
			]
		)
	}

	// LDIR
	func testLDIR() {
		test(
			program: [
				0x01, 0x02, 0x00,	// LD BC, 2
				0xed, 0xb0,			// LDIR
				0x00, 0x00, 0x00	// NOP, NOP, NOP
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 5),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .write, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// CPIR (as per CPI; assumed no writes)
	func testCPIR() {
		test(
			program: [
				0x01, 0x02, 0x00,	// LD BC, 2
				0xed, 0xb1,			// CPIR
				0x00, 0x00, 0x00	// NOP, NOP, NOP
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// ADD A,r
	func testADDAr() {
		test(
			program: [0x80],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// ADD A,n
	func testADDAn() {
		test(
			program: [0xc6, 0x00],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// ADD A,(HL)
	func testADDAHL() {
		test(
			program: [0x86],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// ADD A,(IX+d)
	func testADDAIXd() {
		test(
			program: [0xdd, 0x86],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// INC r, DEC r
	func testINCrDECr() {
		test(
			program: [0x3c, 0x3d],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// INC (HL), DEC (HL)
	func testINCHLDECHL() {
		test(
			program: [0x34, 0x34],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// DAA, CPL, CCF, SCF, NOP, DI, EI, HALT
	func testDAACPLCCFSCFNOPDIEIHALT() {
		test(
			program: [0x27, 0x2f, 0x3f, 0x37, 0x00, 0xf3, 0xfb, 0x76],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),	// one more for luck
			]
		)
	}

	// NEG, IM 0, IM 1, IM 2
	func testNEGIMs() {
		test(
			program: [0xed, 0x44, 0xed, 0x46, 0xed, 0x56, 0xed, 0x5e],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// ADD HL, rr
	func testADDHL() {
		test(
			program: [0x09],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .internalOperation, length: 4),
				MachineCycle(operation: .internalOperation, length: 3),
			]
		)
	}

	// ADD IX, rr
	func testADDIX() {
		test(
			program: [0xdd, 0x09],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .internalOperation, length: 4),
				MachineCycle(operation: .internalOperation, length: 3),
			]
		)
	}

	// ADD A, (HL)
	func testADCHL() {
		test(
			program: [0xed, 0x4a],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .internalOperation, length: 4),
				MachineCycle(operation: .internalOperation, length: 3),
			]
		)
	}

	// INC rr
	func testINCss() {
		test(
			program: [0x03],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 6),
			]
		)
	}

	// INC IX
	func testINCIX() {
		test(
			program: [0xdd, 0x23],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 6),
			]
		)
	}

	// RLCA
	func testRLCA() {
		test(
			program: [0x07],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RLC r
	func testRLCr() {
		test(
			program: [0xcb, 0x00],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RLC (HL)
	func testRLCHL() {
		test(
			program: [0xcb, 0x06],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// RLC (IX+d) (NB: the official table doesn't read the final part of the instruction; I've assumed a five-cycle version of read opcode replaces the internal operation)
	func testRLCIX() {
		test(
			program: [0xdd, 0xcb, 0x00, 0x06],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// RLD
	func testRLD() {
		test(
			program: [0xed, 0x6f],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// BIT n,r
	func testBITr() {
		test(
			program: [0xcb, 0x40],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// SET n,r
	func testSETr() {
		test(
			program: [0xcb, 0xc0],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// BIT n,(HL)
	func testBITHL() {
		test(
			program: [0xcb, 0x46],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 4),
			]
		)
	}

	// SET n,(HL)
	func testSETHL() {
		test(
			program: [0xcb, 0xc6],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// BIT n,(IX+d)
	func testBITIX() {
		test(
			program: [0xdd, 0xcb, 0x00, 0x46],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 4),
			]
		)
	}

	// SET n,(IX+d)
	func testSETIX() {
		test(
			program: [0xdd, 0xcb, 0x00, 0xc6],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
			]
		)
	}

	// JP nn
	func testJPnn() {
		test(
			program: [0xc3, 0x00, 0x80],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// JP cc, nn
	func testJPccnn() {
		test(
			program: [
				0x37,				// SCF
				0xd2, 0x00, 0x80,	// JP NC, 0x8000
				0xda, 0x00, 0x80,	// JP C, 0x8000
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),
			]
		)
	}

	// JR e
	func testJRe() {
		test(
			program: [0x18, 0x80],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// JR cc, e
	func testJRcce() {
		test(
			program: [
				0x37,			// SCF
				0x30, 0x80,		// JR NC, 0x8000
				0x38, 0x80,		// JR C, 0x8000
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// JP (HL)
	func testJPHL() {
		test(
			program: [0xe9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// JP (IX)
	func testJPIX() {
		test(
			program: [0xdd, 0xe9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// DJNZ
	func testDJNZ() {
		test(
			program: [
				0x06, 0x02,			// LD B, 2
				0x10, 0xfe,			// DJNZ -2
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// CALL
	func testCALL() {
		test(
			program: [0xcd, 0x00, 0x80],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// CALL cc
	func testCALLcc() {
		test(
			program: [
				0x37,					// SCF
				0xd4, 0x00, 0x80,		// CALL NC
				0xdc, 0x00, 0x80		// CALL C
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 4),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RET
	func testRET() {
		test(
			program: [0xc9],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RET cc
	func testRETcc() {
		test(
			program: [
				0x37,		// SCF
				0xd0,		// RET NC
				0xd8,		// RET C
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),

				MachineCycle(operation: .readOpcode, length: 5),

				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RETI
	func testRETI() {
		test(
			program: [0xed, 0x4d],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// RST
	func testRST() {
		test(
			program: [0xc7],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// IN A, (n)
	func testINAn() {
		test(
			program: [0xdb, 0xfe],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .portRead, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// IN r, (C)
	func testINrC() {
		test(
			program: [0xed, 0x40],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .portRead, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// INI
	func testINI() {
		test(
			program: [0xed, 0xa2],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .portRead, length: 4),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// INIR
	func testINIR() {
		test(
			program: [
				0x01, 0x02, 0x00,	// LD BC, 2
				0xed, 0xb2,			// INIR
				0x00, 0x00, 0x00	// NOP, NOP, NOP
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .portRead, length: 4),
				MachineCycle(operation: .write, length: 3),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .portRead, length: 4),
				MachineCycle(operation: .write, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// OUT (n), A
	func testOUTnA() {
		test(
			program: [0xd3, 0xfe],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .portWrite, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// OUT (C), r
	func testOUTCr() {
		test(
			program: [0xed, 0x41],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .portWrite, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// OUTI
	func testOUTI() {
		test(
			program: [0xed, 0xa3],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .portWrite, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

	// OTIR
	func testOTIR() {
		test(
			program: [
				0x01, 0x02, 0x00,	// LD BC, 2
				0xed, 0xb3,			// OTIR
				0x00, 0x00, 0x00	// NOP, NOP, NOP
			],
			busCycles: [
				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .read, length: 3),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .portWrite, length: 4),
				MachineCycle(operation: .internalOperation, length: 5),

				MachineCycle(operation: .readOpcode, length: 4),
				MachineCycle(operation: .readOpcode, length: 5),
				MachineCycle(operation: .read, length: 3),
				MachineCycle(operation: .portWrite, length: 4),

				MachineCycle(operation: .readOpcode, length: 4),
			]
		)
	}

}
