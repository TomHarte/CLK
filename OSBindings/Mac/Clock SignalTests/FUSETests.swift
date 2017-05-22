//
//  FUSETests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 21/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class FUSETests: XCTestCase {

	func testFUSE() {
		if	let inputFilename = Bundle(for: type(of: self)).path(forResource: "tests", ofType: "in"),
			let outputFilename = Bundle(for: type(of: self)).path(forResource: "tests", ofType: "expected") {
			if	let input = try? String(contentsOf: URL(fileURLWithPath: inputFilename), encoding: .utf8),
				let output = try? String(contentsOf: URL(fileURLWithPath: outputFilename), encoding: .utf8) {

				let machine = CSTestMachineZ80()
//				machine.setData(testData, atAddress: 0x0100)

//				machine.setValue(0x0100, for: .programCounter)

//				machine.runForNumber(ofCycles: 20)
			}
		}
	}

}
