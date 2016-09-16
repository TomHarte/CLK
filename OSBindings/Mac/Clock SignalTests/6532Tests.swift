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

	fileprivate func with6532(_ action: (MOS6532Bridge) -> ()) {
		let bridge = MOS6532Bridge()
		action(bridge)
	}

	// MARK: Timer tests
	func testOneTickTimer() {
		with6532 {
			// set a count of 128 at single-clock intervals
			$0.setValue(128, forRegister:0x14)

			// run for one clock and the count should now be 127
			$0.run(forCycles: 1)
			XCTAssert($0.value(forRegister: 4) == 127, "A single tick should decrease the counter once")

			// run for a further 200 clock counts; timer should reach -73 = 183
			$0.run(forCycles: 200)
			XCTAssert($0.value(forRegister: 4) == 183, "Timer should underflow and keep counting")
		}
	}

	// TODO: the tests below makes the assumption that divider phase is flexible; verify
	func testEightTickTimer() {
		with6532 {
			// set a count of 28 at eight-clock intervals
			$0.setValue(28, forRegister:0x15)

			// run for seven clock and the count should still be 28
			$0.run(forCycles: 7)
			XCTAssert($0.value(forRegister: 4) == 28, "The timer should remain unchanged for seven clocks")

			// run for a further clock and the count should now be 27
			$0.run(forCycles: 1)
			XCTAssert($0.value(forRegister: 4) == 27, "The timer should have decremented once after 8 cycles")

			// run for a further 7 + 27*8 + 5 = 228 clock counts; timer should reach -5 = 0xfb
			$0.run(forCycles: 228)
			XCTAssert($0.value(forRegister: 4) == 0xfb, "Timer should underflow and start counting at single-clock pace")

			// timer should now resume dividing by eight
			$0.run(forCycles: 7)
			XCTAssert($0.value(forRegister: 4) == 0xfb, "Timer should remain unchanged for seven cycles")

			// timer should now resume dividing by eight
			$0.run(forCycles: 1)
			XCTAssert($0.value(forRegister: 4) == 0xfa, "Timer should decrement after eighth cycle")
		}
	}

	func testTimerInterrupt() {
		with6532 {
			// set a count of 1 at single-clock intervals
			$0.setValue(1, forRegister:0x1c)

			// run for one clock and the count should now be zero
			$0.run(forCycles: 1)

			// interrupt shouldn't be signalled yet, bit should not be set
			XCTAssert(!$0.irqLine, "IRQ line should not be set")
			XCTAssert($0.value(forRegister: 5) == 0x00, "Counter interrupt should not be set")

			// run for one more clock
			$0.run(forCycles: 1)

			// the interrupt line and bit should now be set
			XCTAssert($0.irqLine, "IRQ line should be set")
			XCTAssert($0.value(forRegister: 5) == 0x80, "Counter interrupt should be set")

			// writing again to the timer should clear both
			$0.setValue(1, forRegister:0x1c)
			XCTAssert(!$0.irqLine, "IRQ line should be clear")
			XCTAssert($0.value(forRegister: 5) == 0x00, "Counter interrupt should not be set")
		}
	}


	// MARK: PA7 interrupt tests
	func testPA7InterruptDisabled() {
		with6532 {
			// disable edge detection
			$0.setValue(0, forRegister:4)

			// set output mode for port a
			$0.setValue(0xff, forRegister:1)

			// toggle bit 7 of port a in both directions
			$0.setValue(0x80, forRegister:0)
			$0.setValue(0x00, forRegister:0)
			$0.setValue(0x80, forRegister:0)

			// confirm that the interrupt flag is set but the line is not
			XCTAssert(!$0.irqLine, "IRQ line should not be set")
			XCTAssert($0.value(forRegister: 5) == 0x40, "Timer interrupt bit should be set")

			// reading the status register should have reset the interrupt flag
			XCTAssert($0.value(forRegister: 5) == 0x00, "Timer interrupt bit should be reset")
		}
	}

	func testPA7LeadingEdge() {
		with6532 {
			// seed port a is high; ensure interrupt bit is clear
			$0.setValue(0x00, forRegister:0)
			$0.value(forRegister: 5)

			// enable leading edge detection
			$0.setValue(0, forRegister:7)

			// set output mode for port a
			$0.setValue(0xff, forRegister:1)

			// toggle bit 7 of port a in a leading direction
			$0.setValue(0x80, forRegister:0)

			// confirm that both the interrupt flag are the line are set
			XCTAssert($0.irqLine, "IRQ line should be set")
			XCTAssert($0.value(forRegister: 5) == 0x40, "Timer interrupt bit should be set")
		}
	}

	func testPA7TrailingEdge() {
		with6532 {
			// seed port a is high; ensure interrupt bit is clear
			$0.setValue(0x80, forRegister:0)
			$0.value(forRegister: 5)

			// enable trailing edge detection
			$0.setValue(0, forRegister:6)

			// set output mode for port a
			$0.setValue(0xff, forRegister:1)

			// toggle bit 7 of port a in a rising direction
			$0.setValue(0x00, forRegister:0)

			// confirm that both the interrupt flag are the line are set
			XCTAssert($0.irqLine, "IRQ line should be set")
			XCTAssert($0.value(forRegister: 5) == 0x40, "Timer interrupt bit should be set")
		}
	}
}
