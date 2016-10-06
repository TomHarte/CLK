//
//  Clock_SignalTests.swift
//  Clock SignalTests
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import XCTest
@testable import Clock_Signal

class AllSuiteATests: XCTestCase {

	func testAllSuiteA() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "AllSuiteA", ofType: "bin") {
			if let allSuiteA = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine()

				machine.setData(allSuiteA, atAddress: 0x4000)
				machine.setValue(CSTestMachineJamOpcode, forAddress:0x45c0);  // end

				machine.setValue(0x4000, for: CSTestMachineRegister.programCounter)
				while !machine.isJammed {
					machine.runForNumber(ofCycles: 1000)
				}

				if machine.value(forAddress: 0x0210) != 0xff {
					NSException(name: NSExceptionName(rawValue: "Failed AllSuiteA"), reason: "Failed test \(machine.value(forAddress: 0x0210))", userInfo: nil).raise()
				}
			}
		}
	}
}
