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
		if let filename = NSBundle(forClass: self.dynamicType).pathForResource("AllSuiteA", ofType: "bin") {
			if let allSuiteA = NSData(contentsOfFile: filename) {
				let machine = CSTestMachine()

				machine.setData(allSuiteA, atAddress: 0x4000)
				machine.setValue(CSTestMachineJamOpcode, forAddress:0x45c0);  // end

				machine.setValue(0x4000, forRegister: CSTestMachineRegister.ProgramCounter)
				while !machine.isJammed {
					machine.runForNumberOfCycles(1000)
				}

				if machine.valueForAddress(0x0210) != 0xff {
					NSException(name: "Failed AllSuiteA", reason: "Failed test \(machine.valueForAddress(0x0210))", userInfo: nil).raise()
				}
			}
		}
	}
}