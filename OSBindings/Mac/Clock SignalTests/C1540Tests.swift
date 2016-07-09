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

	// MARK: EOI

	func testTransmission() {
		with1540 {
			// allow some booting time
			$0.runForCycles(2000000)

			// I want to be talker, so hold attention and clock low
			$0.clockLine = false
			$0.attentionLine = false
			$0.runForCycles(200)

			XCTAssert($0.dataLine == false, "Listener should have taken data line low")
		}
	}
}
