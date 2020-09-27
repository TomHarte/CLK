//
//  Clock_SignalTests.swift
//  Clock SignalTests
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import XCTest

class AllSuiteATests: XCTestCase {

	func testAllSuiteA() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "AllSuiteA", ofType: "bin") {
			if let allSuiteA = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(processor: .processor6502)

				machine.setData(allSuiteA, atAddress: 0x4000)
				machine.setValue(CSTestMachine6502JamOpcode, forAddress:0x45c0);  // end

				machine.setValue(0x4000, for: CSTestMachine6502Register.programCounter)
				while !machine.isJammed {
					machine.runForNumber(ofCycles: 1000)
				}

				XCTAssert(machine.value(forAddress: 0x0210) == 0xff, "Failed test \(machine.value(forAddress: 0x0210))")
			}
		}
	}
}
