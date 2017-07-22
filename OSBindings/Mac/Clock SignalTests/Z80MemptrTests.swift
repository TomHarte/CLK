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

	// LD A,(addr)
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

}
