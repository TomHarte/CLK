//
//  ZexallTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class ZexallTests: XCTestCase {

	func testZexall() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "zexall", ofType: "com") {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				let machine = CSTestMachineZ80()
				machine.setData(testData, atAddress: 0x0100)

				machine.runForNumber(ofCycles: 20)
			}
		}
	}

}
