//
//  BCDTest.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import Foundation
import XCTest

class BCDTest: XCTestCase, CSTestMachineTrapHandler {

	func testBCD(processor: CSTestMachine6502Processor) {
		if let filename = Bundle(for: type(of: self)).path(forResource: "BCDTEST_beeb", ofType: nil) {
			if let bcdTest = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				let machine = CSTestMachine6502(processor: processor)
				machine.trapHandler = self

				machine.setData(bcdTest, atAddress: 0x2900)

				// install a launchpad
				machine.setValue(0x20, forAddress:0x200)	// JSR 0x2900
				machine.setValue(0x00, forAddress:0x201)
				machine.setValue(0x29, forAddress:0x202)
				machine.setValue(0x4c, forAddress:0x203)	// JMP 0x0203
				machine.setValue(0x03, forAddress:0x204)
				machine.setValue(0x02, forAddress:0x205)

				machine.setValue(0x200, for: .programCounter)

				machine.setValue(0x60, forAddress:0xffee)
				machine.addTrapAddress(0xffee) // OSWRCH, an RTS

				while(machine.value(for: .programCounter) != 0x203) {
					machine.runForNumber(ofCycles: 1000)
				}
				XCTAssert(machine.value(forAddress:0x84) == 0, output)
			}
		}
	}

	func test6502BCD() {
		testBCD(processor: .processor6502)
	}

	func test65C02BCD() {
		testBCD(processor: .processor65C02)
	}

	private var output: String = ""
	func testMachine(_ testMachine: CSTestMachine, didTrapAtAddress address: UInt16) {
		let machine6502 = testMachine as! CSTestMachine6502

		// Only OSWRCH is trapped, so...
		let character = Character(UnicodeScalar(machine6502.value(for: .A))!)
		if character != "\r" {		// The test internally uses \r\n; keep only one of those.
			output.append(character)
		}
	}

}
