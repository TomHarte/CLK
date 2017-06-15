//
//  Z80MachineCycleTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 15/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest

class Z80MachineCycleTests: XCTestCase {

	struct MachineCycle {
		var operation: CSTestMachineZ80BusOperationCaptureOperation
		var length: Int32
	}

	func test(program : Array<UInt8>, busCycles : [MachineCycle]) {
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

	func testLDrs() {
		test(
			program: [0x40],
			busCycles: [
				MachineCycle(operation: .read, length: 4)
			]
		)
	}

}
