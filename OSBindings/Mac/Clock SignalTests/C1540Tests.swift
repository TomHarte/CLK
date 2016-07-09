//
//  C1540Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 09/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import XCTest

class C1540Tests: XCTestCase {

	private func with1540(action: (C1540Bridge) -> ()) {
		let bridge = C1540Bridge()

		if let path = NSBundle.mainBundle().pathForResource("1541", ofType: "bin", inDirectory: "ROMImages/Commodore1540") {
			let data = NSData(contentsOfFile: path)
			bridge.setROM(data)
		}

		action(bridge)
	}

	private func transmit(c1540: C1540Bridge, value: Int) {
		var shiftedValue = value
		for _ in 1..<8 {
			// load data line
			c1540.dataLine = (shiftedValue & 1) == 1
			shiftedValue >>= 1

			// toggle clock
			c1540.clockLine = false
			c1540.runForCycles(40)
			c1540.clockLine = true
			c1540.runForCycles(40)

			// wait up to 70 µs for an acknowledge
			var cyclesWaited = 0
			while c1540.clockLine == true && cyclesWaited < 140 {
				c1540.runForCycles(1)
				cyclesWaited = cyclesWaited + 1
			}
			XCTAssert(c1540.clockLine == false, "Listener should have started to acknowledge bit")
			while c1540.clockLine == false && cyclesWaited < 140 {
				c1540.runForCycles(1)
				cyclesWaited = cyclesWaited + 1
			}
			XCTAssert(c1540.clockLine == true, "Listener should have completed acknowledging bit")
		}
	}

	// MARK: EOI

	func testTransmission() {
		with1540 {
			// allow some booting time
			$0.runForCycles(2000000)

			// I want to be talker, so hold attention and clock low with data high
			$0.clockLine = false
			$0.attentionLine = false
			$0.dataLine = true

			// proceed 1 ms and check that the 1540 pulled the data line low
			$0.runForCycles(2000)
			XCTAssert($0.dataLine == false, "Listener should have taken data line low")

			// transmit LISTEN #8
			self.transmit($0, value: 0x28)
		}
	}
}
