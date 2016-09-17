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

	fileprivate func with6522(_ action: (MOS6522Bridge) -> ()) {
		let bridge = MOS6522Bridge()
		action(bridge)
	}

	// MARK: Timer tests

	func testTimerCount() {
		with6522 {
			// set timer 1 to a value of $000a
			$0.setValue(10, forRegister: 4)
			$0.setValue(0, forRegister: 5)

			// run for 5 cycles
			$0.run(forHalfCycles: 10)

			// check that the timer has gone down by 5
			XCTAssert($0.value(forRegister: 4) == 5, "Low order byte should be 5; was \($0.value(forRegister: 4))")
			XCTAssert($0.value(forRegister: 5) == 0, "High order byte should be 0; was \($0.value(forRegister: 5))")
		}
	}

	func testTimerLatches() {
		with6522 {
			// set timer 2 to $1020
			$0.setValue(0x10, forRegister: 8)
			$0.setValue(0x20, forRegister: 9)

			// change the low-byte latch
			$0.setValue(0x40, forRegister: 8)

			// chek that the new latched value hasn't been copied
			XCTAssert($0.value(forRegister: 8) == 0x10, "Low order byte should be 0x10; was \($0.value(forRegister: 8))")
			XCTAssert($0.value(forRegister: 9) == 0x20, "High order byte should be 0x20; was \($0.value(forRegister: 9))")

			// write the low-byte latch
			$0.setValue(0x50, forRegister: 9)

			// chek that the latched value has been copied
			XCTAssert($0.value(forRegister: 8) == 0x40, "Low order byte should be 0x50; was \($0.value(forRegister: 8))")
			XCTAssert($0.value(forRegister: 9) == 0x50, "High order byte should be 0x40; was \($0.value(forRegister: 9))")
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
			$0.run(forHalfCycles: 32)

			// check that the timer has gone down to 0 but not yet triggered an interrupt
			XCTAssert($0.value(forRegister: 4) == 0, "Low order byte should be 0; was \($0.value(forRegister: 4))")
			XCTAssert($0.value(forRegister: 5) == 0, "High order byte should be 0; was \($0.value(forRegister: 5))")
			XCTAssert(!$0.irqLine, "IRQ should not yet be active")

			// check that two half-cycles later the timer is $ffff but IRQ still hasn't triggered
			$0.run(forHalfCycles: 2)
			XCTAssert($0.value(forRegister: 4) == 0xff, "Low order byte should be 0xff; was \($0.value(forRegister: 4))")
			XCTAssert($0.value(forRegister: 5) == 0xff, "High order byte should be 0xff; was \($0.value(forRegister: 5))")
			XCTAssert(!$0.irqLine, "IRQ should not yet be active")

			// check that one half-cycle later the timer is still $ffff and IRQ has triggered...
			$0.run(forHalfCycles: 1)
			XCTAssert($0.irqLine, "IRQ should be active")
			XCTAssert($0.value(forRegister: 4) == 0xff, "Low order byte should be 0xff; was \($0.value(forRegister: 4))")
			XCTAssert($0.value(forRegister: 5) == 0xff, "High order byte should be 0xff; was \($0.value(forRegister: 5))")

			// ... but that reading the timer cleared the interrupt
			XCTAssert(!$0.irqLine, "IRQ should be active")

			// check that one half-cycles later the timer has reloaded
			$0.run(forHalfCycles: 1)
			XCTAssert($0.value(forRegister: 4) == 0x10, "Low order byte should be 0x10; was \($0.value(forRegister: 4))")
			XCTAssert($0.value(forRegister: 5) == 0x00, "High order byte should be 0x00; was \($0.value(forRegister: 5))")
		}
	}


	// MARK: Data direction tests
	func testDataDirection() {
		with6522 {
			// set low four bits of register B as output, the top four as input
			$0.setValue(0xf0, forRegister: 2)

			// ask to output 0x8c
			$0.setValue(0x8c, forRegister: 0)

			// set current input as 0xda
			$0.portBInput = 0xda

			// test that the result of reading register B is therefore 0x8a
			XCTAssert($0.value(forRegister: 0) == 0x8a, "Data direction register should mix input and output; got \($0.value(forRegister: 0))")
		}
	}
}
