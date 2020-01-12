//
//  DPLLTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 12/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import XCTest

class DPLLTests: XCTestCase {

	func testRegularNibblesOnPLL(_ pll: DigitalPhaseLockedLoopBridge, bitLength: UInt) {
		// clock in two 1s, a 0, and a 1, 200 times over
		for _ in 0 ..< 200 {
			pll.run(forCycles: bitLength/2)
			pll.addPulse()
			pll.run(forCycles: bitLength)
			pll.addPulse()
			pll.run(forCycles: bitLength*2)
			pll.addPulse()
			pll.run(forCycles: bitLength/2)
		}

		XCTAssert((pll.stream&0xffffffff) == 0xdddddddd, "PLL should have synchronised and clocked repeating 0xd nibbles; got \(String(pll.stream, radix: 16, uppercase: false))")
	}

	func testPerfectInput() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100)
		testRegularNibblesOnPLL(pll!, bitLength: 100)
	}

	func testFastButRegular() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100)
		testRegularNibblesOnPLL(pll!, bitLength: 90)
	}

	func testSlowButRegular() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100)
		testRegularNibblesOnPLL(pll!, bitLength: 110)
	}

	func testTwentyPercentSinePattern() {
		let pll = DigitalPhaseLockedLoopBridge(clocksPerBit: 100)
		var angle = 0.0

		// clock in two 1s, a 0, and a 1, 200 times over
		for _ in 0 ..< 200 {
			let bitLength: UInt = UInt(100 + 20 * sin(angle))

			pll?.run(forCycles: bitLength/2)
			pll?.addPulse()
			pll?.run(forCycles: (bitLength*3)/2)

			angle = angle + 0.1
		}

		let endOfStream = (pll?.stream)!&0xffffffff;
		XCTAssert(endOfStream == 0xaaaaaaaa || endOfStream == 0x55555555, "PLL should have synchronised and clocked repeating 0xa or 0x5 nibbles; got \(String(pll!.stream, radix: 16, uppercase: false))")
	}
}
