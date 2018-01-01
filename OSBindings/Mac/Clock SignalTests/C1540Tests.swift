//
//  C1540Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 09/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest

class C1540Tests: XCTestCase {

	fileprivate func with1540(_ action: (C1540Bridge) -> ()) {
		let bridge = C1540Bridge()
		action(bridge)
	}

	fileprivate func transmit(_ c1540: C1540Bridge, value: Int) {
		var shiftedValue = value

		c1540.dataLine = true
		c1540.run(forCycles: 256)
		XCTAssert(c1540.dataLine == false, "Listener should have taken data line low for start of transmission")

		c1540.clockLine = true
		c1540.run(forCycles: 256)	// this isn't time limited on real hardware
		XCTAssert(c1540.dataLine == true, "Listener should have let data line go high again")

		// set up for byte transfer
		c1540.clockLine = false
		c1540.dataLine = true
		c1540.run(forCycles: 40)

		// transmit bits
		for _ in 0..<8 {
			// load data line
			c1540.dataLine = (shiftedValue & 1) == 1
			shiftedValue >>= 1

			// toggle clock
			c1540.clockLine = true
			c1540.run(forCycles: 40)
			c1540.clockLine = false
			c1540.run(forCycles: 40)
		}

		// check for acknowledgment
		c1540.dataLine = true
		c1540.run(forCycles: 1000)
		XCTAssert(c1540.dataLine == false, "Listener should have acknowledged byte")
	}

	// MARK: EOI

	func testTransmission() {
		with1540 {
			// allow some booting time
			$0.run(forCycles: 2000000)

			// I want to be talker, so hold attention and clock low with data high
			$0.clockLine = false
			$0.attentionLine = false
			$0.dataLine = true

			// proceed 1 ms and check that the 1540 pulled the data line low
			$0.run(forCycles: 1000)
			XCTAssert($0.dataLine == false, "Listener should have taken data line low")

			// transmit LISTEN #8
			self.transmit($0, value: 0x28)
		}
	}
}
