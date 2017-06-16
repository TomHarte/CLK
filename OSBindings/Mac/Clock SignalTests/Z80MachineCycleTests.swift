//
//  Z80MachineCycleTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 15/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MachineCycleTests: XCTestCase {

	private struct MachineCycle {
		var operation: CSTestMachineZ80BusOperationCaptureOperation
		var length: Int32
	}

	private func test(program : [UInt8], busCycles : [MachineCycle]) {
		// Create a machine and install the supplied program at address 0, setting the PC to run from there
		let machine = CSTestMachineZ80()
		machine.setValue(0x0000, for: .programCounter)
		machine.setData(Data(bytes: program), atAddress: 0x0000)

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
		for cycle in machine.busOperationCaptures {
			let length = cycle.timeStamp - totalCycles
			totalCycles += length

			XCTAssertEqual(length, busCycles[index].length)
			XCTAssertEqual(cycle.operation, busCycles[index].operation)

			index += 1
		}
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

	// LD r, nn
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
	func testLDAnn() {
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
	func testLDnnA() {
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
}
