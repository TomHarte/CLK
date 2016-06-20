//
//  6532Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class MOS6532Tests: XCTestCase {

	private func with6532(action: (MOS6532Bridge) -> ()) {
		let bridge = MOS6532Bridge()
		action(bridge)
	}

	// MARK: Timer tests
	func testOneTickTimer() {
		with6532 {
			// set a count of 128 at single-clock intervals
			$0.setValue(128, forRegister:4)

			// run for one clock and the count should now be 127
			$0.runForCycles(1)
			XCTAssert($0.valueForRegister(4) == 127, "A single tick should decrease the counter once")

			// run for a further 200 clock counts; timer should reach -73 = 183
			$0.runForCycles(200)
			XCTAssert($0.valueForRegister(4) == 183, "Timer should underflow and keep counting")
		}
	}

	func testEightTickTimer() {
		with6532 {
			// set a count of 28 at eight-clock intervals
			$0.setValue(28, forRegister:5)

			// run for seven clock and the count should still be 28
			$0.runForCycles(7)
			XCTAssert($0.valueForRegister(4) == 28, "The timer should remain unchanged for seven clocks")

			// run for a further clock and the count should now be 27
			$0.runForCycles(1)
			XCTAssert($0.valueForRegister(4) == 27, "The timer should have decremented once after 8 cycles")

			// run for a further 7 + 27*8 + 5 = 228 clock counts; timer should reach -5 = 0xfb
			$0.runForCycles(228)
			XCTAssert($0.valueForRegister(4) == 0xfb, "Timer should underflow and start counting at single-clock pace")

			// timer should now resume dividing by eight
			$0.runForCycles(7)
			XCTAssert($0.valueForRegister(4) == 0xfb, "Timer should remain unchanged for seven cycles")

			// timer should now resume dividing by eight
			$0.runForCycles(1)
			XCTAssert($0.valueForRegister(4) == 0xfa, "Timer should decrement after eighth cycle")
		}
	}
}
