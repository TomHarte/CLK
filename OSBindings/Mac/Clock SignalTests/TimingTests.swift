//
//  TimingTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 13/08/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

class TimingTests: XCTestCase, CSTestMachineJamHandler {

	private var endTime: UInt32 = 0

	func testImpliedNOP() {
		let code: [UInt8] = [0xea, CSTestMachineJamOpcode]
		self.runTest(code, expectedRunLength: 2)
	}

	func runTest(code: [UInt8], expectedRunLength: UInt32) {
		let machine = CSTestMachine()

		machine.jamHandler = self

		let immediateCode = NSData(bytes: code, length: code.count)
		machine.setData(immediateCode, atAddress: 0x200)
		machine.setValue(0x200, forRegister: CSTestMachineRegister.ProgramCounter)

		self.endTime = 0
		while self.endTime == 0 {
			machine.runForNumberOfCycles(10)
		}

		XCTAssert(self.endTime == expectedRunLength, "Took \(self.endTime) cycles to perform")
	}

	func testMachine(machine: CSTestMachine!, didJamAtAddress address: UInt16) {
		if self.endTime == 0 {
			self.endTime = machine.timestamp - 3
		}
	}
}
