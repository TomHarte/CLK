//
//  6532Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 19/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class MOS6532Tests: XCTestCase {

	private func with6532(_ action: (MOS6532Bridge) -> ()) {
		let bridge = MOS6532Bridge()
		action(bridge)
	}

	// MARK: Timer tests
	func testOneTickTimer() {
		with6532 {
			// set a count of 128 at single-clock intervals
			$0.setValue(128, forRegister:0x14)
			XCTAssertEqual($0.value(forRegister: 4), 128)

			// run for one more clock and the count should now be 127
			$0.run(forCycles: 1)
			XCTAssertEqual($0.value(forRegister: 4), 127)

			// run for 127 clocks and the timer should be zero, but the timer flag will not yet be set
			$0.run(forCycles: 127)
			XCTAssertEqual($0.value(forRegister: 5) & 0x80, 0)
			XCTAssertEqual($0.value(forRegister: 4), 0)

			// after one more cycle the counter should be 255 and the timer flag will now be set
			$0.run(forCycles: 1)
			XCTAssertEqual($0.value(forRegister: 5) & 0x80, 0x80)
			XCTAssertEqual($0.value(forRegister: 4), 255)

			// run for a further 55 clock counts; timer should reach -200
			$0.run(forCycles: 55)
			XCTAssertEqual($0.value(forRegister: 4), 200)
		}
	}

	// TODO: the tests below makes the assumption that divider phase is flexible; verify
	func testEightTickTimer() {
		with6532 {
			// set a count of 28 at eight-clock intervals
			$0.setValue(28, forRegister:0x15)
			XCTAssertEqual($0.value(forRegister: 4), 28)

			// one further cycle and the timer should hit 27
			$0.run(forCycles: 1)
			XCTAssertEqual($0.value(forRegister: 4), 27)

			// run for seven clock and the count should still be 27
			$0.run(forCycles: 7)
			XCTAssertEqual($0.value(forRegister: 4), 27)

			// run for a further clock and the count should now be 26
			$0.run(forCycles: 1)
			XCTAssertEqual($0.value(forRegister: 4), 26)

			// run for another 26 * 8 = 208 cycles and the count should hit zero without setting the timer flag, and
			// stay there for seven more cycles
			$0.run(forCycles: 208)
			for _ in 0 ..< 8 {
				XCTAssertEqual($0.value(forRegister: 5) & 0x80, 0)
				XCTAssertEqual($0.value(forRegister: 4), 0)
				$0.run(forCycles: 1)
			}

			// run six more, and the timer should reach 249, with the interrupt flag set
			$0.run(forCycles: 6)
			XCTAssertEqual($0.value(forRegister: 5) & 0x80, 0x80)
			XCTAssertEqual($0.value(forRegister: 4), 249)

			// timer should now resume dividing by eight
			$0.run(forCycles: 7)
			XCTAssertEqual($0.value(forRegister: 4), 249)

			$0.run(forCycles: 1)
			XCTAssertEqual($0.value(forRegister: 4), 248)
		}
	}

	func testTimerInterrupt() {
		with6532 {
			// set a count of 1 at single-clock intervals
			$0.setValue(1, forRegister:0x1c)
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
