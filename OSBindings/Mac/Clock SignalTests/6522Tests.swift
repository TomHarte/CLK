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
			$0.setValue(10, forRegister: 4)
			$0.setValue(0, forRegister: 5)

			$0.runForHalfCycles(10)

			XCTAssert($0.valueForRegister(4) == 5, "Low order byte of timer should be 5; was \($0.valueForRegister(4))")
			XCTAssert($0.valueForRegister(5) == 0, "High order byte of timer should be 5; was \($0.valueForRegister(5))")
		}
	}
}
