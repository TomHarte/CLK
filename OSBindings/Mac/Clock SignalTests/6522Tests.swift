//
//  6522Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class MOS6522Tests: XCTestCase {

	private func with6522(action: (MOS6522Bridge) -> ()) {
		let bridge = MOS6522Bridge()
		action(bridge)
	}

	func testTimerCount() {
		with6522 {
			// set timer 1 to a value of $000a
			$0.setValue(10, forRegister: 4)
			$0.setValue(0, forRegister: 5)

			// run for 5 cycles
			$0.runForHalfCycles(10)

			// check that the timer has gone down by 5
			XCTAssert($0.valueForRegister(4) == 5, "Low order byte of timer should be 5; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0, "High order byte of timer should be 0; was \($0.valueForRegister(5))")
		}
	}

	func testTimerReload() {
		with6522 {
			// set timer 1 to a value of $0010, enable repeating mode
			$0.setValue(16, forRegister: 4)
			$0.setValue(0, forRegister: 5)
			$0.setValue(0x40, forRegister: 11)
			$0.setValue(0x40 | 0x80, forRegister: 14)

			// run for 16 cycles
			$0.runForHalfCycles(32)

			// check that the timer has gone down to 0 but not yet triggered an interrupt
			XCTAssert($0.valueForRegister(4) == 0, "Low order byte of timer should be 0; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0, "High order byte of timer should be 0; was \($0.valueForRegister(5))")
			XCTAssert(!$0.irqLine, "IRQ should not yet be active")

			// check that two half-cycles later the timer is $ffff but IRQ still hasn't triggered
			$0.runForHalfCycles(2)
			XCTAssert($0.valueForRegister(4) == 0xff, "Low order byte of timer should be 0xff; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0xff, "High order byte of timer should be 0xff; was \($0.valueForRegister(5))")
			XCTAssert(!$0.irqLine, "IRQ should not yet be active")

			// check that one half-cycle later the timer is still $ffff and IRQ has triggered
			$0.runForHalfCycles(1)
			XCTAssert($0.irqLine, "IRQ should be active")
			XCTAssert($0.valueForRegister(4) == 0xff, "Low order byte of timer should be 0xff; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0xff, "High order byte of timer should be 0xff; was \($0.valueForRegister(5))")

			// check that one half-cycles later the timer has reloaded
			$0.runForHalfCycles(1)
			XCTAssert($0.valueForRegister(4) == 0x10, "Low order byte of timer should be 0x10; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0x00, "High order byte of timer should be 0x00; was \($0.valueForRegister(5))")
		}
	}
}
