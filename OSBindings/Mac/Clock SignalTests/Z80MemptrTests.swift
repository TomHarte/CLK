//
//  Z80MemptrTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MemptrTests: XCTestCase {
	private let machine = CSTestMachineZ80()

	private func test(program : [UInt8], length : Int32, initialValue : UInt16) -> UInt16 {
		// Create a machine and install the supplied program at address 0, setting the PC to run from there
		machine.setValue(0x0000, for: .programCounter)
		machine.setData(Data(bytes: program), atAddress: 0x0000)

		// Set the initial value of memptr, run for the requested number of cycles,
		// return the new value
		machine.setValue(initialValue, for: .memPtr)
		machine.runForNumber(ofCycles: length)
		return machine.value(for: .memPtr)
	}

	// LD A, (addr)
	func testLDAnn() {
		var program: [UInt8] = [
			0x3a, 0x00, 0x00
		]
		for addr in 0 ..< 65536 {
			program[1] = UInt8(addr & 0x00ff)
			program[2] = UInt8(addr >> 8)
			let expectedResult = UInt16((addr + 1) & 0xffff)

			let result = test(program: program, length: 13, initialValue: 0xffff)
			XCTAssertEqual(result, expectedResult)
		}
	}

	// LD A, (rp)
	func testLDArp() {
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

			let bcResult = test(program: bcProgram, length: 7, initialValue: 0xffff)
			let deResult = test(program: deProgram, length: 7, initialValue: 0xffff)

			XCTAssertEqual(bcResult, expectedResult)
			XCTAssertEqual(deResult, expectedResult)
		}
	}

	// LD (addr), rp
	func testLDnnrp() {
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

			XCTAssertEqual(test(program: hlBaseProgram, length: 16, initialValue: 0xffff), expectedResult)

			XCTAssertEqual(test(program: bcEDProgram, length: 20, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: deEDProgram, length: 20, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: hlEDProgram, length: 20, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: spEDProgram, length: 20, initialValue: 0xffff), expectedResult)

			XCTAssertEqual(test(program: ixProgram, length: 20, initialValue: 0xffff), expectedResult)
			XCTAssertEqual(test(program: iyProgram, length: 20, initialValue: 0xffff), expectedResult)
		}
	}
}
