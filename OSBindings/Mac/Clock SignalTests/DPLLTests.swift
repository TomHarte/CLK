//
//  DPLLTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 12/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest

class DPLLTests: XCTestCase {

	func testRegularNibblesOnPLL(pll: DigitalPhaseLockedLoopBridge, bitLength: UInt) {
		// clock in two 1s, a 0, and a 1, 200 times over
		for _ in 0 ..< 200 {
			pll.runForCycles(bitLength/2)
			pll.addPulse()
			pll.runForCycles(bitLength)
			pll.addPulse()
			pll.runForCycles(bitLength*2)
			pll.addPulse()
			pll.runForCycles(bitLength/2)
		}

		XCTAssert((pll.stream&0xffffff) == 0xdddddd, "PLL should have synchronised and clocked repeating 0xd nibbles; got \(String(pll.stream, radix: 16, uppercase: false))")
	}

	func testPerfectInput() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100, tolerance: 20, historyLength: 5)
		testRegularNibblesOnPLL(pll, bitLength: 100)
	}

	func testFastButRegular() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100, tolerance: 20, historyLength: 5)
		testRegularNibblesOnPLL(pll, bitLength: 90)
	}

	func testSlowButRegular() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100, tolerance: 20, historyLength: 5)
		testRegularNibblesOnPLL(pll, bitLength: 110)
	}
}
