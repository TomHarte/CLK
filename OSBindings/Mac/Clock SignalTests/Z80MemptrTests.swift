//
//  Z80MemptrTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MemptrTests: XCTestCase {
	private func test(program : [UInt8], length : Int32, initialValue : UInt16) -> UInt16 {
		// Create a machine and install the supplied program at address 0, setting the PC to run from there
		let machine = CSTestMachineZ80()
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
		let program: [UInt8] = [
			0x3a, 0x00, 0x00
		]
		let result = test(program: program, length: 13, initialValue: 0xffff)
		XCTAssertEqual(result, 0x0001)
	}

}
