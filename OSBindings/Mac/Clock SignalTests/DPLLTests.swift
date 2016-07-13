//
//  DPLLTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 12/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest

class DPLLTests: XCTestCase {

	func testPerfectInput() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100, tolerance: 20, historyLength: 5)

		// clock in two 1s, a 0, and a 1
		pll.runForCycles(50)
		pll.addPulse()
		pll.runForCycles(100)
		pll.addPulse()
		pll.runForCycles(200)
		pll.addPulse()
		pll.runForCycles(50)

		XCTAssert(pll.stream == 0xd, "PLL should have clocked four bits")
	}

}
